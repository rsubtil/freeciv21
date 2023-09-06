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

void finish_audit(struct government_audit_info* info)
{
  int curr_audit_idx = -1;
  for (int i = 0; i < MAX_AUDIT_NUM; i++) {
    if (info->id == g_info.curr_audits[i]) {
      curr_audit_idx = i;
      break;
    }
  }
  if (curr_audit_idx == -1) {
    log_warning("The specified audit is not active.");
    return;
  }

  struct sabotage_info *sabotage_info =
      s_info.find_cached_sabotage(info->sabotage_id);
  struct player *paccuser = get_player_from_id(info->accuser_id);
  struct player *paccused = get_player_from_id(info->accused_id);
  struct player *pjury1 = get_player_from_id(
      determine_jury_id(info->accuser_id, info->accused_id, 1));
  struct player *pjury2 = get_player_from_id(
      determine_jury_id(info->accuser_id, info->accused_id, 2));
  if (!paccuser || !paccused || !pjury1 || !pjury2) {
    log_error("One or more players was null! This is wrong!");
    return;
  }

  // Find final decision
  int votes_guilty = 0;
  int votes_innocent = 0;

  switch (info->jury_1_vote) {
  case AUDIT_VOTE_YES:
    votes_guilty++;
    break;
  case AUDIT_VOTE_NO:
    votes_innocent++;
    break;
  default:
    break;
  }
  switch (info->jury_2_vote) {
  case AUDIT_VOTE_YES:
    votes_guilty++;
    break;
  case AUDIT_VOTE_NO:
    votes_innocent++;
    break;
  default:
    break;
  }

  struct government_news *news = g_info.new_government_news();

  if (votes_guilty > votes_innocent) {
    // Accused is guilty!
    bool was_decision_right = sabotage_info->player_src == paccused;
    log_warning("Accused is guilty! Decision was right? %s",
                was_decision_right ? "yes" : "no");
    switch (info->consequence) {
    case CONSEQUENCE_GOLD: {
      int take = int(ceil(paccused->economic.gold * 0.3f));
      paccused->economic.gold -= take;
      log_warning("Accused: %d -> %d (%d)", paccused->economic.gold + take,
                  paccused->economic.gold, take);
      paccuser->economic.gold += take;
      log_warning("Accuser: %d -> %d (%d)", paccuser->economic.gold - take,
                  paccuser->economic.gold, take);
      news->news = QString(_("The jury determined %1 was right, and %2 is guilty of %3"
                            " sabotage. %4 had to return 30\% of it's reserve to %5."))
                       .arg(paccuser->name)
                       .arg(paccused->name)
                       .arg("gold")
                       .arg(paccused->name)
                       .arg(paccuser->name);
      if (was_decision_right) {
        if (info->jury_1_vote == AUDIT_VOTE_YES) {
          int bonus = int(ceil(pjury1->economic.gold * 0.1f));
          log_warning("Jury 1: %d -> %d (%d)", pjury1->economic.gold,
                      pjury1->economic.gold + bonus, bonus);
          pjury1->economic.gold += bonus;
          notify_player(
              pjury1, nullptr, E_MY_SPY_STEAL_GOLD, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was accurate. You have received a bonus of %d gold."),
              bonus);
        }
        if (info->jury_2_vote == AUDIT_VOTE_YES) {
          int bonus = int(ceil(pjury2->economic.gold * 0.1f));
          log_warning("Jury 2: %d -> %d (%d)", pjury2->economic.gold,
                      pjury2->economic.gold + bonus, bonus);
          pjury2->economic.gold += bonus;
          notify_player(
              pjury2, nullptr, E_MY_SPY_STEAL_GOLD, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was accurate. You have received a bonus of %d gold."),
              bonus);
        }
      } else {
        if (info->jury_1_vote == AUDIT_VOTE_YES) {
          int bonus = int(ceil(pjury1->economic.gold * 0.1f));
          log_warning("Jury 1: %d -> %d (%d)", pjury1->economic.gold,
                      pjury1->economic.gold - bonus, bonus);
          pjury1->economic.gold -= bonus;
          notify_player(
              pjury1, nullptr, E_MY_SPY_STEAL_GOLD, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was wrong. You have been fined the sum of %d gold."),
              -bonus);
        }
        if (info->jury_2_vote == AUDIT_VOTE_YES) {
          int bonus = int(ceil(pjury2->economic.gold * 0.1f));
          log_warning("Jury 2: %d -> %d (%d)", pjury2->economic.gold,
                      pjury2->economic.gold - bonus, bonus);
          pjury2->economic.gold -= bonus;
          notify_player(
              pjury2, nullptr, E_MY_SPY_STEAL_GOLD, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was wrong. You have been fined the sum of %d gold."),
              -bonus);
        }
      }
      break;
    }
    case CONSEQUENCE_SCIENCE: {
      int take = int(ceil(paccused->economic.science_acc * 0.3f));
      paccused->economic.science_acc -= take;
      log_warning("Accused: %d -> %d (%d)",
                  paccused->economic.science_acc + take,
                  paccused->economic.science_acc, take);
      paccuser->economic.science_acc += take;
      log_warning("Accuser: %d -> %d (%d)",
                  paccuser->economic.science_acc - take,
                  paccuser->economic.science_acc, take);
      news->news = QString(_("The jury determined %1 was right, and %2 is guilty of %3"
                            " sabotage. %4 had to return 30\% of it's reserve to %5."))
                       .arg(paccuser->name)
                       .arg(paccused->name)
                       .arg("science")
                       .arg(paccused->name)
                       .arg(paccuser->name);
      if (was_decision_right) {
        if (info->jury_1_vote == AUDIT_VOTE_YES) {
          int bonus = int(ceil(pjury1->economic.science_acc * 0.1f));
          log_warning("Jury 1: %d -> %d (%d)", pjury1->economic.science_acc,
                      pjury1->economic.science_acc + bonus, bonus);
          pjury1->economic.science_acc += bonus;
          notify_player(
              pjury1, nullptr, E_MY_SPY_STEAL_SCIENCE, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was accurate. You have received a bonus of %d science."),
              bonus);
        }
        if (info->jury_2_vote == AUDIT_VOTE_YES) {
          int bonus = int(ceil(pjury2->economic.science_acc * 0.1f));
          log_warning("Jury 2: %d -> %d (%d)", pjury2->economic.science_acc,
                      pjury2->economic.science_acc + bonus, bonus);
          pjury2->economic.science_acc += bonus;
          notify_player(
              pjury2, nullptr, E_MY_SPY_STEAL_SCIENCE, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was accurate. You have received a bonus of %d science."),
              bonus);
        }
      } else {
        if (info->jury_1_vote == AUDIT_VOTE_YES) {
          int bonus = int(ceil(pjury1->economic.science_acc * 0.1f));
          log_warning("Jury 1: %d -> %d (%d)", pjury1->economic.science_acc,
                      pjury1->economic.science_acc - bonus, bonus);
          pjury1->economic.science_acc -= bonus;
          notify_player(
              pjury1, nullptr, E_MY_SPY_STEAL_SCIENCE, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was wrong. You have been fined the sum of %d science."),
              -bonus);
        }
        if (info->jury_2_vote == AUDIT_VOTE_YES) {
          int bonus = int(ceil(pjury2->economic.science_acc * 0.1f));
          log_warning("Jury 2: %d -> %d (%d)", pjury2->economic.science_acc,
                      pjury2->economic.science_acc - bonus, bonus);
          pjury2->economic.science_acc -= bonus;
          notify_player(
              pjury2, nullptr, E_MY_SPY_STEAL_SCIENCE, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was wrong. You have been fined the sum of %d science."),
              -bonus);
        }
      }
      break;
    }
    case CONSEQUENCE_MATERIALS: {
      int take = int(ceil(paccused->economic.materials * 0.2f));
      paccused->economic.materials -= take;
      log_warning("Accused: %d -> %d (%d)",
                  paccused->economic.materials + take,
                  paccused->economic.materials, take);
      paccuser->economic.materials += take;
      log_warning("Accuser: %d -> %d (%d)",
                  paccuser->economic.materials - take,
                  paccuser->economic.materials, take);
      news->news = QString(_("The jury determined %1 was right, and %2 is guilty of %3"
                            " sabotage. %4 had to return 20\% of it's reserve to %5."))
                       .arg(paccuser->name)
                       .arg(paccused->name)
                       .arg("materials")
                       .arg(paccused->name)
                       .arg(paccuser->name);
      if (was_decision_right) {
        if (info->jury_1_vote == AUDIT_VOTE_YES) {
          int bonus = int(ceil(pjury1->economic.materials * 0.1f));
          log_warning("Jury 1: %d -> %d (%d)", pjury1->economic.materials,
                      pjury1->economic.materials + bonus, bonus);
          pjury1->economic.materials += bonus;
          notify_player(
              pjury1, nullptr, E_MY_SPY_STEAL_MATERIALS, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was accurate. You have received a bonus of %d materials."),
              bonus);
        }
        if (info->jury_2_vote == AUDIT_VOTE_YES) {
          int bonus = int(ceil(pjury2->economic.materials * 0.1f));
          log_warning("Jury 2: %d -> %d (%d)", pjury2->economic.materials,
                      pjury2->economic.materials + bonus, bonus);
          pjury2->economic.materials += bonus;
          notify_player(
              pjury2, nullptr, E_MY_SPY_STEAL_MATERIALS, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was accurate. You have received a bonus of %d materials."),
              bonus);
        }
      } else {
        if (info->jury_1_vote == AUDIT_VOTE_YES) {
          int bonus = int(ceil(pjury1->economic.materials * 0.1f));
          log_warning("Jury 1: %d -> %d (%d)", pjury1->economic.materials,
                      pjury1->economic.materials - bonus, bonus);
          pjury1->economic.materials -= bonus;
          notify_player(
              pjury1, nullptr, E_MY_SPY_STEAL_MATERIALS, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was wrong. You have been fined the sum of %d materials."),
              -bonus);
        }
        if (info->jury_2_vote == AUDIT_VOTE_YES) {
          int bonus = int(ceil(pjury2->economic.materials * 0.1f));
          log_warning("Jury 2: %d -> %d (%d)", pjury2->economic.materials,
                      pjury2->economic.materials - bonus, bonus);
          pjury2->economic.materials -= bonus;
          notify_player(
              pjury2, nullptr, E_MY_SPY_STEAL_MATERIALS, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was wrong. You have been fined the sum of %d materials."),
              -bonus);
        }
      }
      break;
    }
    }
  } else if (votes_guilty < votes_innocent) {
    // Accused is innocent!
    bool was_decision_right = sabotage_info->player_src != paccused;
    log_warning("Accused is innocent! Decision was right? %s",
                was_decision_right ? "yes" : "no");
    switch (info->consequence) {
    case CONSEQUENCE_GOLD: {
      int take = int(ceil(paccuser->economic.gold * 0.3f));
      paccuser->economic.gold -= take;
      log_warning("Accused: %d -> %d (%d)", paccused->economic.gold + take,
                  paccused->economic.gold, take);
      paccused->economic.gold += take;
      log_warning("Accuser: %d -> %d (%d)", paccuser->economic.gold - take,
                  paccuser->economic.gold, take);
      news->news =
          QString(
              _("The jury determined %1 was wrong, and %2 is innocent of %3"
                " sabotage. %4 had to compensate %5 with 30\% of it's reserve."))
              .arg(paccuser->name)
              .arg(paccused->name)
              .arg("gold")
              .arg(paccuser->name)
              .arg(paccused->name);
      if (was_decision_right) {
        if (info->jury_1_vote == AUDIT_VOTE_NO) {
          int bonus = int(ceil(pjury1->economic.gold * 0.1f));
          log_warning("Jury 1: %d -> %d (%d)", pjury1->economic.gold,
                      pjury1->economic.gold + bonus, bonus);
          pjury1->economic.gold += bonus;
          notify_player(
              pjury1, nullptr, E_MY_SPY_STEAL_GOLD, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was accurate. You have received a bonus of %d gold."),
              bonus);
        }
        if (info->jury_2_vote == AUDIT_VOTE_NO) {
          int bonus = int(ceil(pjury2->economic.gold * 0.1f));
          log_warning("Jury 2: %d -> %d (%d)", pjury2->economic.gold,
                      pjury2->economic.gold + bonus, bonus);
          pjury2->economic.gold += bonus;
          notify_player(
              pjury2, nullptr, E_MY_SPY_STEAL_GOLD, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was accurate. You have received a bonus of %d gold."),
              bonus);
        }
      } else {
        if (info->jury_1_vote == AUDIT_VOTE_NO) {
          int bonus = int(ceil(pjury1->economic.gold * 0.1f));
          log_warning("Jury 1: %d -> %d (%d)", pjury1->economic.gold,
                      pjury1->economic.gold - bonus, bonus);
          pjury1->economic.gold -= bonus;
          notify_player(
              pjury1, nullptr, E_MY_SPY_STEAL_GOLD, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was wrong. You have been fined the sum of %d gold."),
              -bonus);
        }
        if (info->jury_2_vote == AUDIT_VOTE_NO) {
          int bonus = int(ceil(pjury2->economic.gold * 0.1f));
          log_warning("Jury 2: %d -> %d (%d)", pjury2->economic.gold,
                      pjury2->economic.gold - bonus, bonus);
          pjury2->economic.gold -= bonus;
          notify_player(
              pjury2, nullptr, E_MY_SPY_STEAL_GOLD, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was wrong. You have been fined the sum of %d gold."),
              -bonus);
        }
      }
      break;
    }
    case CONSEQUENCE_SCIENCE: {
      int take = int(ceil(paccuser->economic.science_acc * 0.3f));
      paccuser->economic.science_acc -= take;
      log_warning("Accused: %d -> %d (%d)",
                  paccused->economic.science_acc + take,
                  paccused->economic.science_acc, take);
      paccused->economic.science_acc += take;
      log_warning("Accuser: %d -> %d (%d)",
                  paccuser->economic.science_acc - take,
                  paccuser->economic.science_acc, take);
      news->news =
          QString(
              _("The jury determined %1 was wrong, and %2 is innocent of %3"
                " sabotage. %4 had to compensate %5 with 30\% of it's reserve."))
              .arg(paccuser->name)
              .arg(paccused->name)
              .arg("science")
              .arg(paccuser->name)
              .arg(paccused->name);
      if (was_decision_right) {
        if (info->jury_1_vote == AUDIT_VOTE_NO) {
          int bonus = int(ceil(pjury1->economic.science_acc * 0.1f));
          log_warning("Jury 1: %d -> %d (%d)", pjury1->economic.science_acc,
                      pjury1->economic.science_acc + bonus, bonus);
          pjury1->economic.science_acc += bonus;
          notify_player(
              pjury1, nullptr, E_MY_SPY_STEAL_SCIENCE, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was accurate. You have received a bonus of %d science."),
              bonus);
        }
        if (info->jury_2_vote == AUDIT_VOTE_NO) {
          int bonus = int(ceil(pjury2->economic.science_acc * 0.1f));
          log_warning("Jury 2: %d -> %d (%d)", pjury2->economic.science_acc,
                      pjury2->economic.science_acc + bonus, bonus);
          pjury2->economic.science_acc += bonus;
          notify_player(
              pjury2, nullptr, E_MY_SPY_STEAL_SCIENCE, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was accurate. You have received a bonus of %d science."),
              bonus);
        }
      } else {
        if (info->jury_1_vote == AUDIT_VOTE_NO) {
          int bonus = int(ceil(pjury1->economic.science_acc * 0.1f));
          log_warning("Jury 1: %d -> %d (%d)", pjury1->economic.science_acc,
                      pjury1->economic.science_acc - bonus, bonus);
          pjury1->economic.science_acc -= bonus;
          notify_player(
              pjury1, nullptr, E_MY_SPY_STEAL_SCIENCE, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was wrong. You have been fined the sum of %d science."),
              -bonus);
        }
        if (info->jury_2_vote == AUDIT_VOTE_NO) {
          int bonus = int(ceil(pjury2->economic.science_acc * 0.1f));
          log_warning("Jury 2: %d -> %d (%d)", pjury2->economic.science_acc,
                      pjury2->economic.science_acc - bonus, bonus);
          pjury2->economic.science_acc -= bonus;
          notify_player(
              pjury2, nullptr, E_MY_SPY_STEAL_SCIENCE, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was wrong. You have been fined the sum of %d science."),
              -bonus);
        }
      }
      break;
    }
    case CONSEQUENCE_MATERIALS: {
      int take = int(ceil(paccuser->economic.materials * 0.2f));
      paccuser->economic.materials -= take;
      log_warning("Accused: %d -> %d (%d)",
                  paccused->economic.materials + take,
                  paccused->economic.materials, take);
      paccused->economic.materials += take;
      log_warning("Accuser: %d -> %d (%d)",
                  paccuser->economic.materials - take,
                  paccuser->economic.materials, take);
      news->news =
          QString(
              _("The jury determined %1 was wrong, and %2 is innocent of %3"
                " sabotage. %4 had to compensate %5 with 20\% of it's reserve."))
              .arg(paccuser->name)
              .arg(paccused->name)
              .arg("materials")
              .arg(paccuser->name)
              .arg(paccused->name);
      if (was_decision_right) {
        if (info->jury_1_vote == AUDIT_VOTE_NO) {
          int bonus = int(ceil(pjury1->economic.materials * 0.1f));
          log_warning("Jury 1: %d -> %d (%d)", pjury1->economic.materials,
                      pjury1->economic.materials + bonus, bonus);
          pjury1->economic.materials += bonus;
          notify_player(
              pjury1, nullptr, E_MY_SPY_STEAL_MATERIALS, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was accurate. You have received a bonus of %d materials."),
              bonus);
        }
        if (info->jury_2_vote == AUDIT_VOTE_NO) {
          int bonus = int(ceil(pjury2->economic.materials * 0.1f));
          log_warning("Jury 2: %d -> %d (%d)", pjury2->economic.materials,
                      pjury2->economic.materials + bonus, bonus);
          pjury2->economic.materials += bonus;
          notify_player(
              pjury2, nullptr, E_MY_SPY_STEAL_MATERIALS, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was accurate. You have received a bonus of %d materials."),
              bonus);
        }
      } else {
        if (info->jury_1_vote == AUDIT_VOTE_NO) {
          int bonus = int(ceil(pjury1->economic.materials * 0.1f));
          log_warning("Jury 1: %d -> %d (%d)", pjury1->economic.materials,
                      pjury1->economic.materials - bonus, bonus);
          pjury1->economic.materials -= bonus;
          notify_player(
              pjury1, nullptr, E_MY_SPY_STEAL_MATERIALS, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was wrong. You have been fined the sum of %d materials."),
              -bonus);
        }
        if (info->jury_2_vote == AUDIT_VOTE_NO) {
          int bonus = int(ceil(pjury2->economic.materials * 0.1f));
          log_warning("Jury 2: %d -> %d (%d)", pjury2->economic.materials,
                      pjury2->economic.materials - bonus, bonus);
          pjury2->economic.materials -= bonus;
          notify_player(
              pjury2, nullptr, E_MY_SPY_STEAL_MATERIALS, ftc_server,
              _("The government's internal audit found that your recent "
              "decision was wrong. You have been fined the sum of %d materials."),
              -bonus);
        }
      }
      break;
    }
    }
  } else {
    // Stalemate!
    log_warning("Stalemate!");
    // Nothing happens
    news->news =
        QString(
            _("The jury could not reach a veredict regarding %1 vs %2, and therefore "
              "the accusation was dropped."))
            .arg(paccuser->name)
            .arg(paccused->name);
  }


  // Mark sabotage as no longer actionable
  sabotage_info->actionable = false;
  info->is_over = true;
  // Clear on curr_audit
  g_info.curr_audits[curr_audit_idx] = -1;

  // Update info on clients
  update_sabotage_info(sabotage_info);
  update_government_audit_info(info);
  update_government_info();
}

