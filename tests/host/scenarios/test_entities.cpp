//
// YaPB test host: unit/entities_forward.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for entities.cpp (included into linkage.cpp under
// LINKENT_STATIC): the ForwardEntityHelper resolve/call-through legs and
// every LINK_ENTITY forwarder (unresolvable in-harness, must no-op safely).
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

// entities.cpp has no header declaration (test-only accessor, no production code)
void ForwardEntityHelper (EntityProto &addr, const char *name, entvars_t *pev);

// linkents carry C linkage via YSTL_EXPORT, namespace is irrelevant for linkage
extern "C" {

void DelayedUse (entvars_t *pev);
void ambient_generic (entvars_t *pev);
void ammo_338magnum (entvars_t *pev);
void ammo_357sig (entvars_t *pev);
void ammo_45acp (entvars_t *pev);
void ammo_50ae (entvars_t *pev);
void ammo_556nato (entvars_t *pev);
void ammo_556natobox (entvars_t *pev);
void ammo_57mm (entvars_t *pev);
void ammo_762nato (entvars_t *pev);
void ammo_9mm (entvars_t *pev);
void ammo_buckshot (entvars_t *pev);
void armoury_entity (entvars_t *pev);
void beam (entvars_t *pev);
void bodyque (entvars_t *pev);
void button_target (entvars_t *pev);
void cycler (entvars_t *pev);
void cycler_prdroid (entvars_t *pev);
void cycler_sprite (entvars_t *pev);
void cycler_weapon (entvars_t *pev);
void cycler_wreckage (entvars_t *pev);
void env_beam (entvars_t *pev);
void env_beverage (entvars_t *pev);
void env_blood (entvars_t *pev);
void env_bombglow (entvars_t *pev);
void env_bubbles (entvars_t *pev);
void env_debris (entvars_t *pev);
void env_explosion (entvars_t *pev);
void env_fade (entvars_t *pev);
void env_funnel (entvars_t *pev);
void env_global (entvars_t *pev);
void env_glow (entvars_t *pev);
void env_laser (entvars_t *pev);
void env_lightning (entvars_t *pev);
void env_message (entvars_t *pev);
void env_rain (entvars_t *pev);
void env_render (entvars_t *pev);
void env_shake (entvars_t *pev);
void env_shooter (entvars_t *pev);
void env_snow (entvars_t *pev);
void env_sound (entvars_t *pev);
void env_spark (entvars_t *pev);
void env_sprite (entvars_t *pev);
void fireanddie (entvars_t *pev);
void func_bomb_target (entvars_t *pev);
void func_breakable (entvars_t *pev);
void func_button (entvars_t *pev);
void func_buyzone (entvars_t *pev);
void func_conveyor (entvars_t *pev);
void func_door (entvars_t *pev);
void func_door_rotating (entvars_t *pev);
void func_escapezone (entvars_t *pev);
void func_friction (entvars_t *pev);
void func_grencatch (entvars_t *pev);
void func_guntarget (entvars_t *pev);
void func_healthcharger (entvars_t *pev);
void func_hostage_rescue (entvars_t *pev);
void func_illusionary (entvars_t *pev);
void func_ladder (entvars_t *pev);
void func_monsterclip (entvars_t *pev);
void func_mortar_field (entvars_t *pev);
void func_pendulum (entvars_t *pev);
void func_plat (entvars_t *pev);
void func_platrot (entvars_t *pev);
void func_pushable (entvars_t *pev);
void func_rain (entvars_t *pev);
void func_recharge (entvars_t *pev);
void func_rot_button (entvars_t *pev);
void func_rotating (entvars_t *pev);
void func_snow (entvars_t *pev);
void func_tank (entvars_t *pev);
void func_tankcontrols (entvars_t *pev);
void func_tanklaser (entvars_t *pev);
void func_tankmortar (entvars_t *pev);
void func_tankrocket (entvars_t *pev);
void func_trackautochange (entvars_t *pev);
void func_trackchange (entvars_t *pev);
void func_tracktrain (entvars_t *pev);
void func_train (entvars_t *pev);
void func_traincontrols (entvars_t *pev);
void func_vehicle (entvars_t *pev);
void func_vehiclecontrols (entvars_t *pev);
void func_vip_safetyzone (entvars_t *pev);
void func_wall (entvars_t *pev);
void func_wall_toggle (entvars_t *pev);
void func_water (entvars_t *pev);
void func_weaponcheck (entvars_t *pev);
void game_counter (entvars_t *pev);
void game_counter_set (entvars_t *pev);
void game_end (entvars_t *pev);
void game_player_equip (entvars_t *pev);
void game_player_hurt (entvars_t *pev);
void game_player_team (entvars_t *pev);
void game_score (entvars_t *pev);
void game_team_master (entvars_t *pev);
void game_team_set (entvars_t *pev);
void game_text (entvars_t *pev);
void game_zone_player (entvars_t *pev);
void gib (entvars_t *pev);
void gibshooter (entvars_t *pev);
void grenade (entvars_t *pev);
void hostage_entity (entvars_t *pev);
void info_bomb_target (entvars_t *pev);
void info_hostage_rescue (entvars_t *pev);
void info_intermission (entvars_t *pev);
void info_landmark (entvars_t *pev);
void info_map_parameters (entvars_t *pev);
void info_null (entvars_t *pev);
void info_player_deathmatch (entvars_t *pev);
void info_player_start (entvars_t *pev);
void info_target (entvars_t *pev);
void info_teleport_destination (entvars_t *pev);
void info_vip_start (entvars_t *pev);
void infodecal (entvars_t *pev);
void item_airtank (entvars_t *pev);
void item_airbox (entvars_t *pev);
void item_antidote (entvars_t *pev);
void item_assaultsuit (entvars_t *pev);
void item_battery (entvars_t *pev);
void item_healthkit (entvars_t *pev);
void item_kevlar (entvars_t *pev);
void item_longjump (entvars_t *pev);
void item_security (entvars_t *pev);
void item_sodacan (entvars_t *pev);
void item_suit (entvars_t *pev);
void item_thighpack (entvars_t *pev);
void light (entvars_t *pev);
void light_environment (entvars_t *pev);
void light_spot (entvars_t *pev);
void momentary_door (entvars_t *pev);
void momentary_rot_button (entvars_t *pev);
void monster_hevsuit_dead (entvars_t *pev);
void monster_mortar (entvars_t *pev);
void monster_scientist (entvars_t *pev);
void multi_manager (entvars_t *pev);
void multisource (entvars_t *pev);
void path_corner (entvars_t *pev);
void path_track (entvars_t *pev);
void player (entvars_t *pev);
void player_loadsaved (entvars_t *pev);
void player_weaponstrip (entvars_t *pev);
void point_clientcommand (entvars_t *pev);
void point_servercommand (entvars_t *pev);
void soundent (entvars_t *pev);
void spark_shower (entvars_t *pev);
void speaker (entvars_t *pev);
void target_cdaudio (entvars_t *pev);
void test_effect (entvars_t *pev);
void trigger (entvars_t *pev);
void trigger_auto (entvars_t *pev);
void trigger_autosave (entvars_t *pev);
void trigger_bomb_reset (entvars_t *pev);
void trigger_camera (entvars_t *pev);
void trigger_cdaudio (entvars_t *pev);
void trigger_changelevel (entvars_t *pev);
void trigger_changetarget (entvars_t *pev);
void trigger_counter (entvars_t *pev);
void trigger_endsection (entvars_t *pev);
void trigger_gravity (entvars_t *pev);
void trigger_hurt (entvars_t *pev);
void trigger_monsterjump (entvars_t *pev);
void trigger_multiple (entvars_t *pev);
void trigger_once (entvars_t *pev);
void trigger_push (entvars_t *pev);
void trigger_random (entvars_t *pev);
void trigger_random_time (entvars_t *pev);
void trigger_random_unique (entvars_t *pev);
void trigger_relay (entvars_t *pev);
void trigger_setorigin (entvars_t *pev);
void trigger_teleport (entvars_t *pev);
void trigger_transition (entvars_t *pev);
void weapon_ak47 (entvars_t *pev);
void weapon_aug (entvars_t *pev);
void weapon_awp (entvars_t *pev);
void weapon_c4 (entvars_t *pev);
void weapon_deagle (entvars_t *pev);
void weapon_elite (entvars_t *pev);
void weapon_famas (entvars_t *pev);
void weapon_fiveseven (entvars_t *pev);
void weapon_flashbang (entvars_t *pev);
void weapon_g3sg1 (entvars_t *pev);
void weapon_galil (entvars_t *pev);
void weapon_glock18 (entvars_t *pev);
void weapon_hegrenade (entvars_t *pev);
void weapon_knife (entvars_t *pev);
void weapon_m249 (entvars_t *pev);
void weapon_m3 (entvars_t *pev);
void weapon_m4a1 (entvars_t *pev);
void weapon_mac10 (entvars_t *pev);
void weapon_mp5navy (entvars_t *pev);
void weapon_p228 (entvars_t *pev);
void weapon_p90 (entvars_t *pev);
void weapon_scout (entvars_t *pev);
void weapon_sg550 (entvars_t *pev);
void weapon_sg552 (entvars_t *pev);
void weapon_shield (entvars_t *pev);
void weapon_shieldgun (entvars_t *pev);
void weapon_smokegrenade (entvars_t *pev);
void weapon_tmp (entvars_t *pev);
void weapon_ump45 (entvars_t *pev);
void weapon_usp (entvars_t *pev);
void weapon_xm1014 (entvars_t *pev);
void weaponbox (entvars_t *pev);
void world_items (entvars_t *pev);
void worldspawn (entvars_t *pev);
} // extern "C"

