//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

ystl::StringRef DebugPanel::NoiseName (Noise noise) {
  // pick the most descriptive flag set on the noise, single-bit lookup
  struct NoiseName {
    Noise flag;
    ystl::StringRef name;
  };

  static constexpr NoiseName kNames[] {
    { Noise::WeaponFire, "WeaponFire" },
    { Noise::Explosion,  "Explosion"  },
    { Noise::Defuse,     "Defuse"     },
    { Noise::SGDetonate, "SGDetonate" },
    { Noise::Ricochet,   "Ricochet"   },
    { Noise::Footstep,   "Footstep"   },
    { Noise::HitFall,    "HitFall"    },
    { Noise::Pickup,     "Pickup"     },
    { Noise::Hostage,    "Hostage"    },
    { Noise::Door,       "Door"       },
    { Noise::Broke,      "Broke"      },
    { Noise::Ammo,       "Ammo"       },
    { Noise::Zoom,       "Zoom"       },
    { Noise::Misc,       "Misc"       },
  };

  for (const auto &entry : kNames) {
    if (has_flag (noise, entry.flag)) {
      return entry.name;
    }
  }
  return "None";
}

ystl::StringRef DebugPanel::PersonalityName (Personality personality) {
  switch (personality) {
  case Personality::Rusher:
    return "Rusher";

  case Personality::Careful:
    return "Careful";

  case Personality::Normal:
  default:
    return "Normal";
  }
}

ystl::String DebugPanel::KeysName (int buttons) {
  // build a space separated list of the movement/action keys currently held
  ystl::String result {};

  struct KeyName {
    int button;
    ystl::StringRef name;
  };

  static constexpr KeyName kKeys[] {
    { IN_FORWARD,   "Fwd"    },
    { IN_BACK,      "Back"   },
    { IN_MOVELEFT,  "Left"   },
    { IN_MOVERIGHT, "Right"  },
    { IN_JUMP,      "Jump"   },
    { IN_DUCK,      "Duck"   },
    { IN_ATTACK,    "Fire"   },
    { IN_ATTACK2,   "Fire2"  },
    { IN_USE,       "Use"    },
    { IN_RELOAD,    "Reload" },
  };

  for (const auto &key : kKeys) {
    if (buttons & key.button) {
      if (!result.empty ()) {
        result.append (' ');
      }
      result.append (key.name);
    }
  }
  return result.empty () ? ystl::String ("-") : result;
}

ystl::String DebugPanel::AimFlagsName (AimFlags flags) {
  // build a pipe separated list of the active aim flags
  ystl::String result {};

  struct AimName {
    AimFlags flag;
    ystl::StringRef name;
  };

  static constexpr AimName kNames[] {
    { AimFlags::Nav,         "NAV"      },
    { AimFlags::Camp,        "CAMP"     },
    { AimFlags::PredictPath, "PREDICT"  },
    { AimFlags::LastEnemy,   "LAST"     },
    { AimFlags::Entity,      "ENTITY"   },
    { AimFlags::Enemy,       "ENEMY"    },
    { AimFlags::Grenade,     "GRENADE"  },
    { AimFlags::Override,    "OVERRIDE" },
    { AimFlags::Danger,      "DANGER"   },
    { AimFlags::Flash,       "FLASH"    },
  };

  for (const auto &entry : kNames) {
    if (has_flag (flags, entry.flag)) {
      if (!result.empty ()) {
        result.append ('|');
      }
      result.append (entry.name);
    }
  }
  return result.empty () ? ystl::String ("NONE") : result;
}

