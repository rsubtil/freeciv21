/*__            ___                 ***************************************
/   \          /   \          Copyright (c) 1996-2020 Freeciv21 and Freeciv
\_   \        /  __/          contributors. This file is part of Freeciv21.
 _\   \      /  /__     Freeciv21 is free software: you can redistribute it
 \___  \____/   __/    and/or modify it under the terms of the GNU  General
     \_       _/          Public License  as published by the Free Software
       | @ @  \_               Foundation, either version 3 of the  License,
       |                              or (at your option) any later version.
     _/     /\                  You should have received  a copy of the GNU
    /o)  (o/\ \_                General Public License along with Freeciv21.
    \_____/ /                     If not, see https://www.gnu.org/licenses/.
      \____/        ********************************************************/

// utility
#include "fcintl.h"
#include "log.h"
#include "rand.h"
#include "support.h"

// common
#include "actions.h"
#include "ai.h"
#include "city.h"
#include "combat.h"
#include "events.h"
#include "featured_text.h"
#include "game.h"
#include "log.h"
#include "map.h"
#include "movement.h"
#include "packets.h"
#include "player.h"
#include "research.h"
#include "specialist.h"
#include "traderoutes.h"
#include "unit.h"
#include "unitlist.h"
#include "worklist.h"

/* common/aicore */
#include "cm.h"

// server
#include "actiontools.h"
#include "citizenshand.h"
#include "citytools.h"
#include "cityturn.h"
#include "diplomats.h"
#include "government.h"
#include "govhand.h"
#include "maphand.h"
#include "notify.h"
#include "plrhand.h"
#include "sanitycheck.h"
#include "spacerace.h"
#include "techtools.h"
#include "unithand.h"
#include "unittools.h"

std::map<struct unit*, struct tile*> spy_last_sabotages;

bool spy_sabotaged_tile_recently(struct unit* punit, struct tile* tile)
{
  if(spy_last_sabotages.find(punit) == spy_last_sabotages.end()) {
    return false;
  }
  return spy_last_sabotages[punit] == tile;
}

void spy_set_recent_sabotaged_tile(struct unit* punit, struct tile* tile)
{
  spy_last_sabotages[punit] = tile;
}

void update_government_info()
{
  struct packet_government_info info;
  info.last_message_id = g_info.last_message_id;
  info.last_audit_id = g_info.last_audit_id;
  for (int i = 0; i < MAX_AUDIT_NUM; i++) {
    info.curr_audits[i] = g_info.curr_audits[i];
  }

  lsend_packet_government_info(game.est_connections, &info);
}

void handle_government_info_req(struct player *pplayer)
{
  struct packet_government_info info;
  info.last_message_id = g_info.last_message_id;
  info.last_audit_id = g_info.last_audit_id;
  for(int i = 0; i < MAX_AUDIT_NUM; i++) {
    info.curr_audits[i] = g_info.curr_audits[i];
  }

  send_packet_government_info(pplayer->current_conn, &info);
}

void handle_government_news_req(struct player *pplayer, int id)
{
  struct government_news *news = g_info.find_cached_news(id);
  if (news) {
    struct packet_government_news pkt;
    pkt.id = id;
    pkt.timestamp = news->timestamp;
    strcpy(pkt.news, news->news.toUtf8().data());

    send_packet_government_news(pplayer->current_conn, &pkt);
  }
}

void handle_government_audit_info_req(struct player *pplayer, int id)
{
  struct government_audit_info *audit = g_info.find_cached_audit(id);
  if (audit) {
    struct packet_government_audit_info pkt;
    pkt.id = id;
    pkt.accuser_id = audit->accuser_id;
    pkt.accused_id = audit->accused_id;
    pkt.jury_1_vote = audit->jury_1_vote;
    pkt.jury_2_vote = audit->jury_2_vote;
    pkt.consequence = audit->consequence;
    pkt.timestamp = audit->timestamp;

    send_packet_government_audit_info(pplayer->current_conn, &pkt);
  }
}

void handle_government_audit_submit_vote(struct player *pplayer, int id, int vote)
{
  // TODO
}