namespace {

void BootEntities (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
   engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

   HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
   cs.reset ();

   GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
   HOST_REQUIRE (cs.entered () != 0);

   gamefuncs_t table {};
   HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

   game.Precache ();
   table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

   if (analyzer.IsAnalyzing ()) {
      analyzer.Suspend ();
   }
}

bool g_forward_hit = false;

void ForwardProbe (entvars_t *) {
   g_forward_hit = true;
}

} // namespace

TEST_CASE ("unit/entities_forward") {
   testhost::FakeEngine engine;
   testhost::FakeCSApi cs {};

   BootEntities (engine, cs);

   // ...unresolvable names leave the slot null and never call...
   {
      EntityProto addr = nullptr;
      ForwardEntityHelper (addr, "no_such_entity_xyz_123", nullptr);
      CHECK (addr == nullptr);
   }

   // ...a preset address skips the resolve and calls straight through...
   {
      EntityProto addr = &ForwardProbe;
      g_forward_hit = false;
      ForwardEntityHelper (addr, "no_such_entity_xyz_123", nullptr);
      CHECK (g_forward_hit);
      CHECK (addr == &ForwardProbe);
   }

   // ...every linkent forwarder is callable with a null pev and no-ops
   // ...when the gamedll has no such symbol (harness has none)...
   DelayedUse (nullptr);
   ambient_generic (nullptr);
   ammo_338magnum (nullptr);
   ammo_357sig (nullptr);
   ammo_45acp (nullptr);
   ammo_50ae (nullptr);
   ammo_556nato (nullptr);
   ammo_556natobox (nullptr);
   ammo_57mm (nullptr);
   ammo_762nato (nullptr);
   ammo_9mm (nullptr);
   ammo_buckshot (nullptr);
   armoury_entity (nullptr);
   beam (nullptr);
   bodyque (nullptr);
   button_target (nullptr);
   cycler (nullptr);
   cycler_prdroid (nullptr);
   cycler_sprite (nullptr);
   cycler_weapon (nullptr);
   cycler_wreckage (nullptr);
   env_beam (nullptr);
   env_beverage (nullptr);
   env_blood (nullptr);
   env_bombglow (nullptr);
   env_bubbles (nullptr);
   env_debris (nullptr);
   env_explosion (nullptr);
   env_fade (nullptr);
   env_funnel (nullptr);
   env_global (nullptr);
   env_glow (nullptr);
   env_laser (nullptr);
   env_lightning (nullptr);
   env_message (nullptr);
   env_rain (nullptr);
   env_render (nullptr);
   env_shake (nullptr);
   env_shooter (nullptr);
   env_snow (nullptr);
   env_sound (nullptr);
   env_spark (nullptr);
   env_sprite (nullptr);
   fireanddie (nullptr);
   func_bomb_target (nullptr);
   func_breakable (nullptr);
   func_button (nullptr);
   func_buyzone (nullptr);
   func_conveyor (nullptr);
   func_door (nullptr);
   func_door_rotating (nullptr);
   func_escapezone (nullptr);
   func_friction (nullptr);
   func_grencatch (nullptr);
   func_guntarget (nullptr);
   func_healthcharger (nullptr);
   func_hostage_rescue (nullptr);
   func_illusionary (nullptr);
   func_ladder (nullptr);
   func_monsterclip (nullptr);
   func_mortar_field (nullptr);
   func_pendulum (nullptr);
   func_plat (nullptr);
   func_platrot (nullptr);
   func_pushable (nullptr);
   func_rain (nullptr);
   func_recharge (nullptr);
   func_rot_button (nullptr);
   func_rotating (nullptr);
   func_snow (nullptr);
   func_tank (nullptr);
   func_tankcontrols (nullptr);
   func_tanklaser (nullptr);
   func_tankmortar (nullptr);
   func_tankrocket (nullptr);
   func_trackautochange (nullptr);
   func_trackchange (nullptr);
   func_tracktrain (nullptr);
   func_train (nullptr);
   func_traincontrols (nullptr);
   func_vehicle (nullptr);
   func_vehiclecontrols (nullptr);
   func_vip_safetyzone (nullptr);
   func_wall (nullptr);
   func_wall_toggle (nullptr);
   func_water (nullptr);
   func_weaponcheck (nullptr);
   game_counter (nullptr);
   game_counter_set (nullptr);
   game_end (nullptr);
   game_player_equip (nullptr);
   game_player_hurt (nullptr);
   game_player_team (nullptr);
   game_score (nullptr);
   game_team_master (nullptr);
   game_team_set (nullptr);
   game_text (nullptr);
   game_zone_player (nullptr);
   gib (nullptr);
   gibshooter (nullptr);
   grenade (nullptr);
   hostage_entity (nullptr);
   info_bomb_target (nullptr);
   info_hostage_rescue (nullptr);
   info_intermission (nullptr);
   info_landmark (nullptr);
   info_map_parameters (nullptr);
   info_null (nullptr);
   info_player_deathmatch (nullptr);
   info_player_start (nullptr);
   info_target (nullptr);
   info_teleport_destination (nullptr);
   info_vip_start (nullptr);
   infodecal (nullptr);
   item_airtank (nullptr);
   item_airbox (nullptr);
   item_antidote (nullptr);
   item_assaultsuit (nullptr);
   item_battery (nullptr);
   item_healthkit (nullptr);
   item_kevlar (nullptr);
   item_longjump (nullptr);
   item_security (nullptr);
   item_sodacan (nullptr);
   item_suit (nullptr);
   item_thighpack (nullptr);
   light (nullptr);
   light_environment (nullptr);
   light_spot (nullptr);
   momentary_door (nullptr);
   momentary_rot_button (nullptr);
   monster_hevsuit_dead (nullptr);
   monster_mortar (nullptr);
   monster_scientist (nullptr);
   multi_manager (nullptr);
   multisource (nullptr);
   path_corner (nullptr);
   path_track (nullptr);
   player (nullptr);
   player_loadsaved (nullptr);
   player_weaponstrip (nullptr);
   point_clientcommand (nullptr);
   point_servercommand (nullptr);
   soundent (nullptr);
   spark_shower (nullptr);
   speaker (nullptr);
   target_cdaudio (nullptr);
   test_effect (nullptr);
   trigger (nullptr);
   trigger_auto (nullptr);
   trigger_autosave (nullptr);
   trigger_bomb_reset (nullptr);
   trigger_camera (nullptr);
   trigger_cdaudio (nullptr);
   trigger_changelevel (nullptr);
   trigger_changetarget (nullptr);
   trigger_counter (nullptr);
   trigger_endsection (nullptr);
   trigger_gravity (nullptr);
   trigger_hurt (nullptr);
   trigger_monsterjump (nullptr);
   trigger_multiple (nullptr);
   trigger_once (nullptr);
   trigger_push (nullptr);
   trigger_random (nullptr);
   trigger_random_time (nullptr);
   trigger_random_unique (nullptr);
   trigger_relay (nullptr);
   trigger_setorigin (nullptr);
   trigger_teleport (nullptr);
   trigger_transition (nullptr);
   weapon_ak47 (nullptr);
   weapon_aug (nullptr);
   weapon_awp (nullptr);
   weapon_c4 (nullptr);
   weapon_deagle (nullptr);
   weapon_elite (nullptr);
   weapon_famas (nullptr);
   weapon_fiveseven (nullptr);
   weapon_flashbang (nullptr);
   weapon_g3sg1 (nullptr);
   weapon_galil (nullptr);
   weapon_glock18 (nullptr);
   weapon_hegrenade (nullptr);
   weapon_knife (nullptr);
   weapon_m249 (nullptr);
   weapon_m3 (nullptr);
   weapon_m4a1 (nullptr);
   weapon_mac10 (nullptr);
   weapon_mp5navy (nullptr);
   weapon_p228 (nullptr);
   weapon_p90 (nullptr);
   weapon_scout (nullptr);
   weapon_sg550 (nullptr);
   weapon_sg552 (nullptr);
   weapon_shield (nullptr);
   weapon_shieldgun (nullptr);
   weapon_smokegrenade (nullptr);
   weapon_tmp (nullptr);
   weapon_ump45 (nullptr);
   weapon_usp (nullptr);
   weapon_xm1014 (nullptr);
   weaponbox (nullptr);
   world_items (nullptr);
   worldspawn (nullptr);

   CHECK (true);
}

} // namespace bot
