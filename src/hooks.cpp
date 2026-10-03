//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

int32_t ServerQueryHook::SendTo (int socket, const void *message, size_t length, int flags, const sockaddr *dest, int dest_length) {
  const auto send = [&] (const ystl::Twin<const uint8_t *, size_t> &msg) -> int32_t {
    return ystl::Socket::sendto (socket, msg.first, msg.second, flags, dest, dest_length);
  };

  auto packet = reinterpret_cast<const uint8_t *> (message);
  constexpr int32_t kPacketLength = 5;

  // player replies response
  if (length > kPacketLength && memcmp (packet, "\xff\xff\xff\xff", kPacketLength - 1) == 0) {
    if (packet[4] == 'D') {
      QueryBuffer buffer { packet, length, kPacketLength };
      auto count = buffer.Read<uint8_t> ();

      for (uint8_t i = 0; i < count; ++i) {
        buffer.Skip<uint8_t> (); // number

        auto name = buffer.ReadString (); // name
        buffer.Skip<int32_t> (); // score

        auto ctime = buffer.Read<float> (); // override connection time
        buffer.Write<float> (bots.GetConnectionTimes (name, ctime));
      }
      return send (buffer.Data ());
    }
    else if (packet[4] == 'I') {
      QueryBuffer buffer { packet, length, kPacketLength };
      buffer.Skip<uint8_t> (); // protocol

      // skip server name, folder, map game
      for (size_t i = 0; i < 4; ++i) {
        buffer.SkipString ();
      }
      buffer.Skip<short> (); // steam app id
      buffer.Skip<uint8_t> (); // players
      buffer.Skip<uint8_t> (); // maxplayers
      buffer.Skip<uint8_t> (); // bots
      buffer.Write<uint8_t> (0); // zero out bot count

      return send (buffer.Data ());
    }
    else if (packet[4] == 'm') {
      QueryBuffer buffer { packet, length, kPacketLength };

      buffer.ShiftToEnd (); // shift to the end of buffer
      buffer.Write<uint8_t> (0); // zero out bot count

      return send (buffer.Data ());
    }
  }
  return send ({ packet, length });
}

void ServerQueryHook::Init () {
  // if previously requested to disable?
  if (!cv_enable_query_hook) {
    if (send_to_detour_.detoured ()) {
      Disable ();
    }
    return;
  }

  // do not detour twice
  if (send_to_detour_.detoured () || send_to_detour_sys_.detoured ()) {
    return;
  }

  // do not enable on not dedicated server
  if (!game.IsDedicatedServer ()) {
    return;
  }
  SendToProto *send_to_address = sendto;

  // linux workaround with sendto
  if (!ystl::plat.win && !ystl::plat.is_non_x86 ()) {
    if (game.Elib ()) {
      auto address = game.Elib ().resolve<SendToProto *> ("sendto");

      if (address != nullptr) {
        send_to_address = address;
      }
    }
    send_to_detour_sys_.initialize ("ws2_32.dll", "sendto", sendto);
  }
  send_to_detour_.initialize ("ws2_32.dll", "sendto", send_to_address);

  // enable only on modern games
  if (!game.Is (GameFlags::Legacy) && (ystl::plat.nix || ystl::plat.win) && !ystl::plat.is_non_x86 () && !send_to_detour_.detoured ()) {
    send_to_detour_.install (reinterpret_cast<void *> (ServerQueryHook::SendTo), true);

    if (!send_to_detour_sys_.detoured ()) {
      send_to_detour_sys_.install (reinterpret_cast<void *> (ServerQueryHook::SendTo), true);
    }
  }
}

ystl::SharedLibrary::Func EntityLinkHook::LookupSymbol (ystl::SharedLibrary::Handle module, const char *function) {
  static const auto &gamedll = game.Lib ().handle ();
  static const auto &self = self_.handle ();

  const auto resolve = [&] (ystl::SharedLibrary::Handle handle) {
    return dlsym_ (handle, function);
  };

  if (entlink.NeedsBypass () && ystl::StringRef (function) == "CreateInterface") {
    entlink.SetPaused (true);
    auto ret = resolve (module);

    entlink.Disable ();

    return ret;
  }

  // if requested module is yapb module, put in cache the looked up symbol
  if (self != module) {
    return resolve (module);
  }

#if defined(YSTL_WINDOWS)
  if (HIWORD (function) == 0) {
    return resolve (module);
  }
#endif

  if (exports_.exists (function)) {
    return exports_[function];
  }
  auto bot_addr = resolve (self);

  if (!bot_addr) {
    auto game_addr = resolve (gamedll);

    if (game_addr) {
      return exports_[function] = game_addr;
    }
  }
  else {
    return exports_[function] = bot_addr;
  }
  return nullptr;
}

bool EntityLinkHook::CallPlayerFunction (edict_t *ent) {
  auto call_player = [&] () {
    reinterpret_cast<EntityProto> (reinterpret_cast<void *> (exports_["player"])) (&ent->v);
  };

  if (exports_.exists ("player")) {
    call_player ();
    return true;
  }
  auto player_function = game.Lib ().resolve<EntityProto> ("player");

  if (!player_function) {
    ystl::logger.error ("Cannot resolve player() function in GameDLL.");
    return false;
  }
  exports_["player"] = reinterpret_cast<ystl::SharedLibrary::Func> (reinterpret_cast<void *> (player_function));
  call_player ();

  return true;
}

bool EntityLinkHook::NeedsBypass () const {
  return !ystl::plat.win && !game.IsDedicatedServer ();
}

void EntityLinkHook::Initialize () {
#if defined(LINKENT_STATIC)
  return;
#endif

  if (ystl::plat.is_non_x86 () || game.Is (GameFlags::Metamod)) {
    return;
  }
  constexpr ystl::StringRef kKernel32Module = "kernel32.dll";

  dlsym_.initialize (kKernel32Module, ystl::PlatformDynlink::DlsymName, ystl::PlatformDynlink::Dlsym);
  dlsym_.install (reinterpret_cast<void *> (LookupHandler), true);

  if (NeedsBypass ()) {
    dlclose_.initialize (kKernel32Module, ystl::PlatformDynlink::DlcloseName, ystl::PlatformDynlink::Dlclose);
    dlclose_.install (reinterpret_cast<void *> (CloseHandler), true);
  }
  self_.locate (&engfuncs);
}

} // namespace bot