ystl::String DebugPanel::SenseName (Sense states) {
  // build a pipe separated list of the active sense flags
  ystl::String result {};

  struct SenseName {
    Sense flag;
    ystl::StringRef name;
  };

  static constexpr SenseName kNames[] {
    { Sense::SeeingEnemy,    "SEE"     },
    { Sense::HearingEnemy,   "HEAR"    },
    { Sense::SuspectEnemy,   "SUSPECT" },
    { Sense::PickupItem,     "PICKUP"  },
    { Sense::ThrowExplosive, "HE"      },
    { Sense::ThrowFlashbang, "FLASH"   },
    { Sense::ThrowSmoke,     "SMOKE"   },
  };

  for (const auto &entry : kNames) {
    if (has_flag (states, entry.flag)) {
      if (!result.empty ()) {
        result.append ('|');
      }
      result.append (entry.name);
    }
  }
  return result.empty () ? ystl::String ("NONE") : result;
}

void DebugPanel::Reset () {
  for (auto &channel : rows_) {
    for (auto &row : channel) {
      row.clear ();
    }
  }
}

void DebugPanel::Render (edict_t *overlay_entity, ystl::StringRef spectating_name) {
  if (game.IsNullEntity (overlay_entity)) {
    return;
  }

  constexpr float kTopInset = 0.25f;
  constexpr float kSideInset = 0.015f;

  const auto make_params = [] (float x, float y, int channel, int r, int g, int b) {
    hudtextparms_t params {
      .x = x,
      .y = y,
      .effect = 0,
      .r1 = static_cast<uint8_t> (r),
      .g1 = static_cast<uint8_t> (g),
      .b1 = static_cast<uint8_t> (b),
      .a1 = 1,
      .r2 = static_cast<uint8_t> (r),
      .g2 = static_cast<uint8_t> (g),
      .b2 = static_cast<uint8_t> (b),
      .a2 = 1,
      .fadeinTime = 0.0f,
      .fadeoutTime = 0.0f,
      .holdTime = kHoldTime,
      .fxTime = 0.0f,
      .channel = channel,
    };
    return params;
  };

  // compose each column: append its channels and their rows
  for (auto &panel : panels_) {
    panel.clear ();
  }

  // center column gets a small header with the spectated bot name
  panels_[ystl::to_underlying (DebugColumn::Center)].appendf ("Debug panel\nspectating %s\n", spectating_name);

  // walk channels in fixed order so the layout stays stable
  for (const auto &info : kChannels) {
    auto &panel = panels_[ystl::to_underlying (info.column)];
    const auto channel = ystl::to_underlying (info.channel);

    panel.appendf ("\n%s\n", info.title);

    for (const auto &row : rows_[channel]) {
      if (!row.empty ()) {
        panel.append (row);
        panel.append ('\n');
      }
      else {
        panel.append ('\n');
      }
    }
  }

  // parse overlay color from the cvar, fall back to white on bad input
  int r = 255, g = 255, b = 255;
  auto color = ystl::String (cv_debug_overlay_color.As<ystl::StringRef> ()).split (" ");

  if (color.size () >= 3) {
    r = ystl::clamp (color[0].as<int> (), 0, 255);
    g = ystl::clamp (color[1].as<int> (), 0, 255);
    b = ystl::clamp (color[2].as<int> (), 0, 255);
  }

  // send the three panels to their screen columns, channel 0 means auto-slot rotation
  game.SendHudMessage (overlay_entity, make_params (kSideInset, kTopInset, 1, r, g, b), panels_[ystl::to_underlying (DebugColumn::Left)]);
  game.SendHudMessage (overlay_entity, make_params (-1.0f, kTopInset, 2, r, g, b), panels_[ystl::to_underlying (DebugColumn::Center)]);
  game.SendHudMessage (
    overlay_entity, make_params (1.0f - kSideInset, kTopInset, 3, r, g, b), panels_[ystl::to_underlying (DebugColumn::Right)]);
}

