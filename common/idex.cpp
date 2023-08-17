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

/**
   idex = ident index: a lookup table for quick mapping of unit and city
   id values to unit and city pointers.

   Method: use separate hash tables for each type.
   Means code duplication for city/unit cases, but simplicity advantages.
   Don't have to manage memory at all: store pointers to unit and city
   structs allocated elsewhere, and keys are pointers to id values inside
   the structs.

   Note id values should probably be unsigned int: here leave as plain int
   so can use pointers to pcity->id etc.
 */

// utility
#include "log.h"

// common
#include "city.h"
#include "unit.h"

#include "idex.h"

/**
    Initialize.  Should call this at the start before use.
 */
void idex_init(struct world *iworld)
{
  iworld->cities = new QHash<int, const struct city *>;
  iworld->units = new QHash<int, const struct unit *>;
  iworld->buildings = new QHash<int, const struct building *>;
  iworld->bases_empty = new QHash<int, const struct base_empty *>;
}

/**
    Free the hashs.
 */
void idex_free(struct world *iworld)
{
  delete iworld->cities;
  delete iworld->units;
  delete iworld->buildings;
  delete iworld->bases_empty;
  iworld->cities = nullptr;
  iworld->units = nullptr;
  iworld->buildings = nullptr;
  iworld->bases_empty = nullptr;
}

/**
    Register a city into idex, with current pcity->id.
    Call this when pcity created.
 */
void idex_register_city(struct world *iworld, struct city *pcity)
{
  const struct city *old;

  if (iworld->cities->contains(pcity->id)) {
    old = iworld->cities->value(pcity->id);
    fc_assert_ret_msg(nullptr == old,
                      "IDEX: city collision: new %d %p %s, old %d %p %s",
                      pcity->id, (void *) pcity, city_name_get(pcity),
                      old->id, (void *) old, city_name_get(old));
  }
  iworld->cities->insert(pcity->id, pcity);
}

/**
    Register a unit into idex, with current punit->id.
    Call this when punit created.
 */
void idex_register_unit(struct world *iworld, struct unit *punit)
{
  const struct unit *old;

  if (iworld->units->contains(punit->id)) {
    old = iworld->units->value(punit->id);
    fc_assert_ret_msg(nullptr == old,
                      "IDEX: unit collision: new %d %p %s, old %d %p %s",
                      punit->id, (void *) punit, unit_rule_name(punit),
                      old->id, (void *) old, unit_rule_name(old));
  }
  iworld->units->insert(punit->id, punit);
}

void idex_register_building(struct world *iworld, struct building *pbuilding)
{
  const struct building *old;

  if (iworld->buildings->contains(pbuilding->id)) {
    old = iworld->buildings->value(pbuilding->id);
    fc_assert_ret_msg(nullptr == old,
                      "IDEX: building collision: new %d %p %s, old %d %p %s",
                      pbuilding->id, (void *) pbuilding, building_rulename_get(pbuilding),
                      old->id, (void *) old, building_rulename_get(old));
  }
  iworld->buildings->insert(pbuilding->id, pbuilding);
}

void idex_register_base_empty(struct world *iworld, struct base_empty *pbase_empty)
{
  const struct base_empty *old;

  if (iworld->bases_empty->contains(pbase_empty->id)) {
    old = iworld->bases_empty->value(pbase_empty->id);
    fc_assert_ret_msg(nullptr == old,
                      "IDEX: base empty collision: new %d %p, old %d %p",
                      pbase_empty->id, (void *) pbase_empty,
                      old->id, (void *) old);
  }
  iworld->bases_empty->insert(pbase_empty->id, pbase_empty);
}

/**
    Remove a city from idex, with current pcity->id.
    Call this when pcity deleted.
 */
void idex_unregister_city(struct world *iworld, struct city *pcity)
{
  const struct city *old;

  if (!iworld->units->contains(pcity->id)) {
    old = pcity;
    fc_assert_ret_msg(nullptr != old, "IDEX: city unreg missing: %d %p %s",
                      pcity->id, (void *) pcity, city_name_get(pcity));
    fc_assert_ret_msg(old == pcity,
                      "IDEX: city unreg mismatch: "
                      "unreg %d %p %s, old %d %p %s",
                      pcity->id, (void *) pcity, city_name_get(pcity),
                      old->id, (void *) old, city_name_get(old));
  }
  iworld->cities->remove(pcity->id);
}

/**
    Remove a unit from idex, with current punit->id.
    Call this when punit deleted.
 */
void idex_unregister_unit(struct world *iworld, struct unit *punit)
{
  const struct unit *old;

  if (!iworld->units->contains(punit->id)) {
    old = punit;
    fc_assert_ret_msg(nullptr != old, "IDEX: unit unreg missing: %d %p %s",
                      punit->id, (void *) punit, unit_rule_name(punit));
    fc_assert_ret_msg(old == punit,
                      "IDEX: unit unreg mismatch: "
                      "unreg %d %p %s, old %d %p %s",
                      punit->id, (void *) punit, unit_rule_name(punit),
                      old->id, (void *) old, unit_rule_name(old));
  }
  iworld->units->remove(punit->id);
}

void idex_unregister_building(struct world *iworld, struct building *pbuilding)
{
  const struct building *old;

  if (!iworld->buildings->contains(pbuilding->id)) {
    old = pbuilding;
    fc_assert_ret_msg(nullptr != old, "IDEX: building unreg missing: %d %p %s",
                      pbuilding->id, (void *) pbuilding, building_rulename_get(pbuilding));
    fc_assert_ret_msg(old == pbuilding,
                      "IDEX: building unreg mismatch: "
                      "unreg %d %p %s, old %d %p %s",
                      pbuilding->id, (void *) pbuilding, building_rulename_get(pbuilding),
                      old->id, (void *) old, building_rulename_get(old));
  }
  iworld->buildings->remove(pbuilding->id);
}

void idex_unregister_base_empty(struct world *iworld,
                              struct base_empty *pbase_empty)
{
  const struct base_empty *old;

  if (!iworld->bases_empty->contains(pbase_empty->id)) {
    old = pbase_empty;
    fc_assert_ret_msg(
        nullptr != old, "IDEX: base_empty unreg missing: %d %p",
        pbase_empty->id, (void *) pbase_empty);
    fc_assert_ret_msg(old == pbase_empty,
                      "IDEX: base_empty unreg mismatch: "
                      "unreg %d %p, old %d %p",
                      pbase_empty->id, (void *) pbase_empty,
                      old->id, (void *) old);
  }
  iworld->bases_empty->remove(pbase_empty->id);
}

/**
    Lookup city with given id.
    Returns nullptr if the city is not registered (which is not an error).
 */
struct city *idex_lookup_city(struct world *iworld, int id)
{
  const struct city *pcity;

  pcity = iworld->cities->value(id);

  return const_cast<struct city *>(pcity);
}

/**
    Lookup unit with given id.
    Returns nullptr if the unit is not registered (which is not an error).
 */
struct unit *idex_lookup_unit(struct world *iworld, int id)
{
  const struct unit *punit;

  punit = iworld->units->value(id);

  return const_cast<struct unit *>(punit);
}

struct building *idex_lookup_building(struct world *iworld, int id)
{
  const struct building *pbuilding;

  pbuilding = iworld->buildings->value(id);

  return const_cast<struct building *>(pbuilding);
}

struct base_empty *idex_lookup_base_empty(struct world *iworld, int id)
{
  const struct base_empty *pbase_empty;

  pbase_empty = iworld->bases_empty->value(id);

  return const_cast<struct base_empty *>(pbase_empty);
}
