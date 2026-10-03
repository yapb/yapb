//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

bool Rehlds::Ensure () {
  if (tried_) {
    return active_;
  }
  tried_ = true;

  // rehlds is a dedicated server engine, never present on listen/mobile
  if (!game.IsDedicatedServer ()) {
    return false;
  }
  const auto &elib = game.Elib ();

  if (!elib) {
    return false;
  }
  const auto factory = elib.resolve<RehldsCreateInterfaceFn> ("CreateInterface");

  if (!factory) {
    return false;
  }
  int ret = 0;
  const auto api = static_cast<IRehldsApi *> (factory (kRehldsHldsApiVersion, &ret));

  if (!api || ret != 0) {
    return false;
  }

  // major pins the vtable layout, minor only grows the tail
  if (api->GetMajorVersion () != kRehldsApiVersionMajor || api->GetMinorVersion () < kRehldsApiVersionMinor) {
    return false;
  }
  const auto funcs = api->GetFuncs ();

  if (!funcs || !funcs->DropClient) {
    return false;
  }
  api_ = api;
  funcs_ = funcs;
  active_ = true;

  return true;
}

bool Rehlds::Probe () {
  return Ensure ();
}

bool Rehlds::DropClient (int entindex, ystl::StringRef reason) {
  if (entindex <= 0 || reason.empty ()) {
    return false;
  }

  if (!Ensure ()) {
    return false;
  }
  auto server_static = api_->GetServerStatic ();

  if (!server_static) {
    return false;
  }
  const int client_id = entindex - 1;

  if (client_id < 0 || client_id >= server_static->GetMaxClients ()) {
    return false;
  }
  auto client = server_static->GetClient (client_id);

  if (!client) {
    return false;
  }
  funcs_->DropClient (client, false, "%s", reason.chars ());

  return true;
}

} // namespace bot