void handle_government_audit_start(struct player *pplayer, int sabotage_id, int accused_id)
{
  struct sabotage_info* sabotage = s_info.find_cached_sabotage(sabotage_id);
  if(!sabotage) return;
  struct government_audit_info *audit = g_info.new_government_audit();
  audit->accuser_id = player_id_from_string(pplayer->name);
  audit->accused_id = player_id(accused_id);
  audit->jury_1_vote = AUDIT_VOTE_NONE;
  audit->jury_2_vote = AUDIT_VOTE_NONE;
  // TODO: Get constants here
  audit->consequence = 0;//AUDIT_CONSEQUENCE_NONE;

  for(int i = 0; i < MAX_AUDIT_NUM; i++) {
    if(g_info.curr_audits[i] == -1) {
      g_info.curr_audits[i] = audit->id;
      break;
    }
  }

  // Notify all players
  update_government_info();
}

void handle_sabotage_city_req(struct player *pplayer, int actor_id, int tile_id)
{
  struct unit* punit = game_unit_by_number(actor_id);
  struct tile* ptile = unit_tile(punit);
  struct city* pcity = tile_city(ptile);

  struct act_prob probabilities[MAX_NUM_ACTIONS];
  int actor_target_distance;
  const struct player_tile *plrtile;

  // A target should only be sent if it is possible to act against it
  int target_city_id = IDENTITY_NUMBER_ZERO;

  // Initialize the action probabilities.
  action_iterate(act) { probabilities[act] = ACTPROB_NA; }
  action_iterate_end;

  // Check if the request is valid.
  if (!ptile || !punit || !pplayer || punit->owner != pplayer) {
    dsend_packet_sabotage_actions(
        pplayer->current_conn, actor_id, IDENTITY_NUMBER_ZERO, IDENTITY_NUMBER_ZERO,
        tile_id, IDENTITY_NUMBER_ZERO, probabilities);
    return;
  }

  // If city was sabotaged recently, don't allow another sabotage
  if(spy_sabotaged_tile_recently(punit, ptile)) {
    spy_send_error(pplayer, "You can't sabotage the same city twice in a row! Move your spy to another place.");
    return;
  }

  /* The player may have outdated information about the target tile.
   * Limiting the player knowledge look up to the target tile is OK since
   * all targets must be located at it. */
  plrtile = map_get_player_tile(ptile, pplayer);

  // Distance between actor and target tile.
  actor_target_distance = real_map_distance(unit_tile(punit), ptile);

  // Set the probability for the actions.
  action_iterate_range(act, ACTION_SABOTAGE_CITY_INVESTIGATE_GOLD,
                       ACTION_SABOTAGE_CITY_STEAL_MATERIALS)
  {
    if (plrtile && plrtile->site) {
      // Only a known city may be targeted.
      if (pcity) {
        // Calculate the probabilities.
        probabilities[act] = action_prob_vs_city(punit, act, pcity);
        target_city_id = plrtile->site->identity;
      } else if (!tile_is_seen(ptile, pplayer)
                  && action_maybe_possible_actor_unit(act, punit)
                  && action_id_distance_accepted(act,
                                                actor_target_distance)) {
        /* The target city is non existing. The player isn't aware of this
          * fact because he can't see the tile it was located on. The
          * actor unit it self doesn't contradict the requirements to
          * perform the action. The (no longer existing) target city was
          * known to be close enough. */
        probabilities[act] = ACTPROB_NOT_KNOWN;
      } else {
        /* The actor unit is known to be unable to act or the target city
          * is known to be too far away. */
        probabilities[act] = ACTPROB_IMPOSSIBLE;
      }
    } else {
      // No target to act against.
      probabilities[act] = ACTPROB_IMPOSSIBLE;
    }
  }
  action_iterate_end;

  // Send possible actions and targets.
  dsend_packet_sabotage_actions(pplayer->current_conn, actor_id, IDENTITY_NUMBER_ZERO,
                            target_city_id, tile_id, IDENTITY_NUMBER_ZERO,
                            probabilities);
}

