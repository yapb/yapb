//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

#include <linkage/rehlds_api.h>

// optional ReHLDS fast path for dropping clients (SV_DropClient),
// falls back to the "kick" server command on vanilla engines
namespace bot {

class Rehlds final : public ystl::Singleton<Rehlds> {
private:
  IRehldsApi *api_ {};
  const RehldsFuncs *funcs_ {};

  bool tried_ {};
  bool active_ {};

private:
  bool Ensure ();

public:
  Rehlds () = default;
  ~Rehlds () = default;

public:
  // true when SV_DropClient is usable on this engine
  bool active () const {
    return active_;
  }

  // probes the engine for the rehlds api, true when SV_DropClient is usable
  bool Probe ();

  // drops client by 1-based edict index, true when handled via ReHLDS
  bool DropClient (int entindex, ystl::StringRef reason);
};

YSTL_EXPOSE_GLOBAL_SINGLETON (Rehlds, rehlds);

} // namespace bot
