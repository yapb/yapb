//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

FunMode::FunMode () {
  last_mode_ = FunModeId::Off;
  saved_gravity_ = 0.0f;
}

FunModeId FunMode::ParseMode (ystl::StringRef value) {
  static ystl::HashMap<ystl::String, FunModeId> modes {
    { "imsober",     FunModeId::Off     },
    { "off",         FunModeId::Off     },
    { "tronisback",  FunModeId::Tron    },
    { "itsnewyear",  FunModeId::NewYear },
    { "imhaunted",   FunModeId::Haunted },
    { "itstoodark",  FunModeId::Dark    },
    { "stonedagain", FunModeId::Stoned  },
    { "imonmars",    FunModeId::Mars    }
  };

  const auto keyword = ystl::String (value).lowercase ();

  if (modes.exists (keyword)) {
    return modes[keyword];
  }
  return FunModeId::Invalid;
}

ystl::StringRef FunMode::KeywordOf (FunModeId mode) {
  switch (mode) {
  case FunModeId::Tron:
    return "tronisback";
  case FunModeId::NewYear:
    return "itsnewyear";
  case FunModeId::Haunted:
    return "imhaunted";
  case FunModeId::Dark:
    return "itstoodark";
  case FunModeId::Stoned:
    return "stonedagain";
  case FunModeId::Mars:
    return "imonmars";
  default:
    return "imsober";
  }
}

ystl::StringRef FunMode::MessageOf (FunModeId mode) {
  switch (mode) {
  case FunModeId::Tron:
    return "from the eighties with Love";
  case FunModeId::NewYear:
    return "Really ? That soon ?";
  case FunModeId::Haunted:
    return "and the Ghosts are coming for you";
  case FunModeId::Dark:
    return "the Bots will light your way";
  case FunModeId::Stoned:
    return "feeling dizzy now ?";
  case FunModeId::Mars:
    return "feel that Gravity...";
  default:
    return "Back to Life, back to Reality";
  }
}

void FunMode::TurnOff () {
  // revert gravity back to the pre-fun value
  if (!ystl::fzero (saved_gravity_)) {
    sv_gravity.Set (saved_gravity_);
    saved_gravity_ = 0.0f;
  }

  // restore player rendering state
  for (const auto &client : clients) {
    if (!client.IsUsed () || game.IsNullEntity (client.ent)) {
      continue;
    }
    auto &pev = client.ent->v;

    pev.rendermode = kRenderNormal;
    pev.renderfx = kRenderFxNone;
    pev.renderamt = 0.0f;
    pev.effects &= ~EF_BRIGHTLIGHT;
  }

  // stop all the pending stuff
  shake_timer_.invalidate ();
}

void FunMode::OnModeChanged () {
  // keep the gravity value to restore upon mode end
  if (last_mode_ == FunModeId::Mars) {
    saved_gravity_ = sv_gravity.As<float> ();
    sv_gravity.Set (kFunMarsGravity);
  }
}

void FunMode::ApplyTron () {
  // light-cycle glow for everyone, red terrorists, blue cts
  for (const auto &client : clients) {
    if (!client.IsUsed () || game.IsNullEntity (client.ent) || !game.IsAliveEntity (client.ent)) {
      continue;
    }
    auto &pev = client.ent->v;

    if (pev.renderfx == kRenderFxGlowShell) {
      continue; // already glowing
    }
    pev.renderfx = kRenderFxGlowShell;
    pev.renderamt = 10.0f;
    pev.rendercolor = client.team == Team::Terrorist ? ystl::Vector (255.0f, 0.0f, 0.0f) : ystl::Vector (0.0f, 0.0f, 255.0f);
  }
}

void FunMode::ApplyNewYear () {
  // firework sparks around the bots eyes
  for (const auto &bot : bots) {
    if (!game.IsAliveEntity (bot.Ent ())) {
      continue;
    }
    const auto origin = bot.pev->origin + bot.pev->view_ofs;

    MessageWriter (MSG_PVS, SVC_TEMPENTITY, origin).WriteByte (TE_SPARKS).WriteCoord (origin.x).WriteCoord (origin.y).WriteCoord (origin.z);
  }
}

void FunMode::ApplyHaunted () {
  // bots are ghosts now
  for (auto &bot : bots) {
    if (!game.IsAliveEntity (bot.Ent ())) {
      continue;
    }
    bot.pev->rendermode = kRenderTransTexture;
    bot.pev->renderamt = 100.0f;
  }
}

void FunMode::ApplyDark () {
  // bots will light your way
  for (auto &bot : bots) {
    if (!game.IsAliveEntity (bot.Ent ())) {
      continue;
    }
    bot.pev->effects |= EF_BRIGHTLIGHT;
  }
}

void FunMode::ApplyStoned () {
  const auto shake_msg = msgs.Id (NetMsg::ScreenShake);

  if (shake_msg < 0) {
    return; // game does not have the screen shake message
  }

  if (!shake_timer_.elapsed ()) {
    return;
  }
  const auto amplitude = MessageWriter::Fu16 (2048.0f, 12.0f);
  const auto duration = MessageWriter::Fu16 (10.0f, 12.0f);
  const auto frequency = MessageWriter::Fu16 (1.0f, 8.0f);

  for (const auto &client : clients) {
    if (!client.IsUsed () || game.IsNullEntity (client.ent) || !game.IsAliveEntity (client.ent)) {
      continue;
    }

    if (!(client.ent->v.flags & FL_ONGROUND)) {
      continue; // don't shake if not onground
    }
    MessageWriter (MSG_ONE_UNRELIABLE, shake_msg, nullptr, client.ent).WriteShort (amplitude).WriteShort (duration).WriteShort (frequency);
  }
  shake_timer_.start (ystl::rg (10.0f, 20.0f));
}

void FunMode::ApplyMars () {
  // keep the gravity low, even if someone changed it
  if (!ystl::fequal (sv_gravity.As<float> (), kFunMarsGravity)) {
    sv_gravity.Set (kFunMarsGravity);
  }
}

void FunMode::Update () {
  // detect transitions, the cvar can be changed out of band
  const auto mode = ParseMode (cv_fun_mode.As<ystl::StringRef> ());

  if (mode != last_mode_) {
    TurnOff ();
    last_mode_ = mode;
    OnModeChanged ();
  }

  if (last_mode_ == FunModeId::Off || last_mode_ == FunModeId::Invalid) {
    return;
  }

  switch (last_mode_) {
  case FunModeId::Tron:
    ApplyTron ();
    break;

  case FunModeId::NewYear:
    ApplyNewYear ();
    break;

  case FunModeId::Haunted:
    ApplyHaunted ();
    break;

  case FunModeId::Dark:
    ApplyDark ();
    break;

  case FunModeId::Stoned:
    ApplyStoned ();
    break;

  case FunModeId::Mars:
    ApplyMars ();
    break;

  default:
    break;
  }
}

void FunMode::SetMode (FunModeId mode) {
  if (mode == FunModeId::Invalid) {
    return;
  }
  cv_fun_mode.Set (KeywordOf (mode).chars ());
}

} // namespace bot