void update_government_audit_info(struct government_audit_info* audit)
{
  struct packet_government_audit_info pinfo;
  pinfo.id = audit->id;
  pinfo.sabotage_id = audit->sabotage_id;
  pinfo.accused_id = (player_id) audit->accused_id;
  pinfo.accuser_id = (player_id) audit->accuser_id;
  pinfo.jury_1_vote = audit->jury_1_vote;
  pinfo.jury_2_vote = audit->jury_2_vote;
  pinfo.consequence = audit->consequence;
  pinfo.timestamp = audit->timestamp;
  pinfo.is_over = audit->is_over;

  lsend_packet_government_audit_info(game.est_connections, &pinfo);
}

void update_sabotage_info(struct sabotage_info *info)
{
  struct packet_sabotage_info_self pkt;
  pkt.id = info->id;
  pkt.consequence = info->consequence;
  pkt.actionable = info->actionable;
  pkt.timestamp = info->timestamp;
  // Don't send src player; it's basic security
  pkt.player_src = -1;
  pkt.player_tgt = player_number(info->player_tgt);
  strcpy(pkt.info, info->info.c_str());

  conn_list_iterate(game.est_connections, pconn)
  {
    if(pconn->playing == info->player_send_to) {
      send_packet_sabotage_info_self(pconn, &pkt);
    }
  }
  conn_list_iterate_end;
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
    pkt.sabotage_id = audit->sabotage_id;
    pkt.accuser_id = audit->accuser_id;
    pkt.accused_id = audit->accused_id;
    pkt.jury_1_vote = audit->jury_1_vote;
    pkt.jury_2_vote = audit->jury_2_vote;
    pkt.consequence = audit->consequence;
    pkt.timestamp = audit->timestamp;
    pkt.is_over = audit->is_over;

    send_packet_government_audit_info(pplayer->current_conn, &pkt);
  }
}

