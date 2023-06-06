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
#pragma once

#include <QHash>
#include <string>
#include <algorithm>
// utility
#include "iterator.h"
#include "shared.h"
// common
#include "fc_types.h"
#include "name_translation.h"
#include "requirements.h"
#include "networking/packets.h"

struct ruler_title; // Opaque type.

/* G_LAST is a value guaranteed to be larger than any valid
 * Government_type_id. It defines the maximum number of governments
 * (so can also be used to size static arrays indexed by governments);
 * it is sometimes used as a sentinel value (but not in the network
 * protocol, which generally uses government_count()). */
#define G_LAST (127)

enum player_id : int {
  PLAYER_PURPLE = 0,
  PLAYER_BLUE = 1,
  PLAYER_GREEN = 2,
  PLAYER_YELLOW = 3,
};

player_id get_player_id(const struct player *pplayer);
std::string player_id_to_string(player_id id);
player_id player_id_from_string(const std::string &str);

/* This is struct government itself.  All information about a form of
 * government is contained inhere. -- SKi */
struct government {
  Government_type_id item_number;
  struct name_translation name;
  bool ruledit_disabled;
  char graphic_str[MAX_LEN_NAME];
  char graphic_alt[MAX_LEN_NAME];
  struct requirement_vector reqs;
  QHash<const struct nation_type *, struct ruler_title *> *ruler_titles;
  int changed_to_times;
  QVector<QString> *helptext;

  // AI cached data for this government.
  struct {
    struct government *better; // hint: a better government (or nullptr)
  } ai;
  government();
  ~government();
};

struct government_news {
  int id;
  int turn;
  QString news;
};

struct government_news* government_news_new(const struct packet_government_news* news);

struct government_audit_info {
  int id;
  player_id accuser_id;
  player_id accused_id;
  int jury_1_vote;
  int jury_2_vote;
  int consequence;
  int start_turn;
  int end_turn;
};

struct government_audit_info* government_audit_info_new(const struct packet_government_audit_info* audit);

struct government_info {
  int last_message_id;
  int last_audit_id;
  int curr_audits[MAX_AUDIT_NUM];

  std::vector<struct government_news*> cached_news;
  std::vector<struct government_audit_info*> cached_audits;

  void reset() {
    last_message_id = -1;
    last_audit_id = -1;
    for(int i = 0; i < MAX_AUDIT_NUM; i++) {
      curr_audits[i] = -1;
    }
    cached_news.clear();
    cached_audits.clear();
  }

  struct government_news* find_cached_news(int id) {
    auto iter = std::find_if(cached_news.begin(), cached_news.end(), [id](struct government_news* news) {
      return news->id == id;
    });
    if(iter != cached_news.end()) {
      return *iter;
    } else {
      return nullptr;
    }
  }

  struct government_audit_info* find_cached_audit(int id) {
    auto iter = std::find_if(cached_audits.begin(), cached_audits.end(), [id](struct government_audit_info* audit) {
      return audit->id == id;
    });
    if(iter != cached_audits.end()) {
      return *iter;
    } else {
      return nullptr;
    }
  }
};

extern struct government_info g_info;

extern std::vector<government> governments;
// General government accessor functions.
Government_type_id government_count();
Government_type_id government_index(const struct government *pgovern);
Government_type_id government_number(const struct government *pgovern);

struct government *government_by_number(const Government_type_id gov);
struct government *government_of_player(const struct player *pplayer);
struct government *government_of_city(const struct city *pcity);

struct government *government_by_rule_name(const char *name);
struct government *government_by_translated_name(const char *name);

const char *government_rule_name(const struct government *pgovern);
const char *government_name_translation(const struct government *pgovern);
const char *government_name_for_player(const struct player *pplayer);

// Ruler titles.
QHash<const struct nation_type *, struct ruler_title *> *
government_ruler_titles(const struct government *pgovern);
struct ruler_title *government_ruler_title_new(
    struct government *pgovern, const struct nation_type *pnation,
    const char *ruler_male_title, const char *ruler_female_title);

const struct nation_type *
ruler_title_nation(const struct ruler_title *pruler_title);
const char *
ruler_title_male_untranslated_name(const struct ruler_title *pruler_title);
const char *
ruler_title_female_untranslated_name(const struct ruler_title *pruler_title);

const char *ruler_title_for_player(const struct player *pplayer, char *buf,
                                   size_t buf_len);

// Ancillary routines
bool can_change_to_government(struct player *pplayer,
                              const struct government *pgovern);

// Initialization and iteration
void governments_alloc(int num);
void governments_free();

bool untargeted_revolution_allowed();
