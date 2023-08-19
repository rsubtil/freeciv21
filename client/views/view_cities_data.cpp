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
 * in the city view (formally known as the city report).
 */

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>

// utility
#include "fcintl.h"
#include "log.h"
#include "nation.h"
#include "support.h"

// common
#include "city.h"
#include "culture.h"
#include "game.h"
#include "specialist.h"
#include "unitlist.h"

// client
#include "citydlg_common.h" // city_production_cost_str()
#include "governor.h"
#include "options.h"

#include "views/view_cities_data.h"

/**
   cr_entry = return an entry (one column for one city) for the city report
   These return ptrs to filled in static strings.
   Note the returned string may not be exactly the right length; that
   is handled later.
 */
static QString cr_entry_cityname(const struct city *pcity, const void *data)
{
  /* We used to truncate the name to 14 bytes.  This should not be needed
   * in any modern GUI library and may give an invalid string if a
   * multibyte character is clipped. */
  return city_name_get(pcity);
}

/**
   Returns number of supported units written to string.
   Returned string is statically allocated and its contents change when
   this function is called again.
 */
static QString cr_entry_supported(const struct city *pcity, const void *data)
{
  static char buf[8];
  int num_supported = unit_list_size(pcity->units_supported);

  fc_snprintf(buf, sizeof(buf), "%2d", num_supported);

  return buf;
}

/**
   Returns production surplus written to string.
   Returned string is statically allocated and its contents change when
   this function is called again.
 */
static QString cr_entry_prodplus(const struct city *pcity, const void *data)
{
  static char buf[8];
  fc_snprintf(buf, sizeof(buf), "%3d", pcity->surplus[O_SHIELD]);
  return buf;
}

/**
   Returns gold surplus written to string.
   Returned string is statically allocated and its contents change when
   this function is called again.
 */
static QString cr_entry_gold(const struct city *pcity, const void *data)
{
  static char buf[8];

  if (pcity->surplus[O_GOLD] > 0) {
    fc_snprintf(buf, sizeof(buf), "+%d", pcity->surplus[O_GOLD]);
  } else {
    fc_snprintf(buf, sizeof(buf), "%3d", pcity->surplus[O_GOLD]);
  }
  return buf;
}

/**
   Returns science output written to string.
   Returned string is statically allocated and its contents change when
   this function is called again.
 */
static QString cr_entry_science(const struct city *pcity, const void *data)
{
  static char buf[8];
  fc_snprintf(buf, sizeof(buf), "%3d", pcity->prod[O_SCIENCE]);
  return buf;
}

/**
   Returns material output written to string.
   Returned string is statically allocated and its contents change when
   this function is called again.
 */
static QString cr_entry_material(const struct city *pcity, const void *data)
{
  static char buf[8];
  fc_snprintf(buf, sizeof(buf), "%3d", pcity->prod[O_MATERIALS]);
  return buf;
}

/**
   Returns name of current production.
   Returned string is statically allocated and its contents change when
   this function is called again.
 */
static QString cr_entry_building(const struct city *pcity, const void *data)
{
  static char buf[192];
  const char *from_worklist = worklist_is_empty(&pcity->worklist) ? ""
                              : gui_options->concise_city_production
                                  ? "+"
                                  : _("(worklist)");

  if (city_production_has_flag(pcity, IF_GOLD)) {
    fc_snprintf(buf, sizeof(buf), "%s (%d)%s",
                city_production_name_translation(pcity),
                MAX(0, pcity->surplus[O_SHIELD]), from_worklist);
  } else if (city_production_has_flag(pcity, IF_NOTHING)) {
    fc_snprintf(buf, sizeof(buf), "%s",
                city_production_name_translation(pcity));
  } else {
    fc_snprintf(buf, sizeof(buf), "%s (%d/%s)%s",
                city_production_name_translation(pcity), pcity->shield_stock,
                city_production_cost_str(pcity), from_worklist);
  }

  return buf;
}

/**
   Returns cost of buying current production and turns to completion
   written to string. Returned string is statically allocated and its
   contents change when this function is called again.
 */
