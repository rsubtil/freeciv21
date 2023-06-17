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
#include "city.h"
#include "events.h"
#include "game.h"
#include "map.h"
#include "player.h"
#include "specialist.h"
#include "unit.h"
#include "worklist.h"

/* common/aicore */
#include "cm.h"

// server
#include "notify.h"
#include "plrhand.h"

#include "govhand.h"
#include "unithand.h"
#include "government.h"
#include "diplomats.h"

void handle_government_info_req(struct player *pplayer)
{
  // DEBUG: Bogus info
  struct packet_government_info info;
  info.last_message_id = 0;
  info.last_audit_id = -1;
  for(int i = 0; i < MAX_AUDIT_NUM; i++) {
    info.curr_audits[i] = -1;
  }
  info.curr_audits[0] = 0;

  send_packet_government_info(pplayer->current_conn, &info);
}

void handle_government_news_req(struct player *pplayer, int id)
{
  if(id != 0) return;

  struct packet_government_news news;
  news.id = id;
  news.turn = 1;
  strcpy(news.news, "The government has decided to ban, uhhhh, something.");

  send_packet_government_news(pplayer->current_conn, &news);
}

void handle_government_audit_info_req(struct player *pplayer, int id)
{
  if(id != 0) return;

  struct packet_government_audit_info info;
  info.id = id;
  info.accuser_id = 0;
  info.accused_id = 2;
  info.jury_1_vote = 0;
  info.jury_2_vote = 0;
  info.consequence = 0;
  info.start_turn = 1;
  info.end_turn = 852;

  lsend_packet_government_audit_info(pplayer->connections, &info);
}

void handle_government_audit_submit_vote(struct player *pplayer, int id, int vote)
{
  // TODO
}

void handle_government_audit_start(struct player *pplayer, int sabotage_id, int accused_id)
{
  // TODO
}

void handle_government_audit_begin_req(struct player *pplayer)
{
  // TODO
}

void handle_sabotage_city_req(struct player *pplayer, int actor_id, int tile_id)
{
  // City-only for now
  struct unit* punit = game_unit_by_number(actor_id);
  struct city* pcity = tile_city(unit_tile(punit));

  fc_assert(punit);
  fc_assert(pcity);

  handle_unit_get_actions(pplayer->current_conn, actor_id,
      IDENTITY_NUMBER_ZERO, tile_id,
      EXTRA_NONE, true);
}