void handle_government_audit_submit_vote(struct player *pplayer, int audit_id, int vote)
{
  struct government_audit_info *audit = g_info.find_cached_audit(audit_id);
  if(!audit) return;

  player_id id = player_id_from_string(pplayer->name);
  if(id == audit->accuser_id || id == audit->accused_id) {
    log_error("Player %s tried to vote on their own audit!", pplayer->name);
    return;
  }

  if (id == determine_jury_id(audit->accuser_id, audit->accused_id, 1)) {
    audit->jury_1_vote = vote;
  } else if (id == determine_jury_id(audit->accuser_id, audit->accused_id, 2)) {
    audit->jury_2_vote = vote;
  } else {
    log_error("Player %s tried to vote on an audit they are not part of!", pplayer->name);
    return;
  }
  update_government_audit_info(audit);

  // If both juries have voted, finish the audit
  if(audit->jury_1_vote != AUDIT_VOTE_NONE && audit->jury_2_vote != AUDIT_VOTE_NONE) {
    finish_audit(audit);
  }

}

void handle_government_audit_start(struct player *pplayer, int sabotage_id, int accused_id)
{
  struct sabotage_info* sabotage = s_info.find_cached_sabotage(sabotage_id);
  if(!sabotage) return;
  struct government_audit_info *audit = g_info.new_government_audit(sabotage_id);
  audit->accuser_id = player_id_from_string(pplayer->name);
  audit->accused_id = player_id(accused_id);
  audit->jury_1_vote = AUDIT_VOTE_NONE;
  audit->jury_2_vote = AUDIT_VOTE_NONE;
  audit->consequence = sabotage->consequence;
  audit->is_over = false;

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
      if (pcity
          && ((act == ACTION_SABOTAGE_CITY_INVESTIGATE_GOLD
               || act == ACTION_SABOTAGE_CITY_INVESTIGATE_SCIENCE
               || act == ACTION_SABOTAGE_CITY_INVESTIGATE_MATERIALS)
              || ((act == ACTION_SABOTAGE_CITY_STEAL_GOLD
                   || act == ACTION_SABOTAGE_CITY_STEAL_SCIENCE
                   || act == ACTION_SABOTAGE_CITY_STEAL_MATERIALS)
                  && pplayer->server.sabotage_count
                         < game.info.sabotage_limit))) {
        // Calculate the probabilities.
        probabilities[act] = action_prob_vs_city(punit, act, pcity);
        target_city_id = plrtile->site->identity;
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
          )
      && (
            (act == ACTION_SABOTAGE_BUILDING_INVESTIGATE_GOLD || act == ACTION_SABOTAGE_BUILDING_INVESTIGATE_SCIENCE || act == ACTION_SABOTAGE_BUILDING_INVESTIGATE_MATERIALS)
            ||
            (
              (act == ACTION_SABOTAGE_BUILDING_STEAL_GOLD || act == ACTION_SABOTAGE_BUILDING_STEAL_SCIENCE || act == ACTION_SABOTAGE_BUILDING_STEAL_MATERIALS
              ) && pplayer->server.sabotage_count < game.info.sabotage_limit
            )
          )
        ) {
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
    pkt.consequence = info->consequence;
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
    pkt.consequence = info->consequence;
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
