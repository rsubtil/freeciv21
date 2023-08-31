/**************************************************************************
 Copyright (c) 1996-2020 Freeciv21 and Freeciv contributors. This file is
 __    __          part of Freeciv21. Freeciv21 is free software: you can
/ \\..// \    redistribute it and/or modify it under the terms of the GNU
  ( oo )        General Public License  as published by the Free Software
   \__/         Foundation, either version 3 of the License,  or (at your
                      option) any later version. You should have received
    a copy of the GNU General Public License along with Freeciv21. If not,
                  see https://www.gnu.org/licenses/.
**************************************************************************/
#pragma once

#include "fc_types.h"

#include "hand_gen.h"

struct connection;
struct conn_list;

extern std::map<struct unit *, struct tile *> spy_last_sabotages;

void update_government_info();
void update_government_audit_info(struct government_audit_info* info);
void update_sabotage_info(struct sabotage_info* info);
void spy_send_error(struct player *pplayer, const char* msg);
bool spy_sabotaged_tile_recently(struct unit *punit, struct tile *tile);
void spy_set_recent_sabotaged_tile(struct unit *punit, struct tile *tile);
