//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// noise types
namespace bot {

enum class Noise : int32_t {
  HitFall = ystl::bit (0),
  Pickup = ystl::bit (1),
  Zoom = ystl::bit (2),
  Ammo = ystl::bit (3),
  Hostage = ystl::bit (4),
  Broke = ystl::bit (5),
  Door = ystl::bit (6),
  Defuse = ystl::bit (7),
  SGDetonate = ystl::bit (8),
  WeaponFire = ystl::bit (9),
  Footstep = ystl::bit (10),
  Explosion = ystl::bit (11),
  Ricochet = ystl::bit (12),
  Misc = ystl::bit (13)
};
YSTL_ENABLE_ENUM_FLAGS (Noise);

// compile-time sound classification entry
struct SoundTemplate {
  const char *prefix;
  Noise flags;
  float base_radius;
  float duration;
};

// clients noise
struct ClientNoise {
  ystl::Vector pos {};
  float dist {};
  float last {};
  float start {}; // when this sound started (for fade computation)
  Noise type {};

  // return current effective range with sustain + linear fade
  float CurrentRange () const;
};

// single entry in bot's sound memory for tactical recall
struct SoundMemoryEntry {
  ystl::Vector pos {};
  float time {};
  Noise type {};
  edict_t *source {};
};

class Sounds final : public ystl::Singleton<Sounds> {
public:
  friend struct ConfigHook;

private:
  const SoundTemplate *Classify (ystl::StringRef sample);

public:
  Sounds ();
  ~Sounds () = default;

private:
  // sound classification database; defaults from ksounddatabase, replaced by gamedef.cfg
  ystl::Array<SoundTemplate> templates_ {};

  static void SetNoise (ClientNoise &noise, float dist, float duration, const ystl::Vector &pos, Noise type);

public:
  // replaces the sound template database with a config-defined one
  void ApplyTemplates (ystl::Array<SoundTemplate> &&templates) {
    templates_ = ystl::move (templates);
  }

public:
  void Acquire (edict_t *ent, ystl::StringRef sample, float volume, float attenuation);
  void SimulateNoise (int player_index);
};

// expose global
inline auto &sounds { Sounds::instance () };

} // namespace bot
