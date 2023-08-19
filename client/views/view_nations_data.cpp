/*__            ___                 ***************************************
/   \          /   \          Copyright (c) 1996-2023 Freeciv21 and Freeciv
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

/*
 * This file contains functions used to gather varying data elements for use
 * in the nations view (formally known as the plrdlg - player dialog).
 */

// utility
#include "fcintl.h"
#include "support.h"

// common
#include "game.h"
#include "government.h"
#include "nation.h"
#include "research.h"

// client
#include "client_main.h"
#include "climisc.h"
#include "options.h"
#include "text.h"

/* client/include */
#include "views/view_nations_data.h"

/**
   The player-name (aka nation leader) column of the plrdlg.
 */
static QString col_name(const struct player *player)
{
  return player_name(player);
}

/**
   Compares the names of two players in players dialog.
 */
static int cmp_name(const struct player *pplayer1,
                    const struct player *pplayer2)
{
  return fc_stricoll(player_name(pplayer1), player_name(pplayer2));
}

/**
    Compare gold of two players in players dialog,
    needed to sort column
 */
static int cmp_gold(const struct player *player1,
                    const struct player *player2)
{
  return player1->economic.gold - player2->economic.gold;
}

/**
    Show player's gold to me if I am allowed to know it
 */
QString col_gold(const struct player *them)
{
  if (them == nullptr || !them->is_alive) {
    return _("-");
  } else if (BV_ISSET(them->client.visible, NI_GOLD)) {
    return QString::number(them->economic.gold);
  } else {
    return _("?");
  }
}

/***
    Compare science (acc) of two players in players dialog,
    needed to sort column
 */
static int cmp_science_acc(const struct player *player1,
                           const struct player *player2)
{
  return player1->economic.science_acc - player2->economic.science_acc;
}

/**
    Show player's science (acc) to me if I am allowed to know it
 */
QString col_science_acc(const struct player *them)
{
  if (them == nullptr || !them->is_alive) {
    return _("-");
  } else if (BV_ISSET(them->client.visible, NI_SCIENCE_ACC)) {
    return QString::number(them->economic.science_acc);
  } else {
    return _("?");
  }
}

/***
    Compare materials of two players in players dialog,
    needed to sort column
 */
static int cmp_materials(const struct player *player1,
                          const struct player *player2)
{
  return player1->economic.materials - player2->economic.materials;
}

/**
    Show player's materials to me if I am allowed to know it
 */
QString col_materials(const struct player *them)
{
  if (them == nullptr || !them->is_alive) {
    return _("-");
  } else if (BV_ISSET(them->client.visible, NI_MATERIALS)) {
    return QString::number(them->economic.materials);
  } else {
    return _("?");
  }
}

/**
  ...
 */
struct player_dlg_column player_dlg_columns[] = {
    {true, COL_TEXT, N_("?Player:Name"), col_name, nullptr, cmp_name,
     "name"},
    {true, COL_FLAG, N_("Flag"), nullptr, nullptr, nullptr, "flag"},
    {true, COL_TEXT, N_("Gold"), col_gold, nullptr, cmp_gold, "gold"},
    {true, COL_TEXT, N_("Science"), col_science_acc, nullptr, cmp_science_acc, "science_acc"},
    {true, COL_TEXT, N_("Materials"), col_materials, nullptr, cmp_materials, "materials"},
};

const int num_player_dlg_columns = ARRAY_SIZE(player_dlg_columns);

/**
   Translate all titles
 */
void init_player_dlg_common()
{
  int i;

  for (i = 0; i < num_player_dlg_columns; i++) {
    player_dlg_columns[i].title = Q_(player_dlg_columns[i].title);
  }
}
