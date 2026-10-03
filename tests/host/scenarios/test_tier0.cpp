//
// YaPB test host: Tier0 pure-logic cases (no engine boot).
//
// SPDX-License-Identifier: Unlicense
//
// Weapon data invariants: initWeapons() is pure data plus optional file
// overlays (absent under the harness CWD), so the resulting tables plus
// the constexpr id names and bitmasks can be checked without a server.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

TEST_CASE ("tier0/weapon_data") {
  // light prefix only: postload (logger, cvars, cs load) without any
  // server lifecycle. missing gamedef.cfg/weapon.cfg keep the defaults.
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  conf.InitWeapons ();

  auto &weapons = conf.GetWeapons ();
  CHECK (!weapons.empty ());
  CHECK (weapons.size () >= 20);

  // unique ids, aliases and entity names
  for (size_t i = 0; i < weapons.size (); ++i) {
    for (size_t j = i + 1; j < weapons.size (); ++j) {
      CHECK (weapons[i].id != weapons[j].id);
      CHECK (weapons[i].alias != weapons[j].alias);
      CHECK (weapons[i].name != weapons[j].name);
    }
    CHECK (!weapons[i].alias.empty ());
    CHECK (!weapons[i].name.empty ());
    CHECK (weapons[i].price >= 0);
  }

  // spot prices (CS defaults, no weapon.cfg overlay under the harness)
  CHECK (conf.FindWeaponById (Weapon::Knife).price == 0);
  CHECK (conf.FindWeaponById (Weapon::USP).price == 500);
  CHECK (conf.FindWeaponById (Weapon::Glock18).price == 400);
  CHECK (conf.FindWeaponById (Weapon::Deagle).price == 650);
  CHECK (conf.FindWeaponById (Weapon::P228).price == 600);
  CHECK (conf.FindWeaponById (Weapon::AK47).price == 2500);
  CHECK (conf.FindWeaponById (Weapon::M4A1).price == 3100);
  CHECK (conf.FindWeaponById (Weapon::AWP).price == 4750);
  CHECK (conf.FindWeaponById (Weapon::M249).price == 5750);
  CHECK (conf.FindWeaponById (Weapon::Shield).price == 2200);

  // id <-> index roundtrip for every entry
  for (size_t i = 0; i < weapons.size (); ++i) {
    CHECK (conf.FindWeaponIndexById (weapons[i].id) == static_cast<int32_t> (i));
    CHECK (conf.FindWeaponById (weapons[i].id).name == weapons[i].name);
  }

  // bitmask consistency: every gun bit lives in primary|secondary,
  // snipers are a subset of primaries, masks do not overlap
  CHECK ((kPrimaryWeaponMask & kSecondaryWeaponMask) == 0);
  CHECK ((kSniperWeaponMask & ~kPrimaryWeaponMask) == 0);

  for (const auto &weapon : weapons) {
    if (weapon.type == WeaponType::Melee || weapon.id == Weapon::Shield) {
      continue;
    }
    CHECK (has_flag (kPrimaryWeaponMask | kSecondaryWeaponMask, ystl::bit (weapon.id)));
  }

  testhost::CloseFakeCs (cs);
}

} // namespace bot