static QString cr_entry_build_cost(const struct city *pcity,
                                   const void *data)
{
  Q_UNUSED(data)
  char bufone[8];
  char buftwo[8];
  static char buf[32];
  int price;
  int turns;

  if (city_production_has_flag(pcity, IF_GOLD) ||
      city_production_has_flag(pcity, IF_NOTHING)) {
    fc_snprintf(buf, sizeof(buf), "*");
    return buf;
  }
  price = pcity->client.buy_cost;
  turns = city_production_turns_to_build(pcity, true);

  if (price > 99999) {
    fc_snprintf(bufone, sizeof(bufone), "---");
  } else {
    fc_snprintf(bufone, sizeof(bufone), "%d", price);
  }
  if (turns > 999) {
    fc_snprintf(buftwo, sizeof(buftwo), "--");
  } else {
    fc_snprintf(buftwo, sizeof(buftwo), "%3d", turns);
  }
  fc_snprintf(buf, sizeof(buf), "%s/%s", buftwo, bufone);
  return buf;
}

/* City report options (which columns get shown)
 * To add a new entry, you should just have to:
 * - add a function like those above
 * - add an entry in the base_city_report_specs[] table
 */

// This generates the function name and the tagname:
#define FUNC_TAG(var) cr_entry_##var, #var

static const struct city_report_spec base_city_report_specs[] = {
    // CITY columns
    {true, -15, 0, nullptr, N_("?city:Name"), N_("City: Name"), nullptr,
     FUNC_TAG(cityname)},

    // UNIT columns
    {true, 2, 1, N_("Units"),
     // TRANS: Header "Number of units supported by given city"
     N_("?Supported (units):Owned"), N_("Units: Number supported"), nullptr,
     FUNC_TAG(supported)},

    // RESOURCE columns
    {true, 3, 1, nullptr, N_("?Production surplus [short]:+P"),
     N_("Resources: Surplus Production"), nullptr, FUNC_TAG(prodplus)},

    // ECONOMY columns
    {true, 3, 1, nullptr, N_("?Gold:G"), N_("Surplus: Gold"), nullptr,
     FUNC_TAG(gold)},
    {true, 3, 1, nullptr, N_("?Science:S"), N_("Surplus: Science"), nullptr,
     FUNC_TAG(science)},
    {true, 3, 1, nullptr, N_("?Materials:S"), N_("Surplus: Materials"), nullptr,
     FUNC_TAG(material)},

    // PRODUCTION columns
    {true, 9, 1, N_("Production"), N_("Turns/Buy"),
     /*N_("Turns or gold to complete production"), future menu needs
        translation */
     N_("Production: Turns/gold to complete"), nullptr,
     FUNC_TAG(build_cost)},
    {true, 0, 1, N_("Currently Building"), N_("?Stock/Target:(Have/Need)"),
     N_("Production: Currently Building"), nullptr, FUNC_TAG(building)}};

std::vector<city_report_spec> city_report_specs;
static int num_creport_cols;

/**
   Simple wrapper for num_creport_cols()
 */
int num_city_report_spec() { return num_creport_cols; }

/**
   Simple wrapper for city_report_specs.show
 */
bool *city_report_spec_show_ptr(int i)
{
  return &(city_report_specs[i].show);
}

/**
   Simple wrapper for city_report_specs.tagname
 */
const char *city_report_spec_tagname(int i)
{
  return city_report_specs[i].tagname;
}

/**
   Initialize city report data.  This deals with ruleset-depedent
   columns and pre-translates the fields (to make things easier on
   the GUI writers).  Should be called before the GUI starts up.
 */
void init_city_report_game_data()
{
  static char sp_explanation[SP_MAX][128];
  static char sp_explanations[SP_MAX * 128];
  struct city_report_spec *p;
  int i;

  num_creport_cols =
      ARRAY_SIZE(base_city_report_specs) + specialist_count() + 1;
  city_report_specs = std::vector<city_report_spec>(num_creport_cols);
  p = &city_report_specs[0];

  memcpy(p, base_city_report_specs, sizeof(base_city_report_specs));

  for (i = 0; i < ARRAY_SIZE(base_city_report_specs); i++) {
    if (p->title1) {
      p->title1 = Q_(p->title1);
    }
    if (p->title2) {
      p->title2 = Q_(p->title2);
    }
    p->explanation = _(p->explanation);
    p++;
  }

  fc_assert(NUM_CREPORT_COLS
            == ARRAY_SIZE(base_city_report_specs) + specialist_count() + 1);
}

/**
  The following several functions allow intelligent sorting city report
  fields by column.  This doesn't necessarily do the right thing, but
  it's better than sorting alphabetically.

  The GUI gives us two values to compare (as strings).  We try to split
  them into an array of numeric and string fields, then we compare
  lexicographically.  Two numeric fields are compared in the obvious
  way, two character fields are compared alphabetically.  Arbitrarily, a
  numeric field is sorted before a character field (for "justification"
  note that numbers are before letters in the ASCII table).
 */

