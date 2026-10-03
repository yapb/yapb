//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

void ClientManager::Update () {
  // record some stats of all players on the server
  for (int i = 0; i < game.MaxClients (); ++i) {
    auto player = game.PlayerOfIndex (i);
    auto &client = clients_[i];

    if (!game.IsNullEntity (player) && (player->v.flags & FL_CLIENT) && !(player->v.flags & FL_DORMANT)) {
      client.ent = player;
      client.flags |= ClientFlags::Used;

      if (game.IsAliveEntity (player)) {
        client.flags |= ClientFlags::Alive;
      }
      else {
        client.flags &= ~ClientFlags::Alive;
      }

      if (game.IsFakeClientEntity (player)) {
        client.flags |= ClientFlags::Bot;
      }
      else {
        client.flags &= ~ClientFlags::Bot;
      }

      if (has_flag (client.flags, ClientFlags::Alive)) {
        client.origin = player->v.origin;
        sounds.SimulateNoise (i);
      }
    }
    else {
      client.flags &= ~(ClientFlags::Used | ClientFlags::Alive);
      client.ent = nullptr;
    }
  }
}

Client &ClientManager::operator[] (edict_t *ent) {
  // payer edict access (non-const)

  return clients_[game.IndexOfPlayer (ent)];
}

const Client &ClientManager::operator[] (edict_t *ent) const {
  // payer edict  access (const)

  return clients_[game.IndexOfPlayer (ent)];
}

} // namespace bot
