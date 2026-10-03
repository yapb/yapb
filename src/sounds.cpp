//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

// prefix table is checked top to bottom with first match winning
namespace bot {

static constexpr SoundTemplate kSoundDatabase[] = {
  // c4 / bomb
  { .prefix = "weapons/c4_d",        .flags = Noise::Defuse,     .base_radius = 2048.0f,           .duration = 3.00f },
  { .prefix = "weapons/c4_beep",     .flags = Noise::Defuse,     .base_radius = kBombHearDistance, .duration = 2.00f },
  { .prefix = "weapons/c4_explode",  .flags = Noise::Defuse,     .base_radius = 4096.0f,           .duration = 4.00f },
  { .prefix = "weapons/c4_p",        .flags = Noise::Defuse,     .base_radius = 1024.0f,           .duration = 1.50f },

  // smoke grenade
  { .prefix = "weapons/sg_explode",  .flags = Noise::SGDetonate, .base_radius = 1024.0f,           .duration = 2.00f },

  // grenade explosions
  { .prefix = "weapons/explode",     .flags = Noise::Explosion,  .base_radius = 2048.0f,           .duration = 3.00f },
  { .prefix = "weapons/flashbang-",  .flags = Noise::Explosion,  .base_radius = 2048.0f,           .duration = 2.00f },

  // ricochet / bullet impacts
  { .prefix = "weapons/ric",         .flags = Noise::Ricochet,   .base_radius = 1024.0f,           .duration = 1.50f },
  { .prefix = "weapons/bullet_hit",  .flags = Noise::Ricochet,   .base_radius = 1024.0f,           .duration = 1.50f },

  // zoom / weapon gadget
  { .prefix = "weapons/zoo",         .flags = Noise::Zoom,       .base_radius = 512.0f,            .duration = 0.50f },

  // quiet weapon sounds: knife swings, reloads, grenade bounces and pin pulls
  { .prefix = "weapons/knife_",      .flags = Noise::Misc,       .base_radius = 512.0f,            .duration = 0.50f },
  { .prefix = "weapons/clip",        .flags = Noise::Misc,       .base_radius = 512.0f,            .duration = 0.75f },
  { .prefix = "weapons/slide",       .flags = Noise::Misc,       .base_radius = 512.0f,            .duration = 0.75f },
  { .prefix = "weapons/bolt",        .flags = Noise::Misc,       .base_radius = 512.0f,            .duration = 0.75f },
  { .prefix = "weapons/reload",      .flags = Noise::Misc,       .base_radius = 512.0f,            .duration = 0.75f },
  { .prefix = "weapons/grenade_hit", .flags = Noise::Misc,       .base_radius = 512.0f,            .duration = 0.50f },
  { .prefix = "weapons/g_bounce",    .flags = Noise::Misc,       .base_radius = 512.0f,            .duration = 0.50f },
  { .prefix = "weapons/pinpull",     .flags = Noise::Misc,       .base_radius = 512.0f,            .duration = 0.50f },

  // weapon fire catch-all (after all other weapons/ overrides)
  { .prefix = "weapons/",            .flags = Noise::WeaponFire, .base_radius = 2048.0f,           .duration = 2.00f },

  // player hit / fall
  { .prefix = "player/bhit",         .flags = Noise::HitFall,    .base_radius = 768.0f,            .duration = 2.00f },
  { .prefix = "player/head",         .flags = Noise::HitFall,    .base_radius = 768.0f,            .duration = 2.00f },

  // footsteps
  { .prefix = "player/pl_",          .flags = Noise::Footstep,   .base_radius = 1280.0f,           .duration = 1.50f },

  // item pickups
  { .prefix = "items/gunpick",       .flags = Noise::Pickup,     .base_radius = 768.0f,            .duration = 1.50f },
  { .prefix = "items/9mm",           .flags = Noise::Ammo,       .base_radius = 512.0f,            .duration = 1.00f },

  // world interaction
  { .prefix = "hostage/hos",         .flags = Noise::Hostage,    .base_radius = 1024.0f,           .duration = 5.00f },
  { .prefix = "doors/doorm",         .flags = Noise::Door,       .base_radius = 1024.0f,           .duration = 3.50f },
  { .prefix = "debris/bust",         .flags = Noise::Broke,      .base_radius = 1024.0f,           .duration = 3.00f },
};

Sounds::Sounds () {
  // built-in defaults; replaced entirely by gamedef.cfg hearablesounds section when present
  for (const auto &entry : kSoundDatabase) {
    templates_.push (entry);
  }
}

const SoundTemplate *Sounds::Classify (ystl::StringRef sample) {
  for (const auto &entry : templates_) {
    if (sample.starts_with (entry.prefix)) {
      return &entry;
    }
  }
  return nullptr;
}

void Sounds::SetNoise (ClientNoise &noise, float dist, float duration, const ystl::Vector &pos, Noise type) {
  const auto now = game.Time ();

  noise.dist = dist;
  noise.last = now + duration;
  noise.start = now;
  noise.pos = pos;
  noise.type = type;
}

void Sounds::Acquire (edict_t *ent, ystl::StringRef sample, float volume, float attenuation) {
  if (game.IsNullEntity (ent) || sample.empty ()) {
    return;
  }
  const ystl::Vector origin = game.GetEntityOrigin (ent);

  if (origin.empty ()) {
    return;
  }
  const auto *tmpl = Classify (sample);

  if (!tmpl) {
    return;
  }

  // keep track of smoke grenade detonation positions
  if (has_flag (tmpl->flags, Noise::SGDetonate)) {
    sgtrack.Acquire (ent, origin);
  }

  // compute effective range from base radius, volume, and engine attenuation
  float effective_radius = tmpl->base_radius;

  if (attenuation > 0.01f) {
    effective_radius *= 0.8f / attenuation;
  }
  else {
    effective_radius *= 4.0f;
  }
  effective_radius *= volume;

  // attribute sound to correct client
  Client *client = nullptr;

  if (game.IsPlayerEntity (ent)) {
    client = &clients[ent];
  }
  else {
    Client *nearest = nullptr;
    float nearest_sq = ystl::sqrf (kInfiniteDistance);

    for (auto &c : clients) {
      if (!c.IsUsedAndAlive ()) {
        continue;
      }
      const auto distance_sq = c.origin.distance_sq (origin);

      if (distance_sq < nearest_sq) {
        nearest = &c;
        nearest_sq = distance_sq;
      }
    }
    client = nearest;
  }

  if (!client) {
    return;
  }

  // update noise stats - compare with faded current range
  if (client->noise.last > game.Time ()) {
    const float existing = client->noise.CurrentRange ();

    if (existing < effective_radius) {
      SetNoise (client->noise, effective_radius, tmpl->duration, origin, tmpl->flags);
    }
    else if (has_flag (client->noise.type, tmpl->flags)) {

      // refresh position and timer for same sound type even if not louder
      SetNoise (client->noise, ystl::max (existing, effective_radius), tmpl->duration, origin, tmpl->flags);
    }
  }
  else {
    SetNoise (client->noise, effective_radius, tmpl->duration, origin, tmpl->flags);
  }
}

void Sounds::SimulateNoise (int player_index) {
  // simulate sounds the server sound hook does not capture

  if (player_index < 0 || player_index >= game.MaxClients ()) {
    return;
  }
  auto &client = clients[player_index];

  ClientNoise noise {};
  const auto now = game.Time ();

  auto buttons = client.ent->v.button | client.ent->v.oldbuttons;

  // pressed attack button - fallback for gunfire the hook may miss
  if (buttons & IN_ATTACK) {
    noise.dist = 2048.0f;
    noise.last = now + 0.3f;
    noise.type = Noise::WeaponFire;
  }

  // pressed use button - not always emitted via engine sounds
  else if (buttons & IN_USE) {
    noise.dist = 512.0f;
    noise.last = now + 0.5f;
    noise.type = Noise::Misc;
  }

  // pressed reload button - usually captured by hook, keep as fallback
  else if (buttons & IN_RELOAD) {
    noise.dist = 512.0f;
    noise.last = now + 0.5f;
    noise.type = Noise::Misc;
  }

  // uses ladder - no engine sound emitted for ladder climbing
  else if (client.ent->v.movetype == MOVETYPE_FLY) {
    if (ystl::abs (client.ent->v.velocity.z) > 50.0f) {
      noise.dist = 1024.0f;
      noise.last = now + 0.3f;
      noise.type = Noise::Misc;
    }
  }

  // moves fast enough - footsteps are simulated from velocity, scaled by speed
  else if (mp_footsteps) {
    noise.dist = 1280.0f * (client.ent->v.velocity.length2d () / 260.0f);
    noise.last = now + 0.3f;
    noise.type = Noise::Footstep;
  }

  if (noise.dist <= 0.0f) {
    return;
  }

  // keep existing noise if it's louder (after fade) and still active
  if (client.noise.last > now) {
    if (client.noise.CurrentRange () <= noise.dist) {
      SetNoise (client.noise, noise.dist, noise.last - now, client.ent->v.origin, noise.type);
    }
  }
  else {
    SetNoise (client.noise, noise.dist, noise.last - now, client.ent->v.origin, noise.type);
  }
}

// return current effective range with sustain + linear fade
float ClientNoise::CurrentRange () const {
  const float duration = last - start;

  if (duration <= 0.0f) {
    return dist;
  }
  const float elapsed = game.Time () - start;

  if (elapsed >= duration) {
    return 0.0f;
  }
  // sustain at full range for first 40% of lifetime, then linear fade
  constexpr float kSustainFraction = 0.4f;
  const float sustain_end = duration * kSustainFraction;

  if (elapsed <= sustain_end) {
    return dist;
  }
  const float fade_duration = duration - sustain_end;
  return dist * (1.0f - (elapsed - sustain_end) / fade_duration);
}

} // namespace bot
