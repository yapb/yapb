//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

Support::Support () {
  need_to_send_welcome_ = false;
  welcome_timer_.invalidate ();

  // add default welcome sentences; replaced by gamedef.cfg sentences section when present
  sentences_ = { "hello user,communication is acquired", "your presence is acknowledged", "high man, your in command now",
    "blast your hostile for good", "high man, kill some idiot here", "is there a doctor in the area", "warning, experimental materials detected",
    "high amigo, shoot some but", "time for some bad ass explosion", "bad ass son of a breach device activated",
    "high, do not question this great service", "engine is operative, hello and goodbye",
    "high amigo, your administration has been great last day", "attention, expect experimental armed hostile presence",
    "warning, medical attention required", "high man, at your command", "check check, test, mike check, talk device is activated",
    "hello pal, at your service", "good, day mister, your administration is now acknowledged", "attention, anomalous agent activity, detected",
    "mister, you are going down", "all command access granted, over and out",
    "buzwarn hostile presence detected nearest to your sector. over and out. doop", "hostile resistance detected" };
}

bool Support::IsVisible (const ystl::Vector &origin, edict_t *ent) {
  if (game.IsNullEntity (ent)) {
    return false;
  }
  Trace::Result tr {};
  trace.Line (ent->v.origin + ent->v.view_ofs, origin, TraceIgnore::Everything, ent, &tr);

  if (!ystl::fequal (tr.fraction, 1.0f)) {
    return false;
  }
  return trace.IsEndpointClear (tr);
}

void Support::DecalTrace (Trace::Result *result, int decal_index) {
  // this function draw spraypaint depending on the tracing results

  if (ystl::fequal (result->fraction, 1.0f) || decal_index <= 0) {
    return;
  }
  int entity_index = -1, message = TE_DECAL;

  if (!game.IsNullEntity (result->hit)) {
    if (result->hit->v.solid == SOLID_BSP || result->hit->v.movetype == MOVETYPE_PUSHSTEP) {
      entity_index = game.IndexOfEntity (result->hit);
    }
    else {
      return;
    }
  }
  else {
    entity_index = 0;
  }

  if (entity_index != 0) {
    if (decal_index > 255) {
      message = TE_DECALHIGH;
      decal_index -= 256;
    }
  }
  else {
    message = TE_WORLDDECAL;

    if (decal_index > 255) {
      message = TE_WORLDDECALHIGH;
      decal_index -= 256;
    }
  }
  MessageWriter msg {};

  msg.Start (MSG_BROADCAST, SVC_TEMPENTITY)
    .WriteByte (message)
    .WriteCoord (result->end_pos.x)
    .WriteCoord (result->end_pos.y)
    .WriteCoord (result->end_pos.z)
    .WriteByte (decal_index);

  if (entity_index) {
    msg.WriteShort (entity_index);
  }
  msg.end ();
}