void handle_sabotage_building_req(struct player *pplayer, int actor_id, int tile_id)
{
  // City-only for now
  struct unit *punit = game_unit_by_number(actor_id);
  struct tile *ptile = unit_tile(punit);
  struct building *pbuilding = map_buildings_get(ptile);

  if(!pbuilding) return;

  struct act_prob probabilities[MAX_NUM_ACTIONS];

  // A target should only be sent if it is possible to act against it
  int target_extra_id = IDENTITY_NUMBER_ZERO;

  // Initialize the action probabilities.
  action_iterate(act) { probabilities[act] = ACTPROB_NA; }
  action_iterate_end;

  // Check if the request is valid.
  if (!ptile || !punit || !pplayer || !pbuilding || punit->owner != pplayer) {
    dsend_packet_sabotage_actions(
        pplayer->current_conn, actor_id, IDENTITY_NUMBER_ZERO,
        IDENTITY_NUMBER_ZERO, tile_id, IDENTITY_NUMBER_ZERO, probabilities);
    return;
  }

  // If building belongs to player, it makes no sense to auto-sabotage
  if(building_belongs_to(pbuilding, pplayer)) {
    spy_send_error(pplayer, "You can't sabotage your own buildings!");
    return;
  }

  // If building was sabotaged recently, don't allow another sabotage
  if(spy_sabotaged_tile_recently(punit, ptile)) {
    spy_send_error(pplayer, "You can't sabotage the same building twice in a row! Move your spy to another place.");
    return;
  }

  bool anyset = false;
  // Set the probability for the actions.
  action_iterate_range(act, ACTION_SABOTAGE_BUILDING_INVESTIGATE_GOLD,
                       ACTION_SABOTAGE_BUILDING_STEAL_MATERIALS)
  {
    bool set = false;
    extra_type_by_cause_iterate(EC_BUILDING, pextra)
    {
      if (!strcmp(rule_name_get(&pextra->name), building_rulename_get(pbuilding))
       && !building_belongs_to(pbuilding, pplayer)
       && building_owner(pbuilding) != nullptr
       && (
        (
          (act == ACTION_SABOTAGE_BUILDING_INVESTIGATE_GOLD && building_rulename_get(pbuilding)[11] == 'b') ||
          (act == ACTION_SABOTAGE_BUILDING_STEAL_GOLD && building_rulename_get(pbuilding)[11] == 'b')
        ) ||
        (
          (act == ACTION_SABOTAGE_BUILDING_INVESTIGATE_SCIENCE && building_rulename_get(pbuilding)[11] == 'u') ||
          (act == ACTION_SABOTAGE_BUILDING_STEAL_SCIENCE && building_rulename_get(pbuilding)[11] == 'u')
        ) ||
        (
          (act == ACTION_SABOTAGE_BUILDING_INVESTIGATE_MATERIALS && building_rulename_get(pbuilding)[11] == 'f') ||
          (act == ACTION_SABOTAGE_BUILDING_STEAL_MATERIALS && building_rulename_get(pbuilding)[11] == 'f')
        )
      )) {
        // Calculate the probabilities.
        probabilities[act] = action_prob_vs_tile(punit, act, ptile, pextra);
        target_extra_id = pextra->id;
        set = true;
        anyset = true;
        break;
      } else {
        probabilities[act] = ACTPROB_IMPOSSIBLE;
      }
    }
    extra_type_by_cause_iterate_end;

    if(!set) {
      // No target to act against.
      probabilities[act] = ACTPROB_IMPOSSIBLE;
    }
  }
  action_iterate_end;

  // Send possible actions and targets.
  if(anyset) {
    dsend_packet_sabotage_actions(
        pplayer->current_conn, actor_id, IDENTITY_NUMBER_ZERO, IDENTITY_NUMBER_ZERO,
        tile_id, target_extra_id, probabilities);
  } else {
    if(!strcmp(building_rulename_get(pbuilding), "building_u")) {
      spy_send_error(pplayer,
                     "You can't sabotage an unoccupied building!");
    } else {
      spy_send_error(pplayer, "There are no valid sabotages you can perform here!");
    }
  }
}

