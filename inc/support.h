//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

namespace bot {

class Support final : public ystl::Singleton<Support> {
private:
  bool need_to_send_welcome_ {};
  ystl::CountdownTimer welcome_timer_ {};

  ystl::Array<ystl::String> sentences_ {};

private:
private:
  // equipment aliases for weapon ids outside the weapon table
  static constexpr struct EquipmentAlias {
    Weapon id {};
    ystl::StringRef alias {};
    ystl::StringRef full_name {};
  } kEquipmentAliases[] = {
    { Weapon::Flashbang, "flash",    "Concussion Grenade"     },
    { Weapon::Explosive, "hegren",   "High-Explosive Grenade" },
    { Weapon::Smoke,     "sgren",    "Smoke Grenade"          },
    { Weapon::Armor,     "vest",     "Kevlar Vest"            },
    { Weapon::ArmorHelm, "vesthelm", "Kevlar Vest and Helmet" },
    { Weapon::Defuser,   "defuser",  "Defuser Kit"            },
  };

public:
  Support ();
  ~Support () = default;

public:
  // need to send welcome message ?
  void CheckWelcome ();

  // converts weapon id to alias name
  ystl::StringRef WeaponIdToAlias (Weapon id);

  // nearest player search helper
  // nearest player search parameters, designated initializers at call sites
  struct NearestPlayerQuery {
    edict_t *origin = nullptr; // search around this entity (required)
    float distance = 4096.0f; // search radius
    bool same_team = false; // only teammates of origin
    bool alive = false; // only alive players
    bool visible = false; // skip EF_NODRAW entities
    bool skip_c4 = false; // skip players carrying the c4
  };

  // finds the nearest player matching the query, empty when nobody matches
  ystl::Optional<edict_t *> FindNearestPlayer (const NearestPlayerQuery &query);

  // finds the nearest bot matching the query, empty when nobody matches
  // (fake clients never qualify: they have no bot object behind the edict)
  ystl::Optional<Bot *> FindNearestBot (const NearestPlayerQuery &query);

private:
  // shared search core for the nearest flavors above (bots_only selects real bots)
  static edict_t *FindNearest (const NearestPlayerQuery &query, bool bots_only);

public:
  // tracing decals for bots spraying logos
  void DecalTrace (Trace::Result *result, int decal_index);

  // check if origin is visible from the entity side
  bool IsVisible (const ystl::Vector &origin, edict_t *ent);

  // get the current date and time as string
  ystl::String GetCurrentDateTime ();

  // generates fake steam id from bot name
  ystl::StringRef GetFakeSteamId (edict_t *ent);

  // get's the wave length
  float GetWaveFileDuration (ystl::StringRef filename);

  // set custom cvar descriptions
  void SetCustomCvarDescriptions ();

  // applies gamedef.cfg welcome sentences onto defaults
  void ApplySentenceDefs (const ystl::ConfNode *root);

public:
  // re-show welcome after changelevel ?
  void SetNeedForWelcome (bool need) {
    need_to_send_welcome_ = need;
    welcome_timer_.invalidate ();
  }

  // cosine of the angle between the entity's view and the direction to pos
  float ViewDot (edict_t *ent, const ystl::Vector &pos) const {
    return ent->v.v_angle.forward () | (pos - (ent->v.origin + ent->v.view_ofs)).normalize ();
  }

  // check if position is inside view cone of entity
  bool IsInViewCone (const ystl::Vector &pos, edict_t *ent) const {
    return ViewDot (ent, pos) >= ystl::cosf (ystl::deg2rad ((ent->v.fov > 0 ? ent->v.fov : 90.0f) * 0.5f));
  }

  // converts csteam to team
  Team ConvertFromCsTeam (CSTeam team) {
    if (team == CSTeam::CT) {
      return Team::CT;
    }
    else if (team == CSTeam::Terrorist) {
      return Team::Terrorist;
    }
    return Team::Invalid;
  }

  // converts team to csteam
  CSTeam ConvertFromCsTeam (Team team) {
    if (team == Team::CT) {
      return CSTeam::CT;
    }
    else if (team == Team::Terrorist) {
      return CSTeam::Terrorist;
    }
    return CSTeam::Invalid;
  }
};

// expose global
YSTL_EXPOSE_GLOBAL_SINGLETON (Support, util);

} // namespace bot