void Support::CheckWelcome () {
  // the purpose of this function, is  to send quick welcome message, to the listenserver entity

  if (game.IsDedicatedServer () || !cv_display_welcome_text || !need_to_send_welcome_) {
    return;
  }

  const bool need_to_send_msg = (graph.Length () > 0 ? need_to_send_welcome_ : true);
  auto receive_ent = game.GetLocalEntity ();

  if (game.IsAliveEntity (receive_ent) && !welcome_timer_.started () && need_to_send_msg) {
    welcome_timer_.start (2.0f + mp_freezetime.As<float> ()); // receive welcome message in four seconds after game has commencing
  }

  if (welcome_timer_.started () && welcome_timer_.elapsed () && need_to_send_msg) {
    game.ServerCommand ("speak \"%s\"", sentences_.random ());
    ystl::String author_str = "Official Navigation Graph";

    auto graph_author = graph.GetAuthor ();
    auto graph_modified = graph.GetModifiedBy ();

    // legacy welcome message, to respect the original code
    constexpr ystl::StringRef kLegacyWelcomeMessage = "Welcome to POD-Bot V2.5 by Count Floyd\n"
                                                      "Visit http://www.nuclearbox.com/podbot/ or\n"
                                                      "      http://www.botepidemic.com/podbot for Updates\n";

    // it's should be send in very rare cases
    const bool send_legacy_welcome = ystl::rg.chance (game.Is (GameFlags::Legacy) ? 25 : 2);

    if (!graph_author.starts_with (product.name)) {
      author_str.assignf ("Navigation Graph by: %s", graph_author);

      if (!graph_modified.empty ()) {
        author_str.appendf (" (Modified by: %s)", graph_modified);
      }
    }
    // dynamic buffer, graph author is unbounded
    ystl::String modern_welcome_message {};
    modern_welcome_message.assignf (
      "\nHello! You are playing with %s v%s\nDevised by %s\n\n%s", product.name, product.version, product.author, author_str);

    ystl::String modern_chat_welcome_message {};
    modern_chat_welcome_message.assignf (
      "----- %s v%s {%s}, by %s (%s)-----", product.name, product.version, product.date, product.author, product.url);

    // send a chat-position message
    MessageWriter (MSG_ONE, msgs.Id (NetMsg::TextMsg), nullptr, receive_ent)
      .WriteByte (HUD_PRINTTALK)
      .WriteString (modern_chat_welcome_message.chars ());

    hudtextparms_t text_params {
      .x = -1.0f,
      .y = send_legacy_welcome ? 0.0f : -1.0f,
      .effect = ystl::rg (1, 2),
      .r1 = static_cast<uint8_t> (send_legacy_welcome ? 255 : ystl::rg (33, 255)),
      .g1 = static_cast<uint8_t> (send_legacy_welcome ? 0 : ystl::rg (33, 255)),
      .b1 = static_cast<uint8_t> (send_legacy_welcome ? 0 : ystl::rg (33, 255)),
      .a1 = static_cast<uint8_t> (0),
      .r2 = static_cast<uint8_t> (send_legacy_welcome ? 255 : ystl::rg (230, 255)),
      .g2 = static_cast<uint8_t> (send_legacy_welcome ? 255 : ystl::rg (230, 255)),
      .b2 = static_cast<uint8_t> (send_legacy_welcome ? 255 : ystl::rg (230, 255)),
      .a2 = static_cast<uint8_t> (200),
      .fadeinTime = 0.0078125f,
      .fadeoutTime = 2.0f,
      .holdTime = 6.0f,
      .fxTime = 0.25f,
      .channel = 1,
    };

    // send the hud message
    game.SendHudMessage (receive_ent, text_params, send_legacy_welcome ? kLegacyWelcomeMessage.chars () : modern_welcome_message.chars ());

    welcome_timer_.invalidate ();
    need_to_send_welcome_ = false;
  }
}

edict_t *Support::FindNearest (const NearestPlayerQuery &query, bool bots_only) {
  // find the nearest player matching the given team and state filters

  if (query.origin == nullptr) [[unlikely]] {
    return nullptr;
  }
  const float max_distance_sq = ystl::sqrf (query.distance);
  float nearest_player_distance_sq = ystl::sqrf (4096.0f); // nearest player

  edict_t *best = nullptr;

  for (const auto &client : clients) {
    if (!client.IsUsedAndNot (query.origin) || (bots_only && !client.IsBot ())) {
      continue;
    }

    if ((query.same_team && client.team != game.GetPlayerTeam (query.origin)) || (query.alive && !client.IsUsedAndAlive ()) ||
        (query.visible && (client.ent->v.effects & EF_NODRAW)) || (query.skip_c4 && has_flag (client.ent->v.weapons, Weapon::C4))) {

      continue; // filter players with parameters
    }
    const float distance_sq = client.ent->v.origin.distance_sq (query.origin->v.origin);

    if (distance_sq < nearest_player_distance_sq && distance_sq < max_distance_sq) {
      nearest_player_distance_sq = distance_sq;
      best = client.ent;
    }
  }
  return best;
}