void handle_sabotage_transport_req(struct player *pplayer, int actor_id,
                                  int tile_id)
{
  // City-only for now
  struct unit *punit = game_unit_by_number(actor_id);
  struct tile *ptile = unit_tile(punit);
  fc_assert(ptile->label);
  fc_assert(map_transports_get(QString(ptile->label)));

  struct act_prob probabilities[MAX_NUM_ACTIONS];

  // A target should only be sent if it is possible to act against it
  int target_extra_id = IDENTITY_NUMBER_ZERO;

  // Initialize the action probabilities.
  action_iterate(act) { probabilities[act] = ACTPROB_NA; }
  action_iterate_end;

  // Check if the request is valid.
  if (!ptile || !punit || !pplayer
      || punit->owner != pplayer) {
    dsend_packet_sabotage_actions(
        pplayer->current_conn, actor_id, IDENTITY_NUMBER_ZERO,
        IDENTITY_NUMBER_ZERO, tile_id, IDENTITY_NUMBER_ZERO, probabilities);
    return;
  }

  bool anyset = false;
  // Set the probability for the actions.
  action_iterate_range(act, ACTION_WIRETAP, ACTION_TRANSPORT_REPORT)
  {
    bool set = false;
    extra_type_by_cause_iterate(EC_TRANSPORT, pextra)
    {
      if (tile_has_extra(ptile, pextra)) {
        // Calculate the probabilities.
        probabilities[act] = action_prob_vs_tile(punit, act, ptile, pextra);
        target_extra_id = pextra->id;
        set = true;
        anyset = true;
        break;
      } else {
        probabilities[act] = ACTPROB_IMPOSSIBLE;
      }
    }
    extra_type_by_cause_iterate_end;

    if (!set) {
      // No target to act against.
      probabilities[act] = ACTPROB_IMPOSSIBLE;
    }
  }
  action_iterate_end;

  // Send possible actions and targets.
  if (anyset) {
    dsend_packet_sabotage_actions(pplayer->current_conn, actor_id,
                                  IDENTITY_NUMBER_ZERO, IDENTITY_NUMBER_ZERO,
                                  tile_id, target_extra_id, probabilities);
  }
}

void handle_sabotage_info_req(struct player *pplayer)
{
  struct packet_sabotage_info info;
  info.last_sabotage_self_id = pplayer->server.last_sabotage_self_id;
  info.last_sabotage_other_id = pplayer->server.last_sabotage_other_id;

  send_packet_sabotage_info(pplayer->current_conn, &info);
}

void handle_sabotage_info_self_req(struct player *pplayer, int id)
{
  struct sabotage_info *info = s_info.find_cached_sabotage(id);
  if(info && info->player_tgt == pplayer && info->player_send_to == pplayer) {
    struct packet_sabotage_info_self pkt;
    pkt.id = id;
    pkt.actionable = info->actionable;
    pkt.timestamp = info->timestamp;
    // For safety, don't send the src player
    pkt.player_src = -1;
    pkt.player_tgt = player_number(info->player_tgt);
    strcpy(pkt.info, info->info.c_str());

    send_packet_sabotage_info_self(pplayer->current_conn, &pkt);
  }
}

void handle_sabotage_info_other_req(struct player *pplayer, int id)
{
  struct sabotage_info *info = s_info.find_cached_sabotage(id);
  if(info && info->player_src == pplayer && info->player_send_to == pplayer) {
    struct packet_sabotage_info_other pkt;
    pkt.id = id;
    pkt.actionable = info->actionable;
    pkt.timestamp = info->timestamp;
    pkt.player_src = player_number(info->player_src);
    pkt.player_tgt = player_number(info->player_tgt);
    strcpy(pkt.info, info->info.c_str());

    send_packet_sabotage_info_other(pplayer->current_conn, (const struct packet_sabotage_info_other* ) &pkt);
  }
}

void spy_send_error(struct player *pplayer, const char *msg)
{
  if (!pplayer)
    return;

  dsend_packet_sabotage_error(pplayer->current_conn, msg);
}