/* A datum is one short string, or one number.
   A datum_vector represents a long string of alternating strings and
   numbers.
 */
struct datum {
  union {
    float numeric_value;
    char *string_value;
  } val;
  bool is_numeric;
};
#define SPECVEC_TAG datum
#include "specvec.h"

/**
   Init a datum from a substring.
 */
static void init_datum_string(struct datum *dat, const char *left,
                              const char *right)
{
  int len = right - left;

  dat->is_numeric = false;
  dat->val.string_value = new char[len + 1];
  memcpy(dat->val.string_value, left, len);
  dat->val.string_value[len] = 0;
}

/**
   Init a datum from a number (a float because we happen to use
   strtof).
 */
static void init_datum_number(struct datum *dat, float val)
{
  dat->is_numeric = true;
  dat->val.numeric_value = val;
}

/**
   Free the data associated with a datum -- that is, free the string if
   it was allocated.
 */
static void free_datum(struct datum *dat)
{
  if (!dat->is_numeric) {
    delete[] dat->val.string_value;
  }
}

/**
   Compare two data items as described above:
   - numbers in the obvious way
   - strings alphabetically
   - number < string for no good reason
 */
static int datum_compare(const struct datum *a, const struct datum *b)
{
  if (a->is_numeric == b->is_numeric) {
    if (a->is_numeric) {
      if (a->val.numeric_value == b->val.numeric_value) {
        return 0;
      } else if (a->val.numeric_value < b->val.numeric_value) {
        return -1;
      } else if (a->val.numeric_value > b->val.numeric_value) {
        return +1;
      } else {
        return 0; // shrug
      }
    } else {
      return strcmp(a->val.string_value, b->val.string_value);
    }
  } else {
    if (a->is_numeric) {
      return -1;
    } else {
      return 1;
    }
  }
}

/**
   Compare two strings of data lexicographically.
 */
static int data_compare(const struct datum_vector *a,
                        const struct datum_vector *b)
{
  int i, n;

  n = MIN(a->size, b->size);

  for (i = 0; i < n; i++) {
    int cmp = datum_compare(&a->p[i], &b->p[i]);

    if (cmp != 0) {
      return cmp;
    }
  }

  /* The first n fields match; whoever has more fields goes last.
     If they have equal numbers, the two really are equal. */
  return a->size - b->size;
}

/**
   Split a string into a vector of datum.
 */
static void split_string(struct datum_vector *data, const char *str)
{
  const char *string_start;

  datum_vector_init(data);
  string_start = str;
  while (*str) {
    char *endptr;
    float value;

    errno = 0;
    value = strtof(str, &endptr);
    if (errno != 0 || endptr == str || !std::isfinite(value)) {
      // that wasn't a sensible number; go on
      str++;
    } else {
      /* that was a number, so stop the string we were parsing, add
         it (unless it's empty), then add the number we just parsed */
      struct datum d;

      if (str != string_start) {
        init_datum_string(&d, string_start, str);
        datum_vector_append(data, d);
      }

      init_datum_number(&d, value);
      datum_vector_append(data, d);

      // finally, update the string position pointers
      string_start = str = endptr;
    }
  }

  // if we have anything leftover then it's a string
  if (str != string_start) {
    struct datum d;

    init_datum_string(&d, string_start, str);
    datum_vector_append(data, d);
  }
}

/**
   Free every datum in the vector.
 */
static void free_data(struct datum_vector *data)
{
  int i;

  for (i = 0; i < data->size; i++) {
    free_datum(&data->p[i]);
  }
  datum_vector_free(data);
}

/**
   The real function: split the two strings, and compare them.
 */
int cityrepfield_compare(const char *str1, const char *str2)
{
  struct datum_vector data1, data2;
  int retval;

  if (str1 == str2) {
    return 0;
  } else if (nullptr == str1) {
    return 1;
  } else if (nullptr == str2) {
    return -1;
  }

  split_string(&data1, str1);
  split_string(&data2, str2);

  retval = data_compare(&data1, &data2);

  free_data(&data1);
  free_data(&data2);

  return retval;
}

/**
   Same as can_city_sell_building(), but with universal argument.
 */
bool can_city_sell_universal(const struct city *pcity,
                             const struct universal *target)
{
  return target->kind == VUT_IMPROVEMENT
         && can_city_sell_building(pcity, target->value.building);
}