ystl::Optional<edict_t *> Support::FindNearestPlayer (const NearestPlayerQuery &query) {
  if (const auto best = FindNearest (query, false)) {
    return best;
  }
  return {};
}

ystl::Optional<Bot *> Support::FindNearestBot (const NearestPlayerQuery &query) {
  // fake clients never qualify: they have no bot object behind the edict
  if (const auto best = FindNearest (query, true)) {
    if (const auto found = bots[best]) {
      return found;
    }
  }
  return {};
}

ystl::String Support::GetCurrentDateTime () {
  time_t ticks = time (&ticks);
  tm timeinfo {};

  ystl::plat.loctime (&timeinfo, &ticks);

  auto timebuf = ystl::strings.chars ();
  strftime (timebuf, ystl::Strings::StaticBufferSize, "%d-%m-%Y %H:%M:%S", &timeinfo);

  return ystl::String (timebuf);
}

ystl::StringRef Support::GetFakeSteamId (edict_t *ent) {
  if (!cv_enable_fake_steamids || !game.IsPlayerEntity (ent)) {
    return "BOT";
  }
  auto bot_name_hash = ystl::StringRef::fnv1a32 (ent->v.netname.chars ());

  // just fake steam id a d return it with get player authid function
  return ystl::strings.format ("STEAM_0:1:%d", ystl::abs (static_cast<int32_t> (bot_name_hash) & 0xffff00));
}

ystl::StringRef Support::WeaponIdToAlias (Weapon id) {
  // weapons from the table carry their own alias (see gamedef.cfg alias key)
  if (auto *weapon = conf.GetWeapon (id)) {
    if (!weapon->alias.empty ()) {
      return weapon->alias;
    }
  }

  // equipment is not a part of the weapon table
  for (const auto &entry : kEquipmentAliases) {
    if (entry.id == id) {
      return entry.alias;
    }
  }
  return "none";
}

float Support::GetWaveFileDuration (ystl::StringRef filename) {
  ystl::MemFile fp (ystl::strings.join_path (cv_chatter_path.As<ystl::StringRef> (), ystl::strings.format ("%s.wav", filename)));

  if (!fp) {
    return 0.0f;
  }
  static ystl::WaveHelper wh {};

  return wh.get_duration (&fp);
}

void Support::SetCustomCvarDescriptions () {
  // set the cvars custom descriptions here if needed

  ystl::String restrict_info = "Specifies a semicolon separated list of weapons that are not allowed to be bought/picked up.\n";
  restrict_info += "The list of weapons for Counter-Strike 1.6:\n";

  // weapons from the table, in stable order
  for (const auto &weapon : conf.GetWeapons ()) {
    if (!weapon.alias.empty () && !weapon.full_name.empty ()) {
      restrict_info.appendf ("%s - %s\n", weapon.alias.chars (), weapon.full_name.chars ());
    }
  }

  // equipment
  for (const auto &entry : kEquipmentAliases) {
    restrict_info.appendf ("%s - %s\n", entry.alias.chars (), entry.full_name.chars ());
  }
  game.SetCvarDescription (cv_restricted_weapons, restrict_info);
}

void Support::ApplySentenceDefs (const ystl::ConfNode *root) {
  if (!root) {
    return; // no sentences section - keep built-in defaults
  }
  ystl::Array<ystl::String> sentences {};

  // bare list items, one sentence per line
  for (const auto &node : root->children ()) {
    if (node->is_scalar () && !node->value ().empty ()) {
      sentences.push (ystl::String (node->value ()));
    }
  }

  // replace built-in defaults only when the section has valid entries
  if (!sentences.empty ()) {
    sentences_ = ystl::move (sentences);
  }
}

} // namespace bot