void DebugPanel::Update (Bot *bot) {
  if (!bot) {
    return;
  }

  Reset ();

  // EYES: enemy tracking and aim state
  if (!game.IsNullEntity (bot->enemy_)) {
    Set (DebugChannel::Eyes, 0, "Enemy: %s", bot->enemy_->v.netname.chars ());
    Set (DebugChannel::Eyes, 1, "Dist: %.0f", bot->enemy_->v.origin.distance (bot->pev->origin));
  }
  else if (!game.IsNullEntity (bot->last_enemy_)) {
    Set (DebugChannel::Eyes, 0, "Last enemy: %s", bot->last_enemy_->v.netname.chars ());
  }
  else {
    Set (DebugChannel::Eyes, 0, "No enemy");
  }
  Set (DebugChannel::Eyes, 2, "FOV: %.1f", bot->IsInFov (bot->dest_origin_ - bot->GetEyesPos ()));
  Set (DebugChannel::Eyes, 3, "Aim: %s", AimFlagsName (bot->aim_flags_).chars ());

  // EARS: sound memory and heard enemy
  Set (DebugChannel::Ears, 0, "Heard: %s", !game.IsNullEntity (bot->heard_enemy_) ? bot->heard_enemy_->v.netname.chars () : "none");
  Set (DebugChannel::Ears, 1, "States: %s", SenseName (bot->states_).chars ());

  // recent sounds, newest first, with type and age
  const auto now = game.Time ();
  int row = 2;

  for (int i = 0; i < BehaviorData::kSoundMemorySize && row < static_cast<int> (Rows); ++i) {
    const auto index = (bot->sound_memory_head_ - 1 - i + BehaviorData::kSoundMemorySize) % BehaviorData::kSoundMemorySize;
    const auto &mem = bot->sound_memory_[index];

    if (mem.source == nullptr || mem.time + 5.0f < now) {
      continue;
    }
    Set (DebugChannel::Ears, row, "%s @ %.0f (%.1fs)", NoiseName (mem.type), mem.pos.distance (bot->pev->origin), now - mem.time);
    ++row;
  }

  // BODY: stuck / terrain
  Set (DebugChannel::Body, 0, "Stuck: %s", bot->IsStuckState () ? "YES" : "no");
  Set (DebugChannel::Body, 1, "Terrain check: %s", bot->check_terrain_ ? "yes" : "no");
  Set (DebugChannel::Body, 2, "Health: %.0f Armor: %.0f", bot->pev->health, bot->pev->armorvalue);

  // LEGS: buttons and speeds
  Set (DebugChannel::Legs, 0, "Move: %.0f Strafe: %.0f", bot->move_speed_, bot->strafe_speed_);
  Set (DebugChannel::Legs, 1, "Keys: %s", KeysName (bot->pev->button));
  Set (DebugChannel::Legs, 2, "Msec: %.0f Starved: %d", bot->last_command_msec_, bot->starved_commands_);

  // HAND: view angles and weapon
  Set (DebugChannel::Hand, 0, "v_angle (%.1f, %.1f)", bot->pev->v_angle.x, bot->pev->v_angle.y);
  Set (DebugChannel::Hand, 1, "ideal (%.1f, %.1f)", bot->ideal_angles_.x, bot->ideal_angles_.y);
  Set (DebugChannel::Hand, 2, "Weapon: %s", util.WeaponIdToAlias (bot->current_weapon_).chars ());

  // CHAT: current chat buffers
  Set (DebugChannel::Chat, 0, "Say idx: %d", bot->say_text_buffer_.entity_index);
  Set (DebugChannel::Chat, 1, "Buffer: %s", bot->chat_buffer_.chars ());

  // COGNITION: personality, task and desire
  Set (DebugChannel::Cognition, 0, "Personality: %s", PersonalityName (bot->personality_));
  Set (DebugChannel::Cognition, 1, "Task: %s", bot->TaskName (bot->GetTaskId ()).chars ());
  Set (DebugChannel::Cognition, 2, "Money: %d", bot->money_amount_);

  // NAVIGATION: node and path state
  Set (DebugChannel::Navigation, 0, "Node: %d Goal: %d", bot->current_node_index_, bot->chosen_goal_index_);
  Set (DebugChannel::Navigation, 1, "Prev goal: %d", bot->prev_goal_index_);
  Set (DebugChannel::Navigation, 2, "Dest: %.0f %.0f %.0f", bot->dest_origin_.x, bot->dest_origin_.y, bot->dest_origin_.z);
}

} // namespace bot
