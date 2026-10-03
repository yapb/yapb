//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

gamefuncs_t dllapi {};
newgamefuncs_t newapi {};
enginefuncs_t engfuncs {};
gamedll_funcs_t dllfuncs {};

meta_globals_t *gpMetaGlobals = nullptr;
gamedll_funcs_t *gpGamedllFuncs = nullptr;
mutil_funcs_t *gpMetaUtilFuncs = nullptr;
globalvars_t *globals = nullptr;

// metamod plugin information
constinit plugin_info_t Plugin_info = {
  .ifvers = META_INTERFACE_VERSION,
  .name = bot::product.name.chars (),
  .version = bot::product.version.chars (),
  .date = bot::product.date.chars (),
  .author = bot::product.author.chars (),
  .url = bot::product.url.chars (),
  .logtag = bot::product.logtag.chars (),
  .loadable = PT_CHANGELEVEL,
  .unloadable = PT_ANYTIME,
};

// compilers can't create lambdas with vaargs, so put this one in it's own namespace
namespace Hooks {
YSTL_FORCE_STACK_ALIGN void HandlerEngClientCommand (edict_t *ent, char const *format, ...) {
  // this function forces the client whose player entity is ent to issue a client command
  // How it works is that bot::clients all have a argv global string in their client DLL that
  // stores the command string; if ever that string is filled with characters, the client DLL
  // sends it to the engine as a command to be executed. When the engine has executed that
  // command, this argv string is reset to zero. Here is somehow a curious implementation of
  // ClientCommand: the engine sets the command it wants the client to issue in his argv, then
  // the client DLL sends it back to the engine, the engine receives it then executes the
  // command therein. Don't ask me why we need all this complicated crap. Anyhow since bot::bots have
  // no client DLL, be certain never to call this function upon a bot entity, else it will just
  // make the server crash. Since hordes of uncautious, not to say stupid, programmers don't
  // even imagine some players on their servers could be bot::bots, this check is performed less than
  // sometimes actually by their side, that's why we strongly recommend to check it here too. In
  // case it's a bot asking for a client command, we handle it like we do for bot commands

  if (!bot::game.IsNullEntity (ent)) {
    if (bot::bots[ent] || bot::game.IsFakeClientEntity (ent) || (ent->v.flags & FL_DORMANT)) {
      if (bot::game.Is (bot::GameFlags::Metamod)) {
        RETURN_META (MRES_SUPERCEDE); // prevent bot::bots to be forced to issue client commands
      }
      return;
    }
  }

  if (bot::game.Is (bot::GameFlags::Metamod)) {
    RETURN_META (MRES_IGNORED);
  }

  va_list ap;
  auto buffer = ystl::strings.chars ();

  va_start (ap, format);
  ystl::fmtwrap ().vexec (buffer, ystl::Strings::StaticBufferSize, format, ap);
  va_end (ap);

  engfuncs.pfnClientCommand (ent, buffer);
}
}

YSTL_EXPORT int GetEntityAPI (gamefuncs_t *table, int interface_version) {
  // this function is called right after GiveFnptrsToDll() by the engine in the bot::game DLL (or
  // what it BELIEVES to be the bot::game DLL), in order to copy the list of MOD functions that can
  // be called by the engine, into a memory block pointed to by the functionTable pointer
  // that is passed into this function (explanation comes straight from botman). This allows
  // the Half-Life engine to call these MOD DLL functions when it needs to spawn an entity,
  // connect or disconnect a player, call Think() functions, Touch() functions, or Use()
  // functions, etc. The bot DLL passes its OWN list of these functions back to the Half-Life
  // engine, and then calls the MOD DLL's version of GetEntityAPI to get the REAL gamedll
  // functions this time (to use in the bot code)

  ystl::memzero (table, sizeof (gamefuncs_t));

  if (!bot::game.Is (bot::GameFlags::Metamod)) {
    auto api_get_entity_api = bot::game.Lib ().resolve<decltype (&GetEntityAPI)> (__func__);

    // pass other DLLs engine callbacks to function table
    if (!api_get_entity_api || api_get_entity_api (&dllapi, interface_version) == 0) {
      ystl::logger.fatal ("Could not resolve symbol \"%s\" in the bot::game dll.", __func__);
    }
    dllfuncs.dllapi_table = &dllapi;
    gpGamedllFuncs = &dllfuncs;

    memcpy (table, &dllapi, sizeof (gamefuncs_t));
  }

  table->pfnGameInit = [] () YSTL_FORCE_STACK_ALIGN {
    // this function is a one-time call, and appears to be the second function called in the
    // DLL after GiveFntprsToDll() has been called. Its purpose is to tell the MOD DLL to
    // initialize the bot::game before the engine actually hooks into it with its video frames and
    // bot::clients connecting. Note that it is a different step than the *server* initialization
    // This one is called once, and only once, when the bot::game process boots up before the first
    // server is enabled. Here is a good place to do our own bot::game session initialization, and
    // to register by the engine side the server commands we need to administrate our bot::bots

    // execute main config
    bot::conf.LoadMainConfig (true);
    bot::conf.AdjustWeaponPrices ();

    // print info about dll
    bot::game.PrintBotVersion ();

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    dllapi.pfnGameInit ();
  };

  table->pfnSpawn = [] (edict_t *ent) YSTL_FORCE_STACK_ALIGN {
    // this function asks the bot::game DLL to spawn (i.e, give a physical existence in the virtual
    // world, in other words to 'display') the entity pointed to by ent in the bot::game. The
    // Spawn() function is one of the functions any entity is supposed to have in the bot::game DLL,
    // and any MOD is supposed to implement one for each of its entities

    // precache everything
    bot::game.Precache ();

    // notify about entity spawn
    bot::game.OnSpawnEntity (ent);

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META_VALUE (MRES_IGNORED, 0);
    }
    int result = dllapi.pfnSpawn (ent); // get result

    if (ent->v.rendermode == kRenderTransTexture) {
      ent->v.flags &= ~FL_WORLDBRUSH; // clear the FL_WORLDBRUSH flag out of transparent ents
    }
    return result;
  };

  table->pfnTouch = [] (edict_t *pent_touched, edict_t *pent_other) YSTL_FORCE_STACK_ALIGN {
    // this function is called when two entities' bounding boxes enter in collision. For example,
    // when a player walks upon a gun, the player entity bounding box collides to the gun entity
    // bounding box, and the result is that this function is called. It is used by the bot::game for
    // taking the appropriate action when such an event occurs (in our example, the player who
    // is walking upon the gun will "pick it up"). Entities that "touch" others are usually
    // entities having a velocity, as it is assumed that static entities (entities that don't
    // move) will never touch anything. Hence, in our example, the pentTouched will be the gun
    // (static entity), whereas the pentOther will be the player (as it is the one moving). When
    // the two entities both have velocities, for example two players colliding, this function
    // is called twice, once for each entity moving

    if (bot::game.HasBreakables () && !bot::game.IsNullEntity (pent_touched) && pent_other != bot::game.GetStartEntity ()) {

      auto bot = bot::bots[pent_touched];

      if (bot && bot::game.IsBreakableEntity (pent_other)) {
        bot->CheckBreakable (pent_other);
      }
    }

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    dllapi.pfnTouch (pent_touched, pent_other);
  };

  table->pfnClientConnect = [] (edict_t *ent, const char *name, const char *addr, char reject_reason[128]) YSTL_FORCE_STACK_ALIGN {
    // this function is called in order to tell the MOD DLL that a client attempts to connect the
    // bot::game. The entity pointer of this client is ent, the name under which he connects is
    // pointed to by the pszName pointer, and its IP address string is pointed by the pszAddress
    // one. Note that this does not mean this client will actually join the bot::game ; he could as
    // well be refused connection by the server later, because of latency timeout, unavailable
    // bot::game resources, or whatever reason. In which case the reason why the bot::game DLL (read well,
    // the bot::game DLL, *NOT* the engine) refuses this player to connect will be printed in the
    // rejectReason string in all letters. Understand that a client connecting process is done
    // in three steps. First, the client requests a connection from the server. This is engine
    // internals. When there are already too many players, the engine will refuse this client to
    // connect, and the bot::game DLL won't even notice. Second, if the engine sees no problem, the
    // bot::game DLL is asked. This is where we are. Once the bot::game DLL acknowledges the connection,
    // the client downloads the resources it needs and synchronizes its local engine with the one
    // of the server. And then, the third step, which comes *AFTER* ClientConnect (), is when the
    // client officially enters the bot::game, through the ClientPutInServer () function, later below
    // Here we hook this function in order to keep track of the listen server client entity,
    // because a listen server client always connects with a "loopback" address string. Also we
    // tell the bot manager to check the bot population, in order to always have one free slot on
    // the server for incoming bot::clients

    // check if this client is the listen server client
    if (ystl::StringRef (addr) == "loopback") {
      bot::game.SetLocalEntity (ent); // save the edict of the listen server client

      // if not dedicated set the default editor for bot::graph
      if (!bot::game.IsDedicatedServer ()) {
        bot::graph.SetEditor (ent);
      }
    }

    // refresh bot::clients immediately
    bot::clients.Update ();

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META_VALUE (MRES_IGNORED, 0);
    }
    return dllapi.pfnClientConnect (ent, name, addr, reject_reason);
  };

  table->pfnClientDisconnect = [] (edict_t *ent) YSTL_FORCE_STACK_ALIGN {
    // this function is called whenever a client is VOLUNTARILY disconnected from the server,
    // either because the client dropped the connection, or because the server dropped him from
    // the bot::game (latency timeout). The effect is the freeing of a client slot on the server. Note
    // that bot::clients and bot::bots disconnected because of a level change NOT NECESSARILY call this
    // function, because in case of a level change, it's a server shutdown, and not a normal
    // disconnection. I find that completely stupid, but that's it. Anyway it's time to update
    // the bot::bots and players counts, and in case the client disconnecting is a bot, to back its
    // brain(s) up to disk. We also try to notice when a listenserver client disconnects, so as
    // to reset his entity pointer for safety. There are still a few server frames to go once a
    // listen server client disconnects, and we don't want to send him any sort of message then

    for (auto &bot : bot::bots) {
      if (bot.pev == &ent->v) {
        bot::bots.DisconnectBot (&bot); // remove the bot from bot::bots array

        break;
      }
    }

    // refresh bot::clients immediately
    bot::clients.Update ();

    // clear the bot::graph editor upon disconnect
    if (ent == bot::graph.GetEditor ()) {
      bot::graph.SetEditor (nullptr);
    }

    // clear issuer for the menus and commands
    if (ent == bot::ctrl.GetIssuer ()) {
      bot::ctrl.SetIssuer (nullptr);
    }

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    dllapi.pfnClientDisconnect (ent);
  };

  table->pfnClientPutInServer = [] (edict_t *ent) YSTL_FORCE_STACK_ALIGN {
    // this function is called once a just connected client actually enters the bot::game, after
    // having downloaded and synchronized its resources with the of the server's. It's the
    // perfect place to hook for client connecting, since a client can always try to connect
    // passing the ClientConnect() step, and not be allowed by the server later (because of a
    // latency timeout or whatever reason). We can here keep track of both bot::bots and players
    // counts on occurence, since bot::bots connect the server just like the way normal client do,
    // and their third party bot flag is already supposed to be set then. If it's a bot which
    // is connecting, we also have to awake its brain(s) by reading them from the disk

    // refresh pings when client connetcs
    if (bot::fakeping.HasFeature () && !bot::game.IsFakeClientEntity (ent)) {
      bot::fakeping.Emit (ent);
    }

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    dllapi.pfnClientPutInServer (ent);
  };

  table->pfnClientUserInfoChanged = [] (edict_t *ent, char *infobuffer) YSTL_FORCE_STACK_ALIGN {
    // this function is called when a player changes model, or changes team. Occasionally it
    // enforces rules on these changes (for example, some MODs don't want to allow players to
    // change their player model). But most commonly, this function is in charge of handling
    // team changes, recounting the teams population, etc

    bot::ctrl.AssignAdminRights (ent, infobuffer);
    bot::bots.CheckBotModel (ent, infobuffer);

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    dllapi.pfnClientUserInfoChanged (ent, infobuffer);
  };

  table->pfnClientCommand = [] (edict_t *ent) YSTL_FORCE_STACK_ALIGN {
    // this function is called whenever the client whose player entity is ent issues a client
    // command. How it works is that bot::clients all have a global string in their client DLL that
    // stores the command string; if ever that string is filled with characters, the client DLL
    // sends it to the engine as a command to be executed. When the engine has executed that
    // command, that string is reset to zero. By the server side, we can access this string
    // by asking the engine with the CmdArgv(), CmdArgs() and CmdArgc() functions that work just
    // like executable files argument processing work in C (argc gets the number of arguments,
    // command included, args returns the whole string, and argv returns the wanted argument
    // only). Here is a good place to set up either bot debug commands the listen server client
    // could type in his bot::game console, or real new client commands, but we wouldn't want to do
    // so as this is just a bot DLL, not a MOD. The purpose is not to add functionality to
    // bot::clients. Hence it can lack of commenting a bit, since this code is very subject to change

    if (bot::ctrl.HandleClientCommands (ent)) {
      if (bot::game.Is (bot::GameFlags::Metamod)) {
        RETURN_META (MRES_SUPERCEDE);
      }
      return;
    }

    else if (bot::ctrl.HandleMenuCommands (ent)) {
      if (bot::game.Is (bot::GameFlags::Metamod)) {
        RETURN_META (MRES_SUPERCEDE);
      }
      return;
    }

    // record stuff about radio and chat
    bot::bots.CaptureChatRadio (engfuncs.pfnCmd_Argv (0), engfuncs.pfnCmd_Argv (1), ent);

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    dllapi.pfnClientCommand (ent);
  };

  table->pfnServerActivate = [] (edict_t *edict_list, int edict_count, int client_max) YSTL_FORCE_STACK_ALIGN {
    // this function is called when the server has fully loaded and is about to manifest itself
    // on the network as such. Since a mapchange is actually a server shutdown followed by a
    // restart, this function is also called when a new map is being loaded. Hence it's the
    // perfect place for doing initialization stuff for our bot::bots, such as reading the BSP data,
    // loading the bot profiles, and drawing the world map (ie, filling the navigation hashtable)
    // Once this function has been called, the server can be considered as "running"

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    dllapi.pfnServerActivate (edict_list, edict_count, client_max);

    // do a level initialization
    bot::game.LevelInitialize (edict_list, edict_count);
  };

  table->pfnServerDeactivate = [] () YSTL_FORCE_STACK_ALIGN {
    // this function is called when the server is shutting down. A particular note about map
    // changes: changing the map means shutting down the server and starting a new one. Of course
    // this process is transparent to the user, but either in single player when the hero reaches
    // a new level and in multiplayer when it's time for a map change, be aware that what happens
    // is that the server actually shuts down and restarts with a new map. Hence we can use this
    // function to free and deinit anything which is map-specific, for example we free the memory
    // space we m'allocated for our BSP data, since a new map means new BSP data to interpret. In
    // any case, when the new map will be booting, ServerActivate() will be called, so we'll do
    // the loading of new bot::bots and the new BSP data parsing there

    // notify level shutdown
    bot::game.LevelShutdown ();

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    dllapi.pfnServerDeactivate ();
  };

  table->pfnStartFrame = [] () YSTL_FORCE_STACK_ALIGN {
    // this function starts a video frame. It is called once per video frame by the bot::game. If
    // you run Half-Life at 90 fps, this function will then be called 90 times per second. By
    // placing a hook on it, we have a good place to do things that should be done continuously
    // during the bot::game, for example making the bot::bots think (yes, because no Think() function exists
    // for the bot::bots by the MOD side, remember). Also here we have control on the bot population,
    // for example if a new player joins the server, we should disconnect a bot, and if the
    // player population decreases, we should fill the server with other bot::bots

    // full per-frame orchestration lives in game.Frame
    bot::game.Frame ();
  };

  if (bot::game.Is (bot::GameFlags::HasFakePings) && !bot::game.Is (bot::GameFlags::Metamod)) {
    table->pfnUpdateClientData = [] (const struct edict_s *player, int sendweapons, struct clientdata_s *cd) YSTL_FORCE_STACK_ALIGN {
      // this function is a synchronization tool that is used periodically by the engine to tell
      // the bot::game DLL to send player info over the network to one of its bot::clients when it suspects
      // that this client is desynchronizing. Early bot::bots were using it to ask the bot::game DLL for the
      // weapon list of players (by setting sendweapons to TRUE), but most of the time having a
      // look around the ent->v.weapons bitmask is enough, since that's the place commonly used for
      // MODs to store weapon information. If it can't be read from there, catching a few network
      // messages (like in DMC) do the job better than this function anyway

      dllapi.pfnUpdateClientData (player, sendweapons, cd);

      // do a post-processing with non-metamod
      auto ent = const_cast<edict_t *> (reinterpret_cast<const edict_t *> (player));

      if (bot::fakeping.HasFeature ()) {
        if (!bot::game.IsFakeClientEntity (ent) && ((ent->v.oldbuttons | ent->v.button) & IN_SCORE) != 0) {
          bot::fakeping.Emit (ent);
        }
      }
    };
  }

  // add some bullet spread on games, where w're runnung without metamod, but only if
  // yb_enable_bullet_spread is enabled, since randomizing the seed affects bot accuracy
  if (!bot::game.Is (bot::GameFlags::Metamod) && !bot::game.Is (bot::GameFlags::Legacy)) {
    table->pfnCmdStart = [] (const edict_t *player, usercmd_t *cmd, unsigned int random_seed) YSTL_FORCE_STACK_ALIGN {
      // some MODs don't feel like doing like everybody else. It's the case in DMC, where players
      // don't select their weapons using a simple client command, but have to use an horrible
      // datagram like this. CmdStart() marks the start of a network packet bot::clients send to the
      // server that holds a limited set of requests (see the usercmd_t structure for details)
      // It has been adapted for usage to HLTV spectators, who don't send ClientCommands, but send
      // all their update information to the server using usercmd's instead, it seems

      if (bot::cv_enable_bullet_spread && bot::bots[const_cast<edict_t *> (player)]) {
        random_seed = static_cast<uint32_t> (ystl::rg (0, 0x7fffffff));
      }
      dllapi.pfnCmdStart (player, cmd, random_seed);
    };
  }

  table->pfnPM_Move = [] (playermove_t *pm, int server) YSTL_FORCE_STACK_ALIGN {
    // this is the player movement code bot::clients run to predict things when the server can't update
    // them often enough (or doesn't want to). The server runs exactly the same function for
    // moving players. There is normally no distinction between them, else client-side prediction
    // wouldn't work properly (and it doesn't work that well, already...)

    bot::illum.SetWorldModel (pm->physents[0].model);

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    dllapi.pfnPM_Move (pm, server);
  };

  table->pfnKeyValue = [] (edict_t *ent, KeyValueData *kvd) YSTL_FORCE_STACK_ALIGN {
    // this function is called when the bot::game requests a pointer to some entity's keyvalue data
    // The keyvalue data is held in each entity's infobuffer (basically a char buffer where each
    // bot::game DLL can put the stuff it wants) under - as it says - the form of a key/value pair. A
    // common example of key/value pair is the "model", "(name of player model here)" one which
    // is often used for client DLLs to display player characters with the right model (else they
    // would all have the dull "models/player.mdl" one). The entity for which the keyvalue data
    // pointer is requested is pentKeyvalue, the pointer to the keyvalue data structure pkvd

    if (!bot::game.IsNullEntity (ent) && ystl::StringRef (ent->v.classname.chars ()) == "func_breakable") {
      if (kvd && kvd->szKeyName && ystl::StringRef (kvd->szKeyName) == "material") {
        if (kvd->szValue && atoi (kvd->szValue) == 7) {
          bot::game.MarkBreakableAsInvalid (ent);
        }
      }
    }

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    dllapi.pfnKeyValue (ent, kvd);
  };
  return HLTrue;
}

YSTL_C_LINKAGE int GetEntityApiPost (gamefuncs_t *table, int) {
  // this function is called right after GiveFnptrsToDll() by the engine in the bot::game DLL (or
  // what it BELIEVES to be the bot::game DLL), in order to copy the list of MOD functions that can
  // be called by the engine, into a memory block pointed to by the functionTable pointer
  // that is passed into this function (explanation comes straight from botman). This allows
  // the Half-Life engine to call these MOD DLL functions when it needs to spawn an entity,
  // connect or disconnect a player, call Think() functions, Touch() functions, or Use()
  // functions, etc. The bot DLL passes its OWN list of these functions back to the Half-Life
  // engine, and then calls the MOD DLL's version of GetEntityAPI to get the REAL gamedll
  // functions this time (to use in the bot code). Post version, called only by metamod

  ystl::memzero (table, sizeof (gamefuncs_t));

  table->pfnSpawn = [] (edict_t *ent) YSTL_FORCE_STACK_ALIGN {
    // this function asks the bot::game DLL to spawn (i.e, give a physical existence in the virtual
    // world, in other words to 'display') the entity pointed to by ent in the bot::game. The
    // Spawn() function is one of the functions any entity is supposed to have in the bot::game DLL,
    // and any MOD is supposed to implement one for each of its entities. Post version called
    // only by metamod

    // solves the bot::bots unable to see through certain types of glass bug
    if (ent->v.rendermode == kRenderTransTexture) {
      ent->v.flags &= ~FL_WORLDBRUSH; // clear the FL_WORLDBRUSH flag out of transparent ents
    }
    RETURN_META_VALUE (MRES_HANDLED, 0);
  };

  table->pfnStartFrame = [] () YSTL_FORCE_STACK_ALIGN {
    // this function starts a video frame. It is called once per video frame by the bot::game. If
    // you run Half-Life at 90 fps, this function will then be called 90 times per second. By
    // placing a hook on it, we have a good place to do things that should be done continuously
    // during the bot::game, for example making the bot::bots think (yes, because no Think() function exists
    // for the bot::bots by the MOD side, remember).  Post version called only by metamod

    // run the bot ai
    bot::bots.Frame ();

    // refresh bot::clients after bot ai, so bot input is fresh for the noise simulation
    bot::clients.Update ();

    RETURN_META (MRES_IGNORED);
  };

  table->pfnServerActivate = [] (edict_t *edict_list, int edict_count, int) YSTL_FORCE_STACK_ALIGN {
    // this function is called when the server has fully loaded and is about to manifest itself
    // on the network as such. Since a mapchange is actually a server shutdown followed by a
    // restart, this function is also called when a new map is being loaded. Hence it's the
    // perfect place for doing initialization stuff for our bot::bots, such as reading the BSP data,
    // loading the bot profiles, and drawing the world map (ie, filling the navigation hashtable)
    // Once this function has been called, the server can be considered as "running". Post version
    // called only by metamod

    // do a level initialization
    bot::game.LevelInitialize (edict_list, edict_count);

    RETURN_META (MRES_IGNORED);
  };

  if (bot::game.Is (bot::GameFlags::HasFakePings)) {
    table->pfnUpdateClientData = [] (const struct edict_s *player, int, struct clientdata_s *) YSTL_FORCE_STACK_ALIGN {
      // this function is a synchronization tool that is used periodically by the engine to tell
      // the bot::game DLL to send player info over the network to one of its bot::clients when it suspects
      // that this client is desynchronizing. Early bot::bots were using it to ask the bot::game DLL for the
      // weapon list of players (by setting sendweapons to TRUE), but most of the time having a
      // look around the ent->v.weapons bitmask is enough, since that's the place commonly used for
      // MODs to store weapon information. If it can't be read from there, catching a few network
      // messages (like in DMC) do the job better than this function anyway
      //
      // do a post-processing with non-metamod
      auto ent = const_cast<edict_t *> (reinterpret_cast<const edict_t *> (player));

      if (bot::fakeping.HasFeature ()) {
        if (!bot::game.IsFakeClientEntity (ent) && ((ent->v.oldbuttons | ent->v.button) & IN_SCORE) != 0) {
          bot::fakeping.Emit (ent);
        }
      }
      RETURN_META (MRES_IGNORED);
    };
  }

  return HLTrue;
}

YSTL_C_LINKAGE int GetEngineFunctions (enginefuncs_t *table, int *) {
  if (bot::game.Is (bot::GameFlags::Metamod)) {
    ystl::memzero (table, sizeof (enginefuncs_t));
  }

  if (bot::entlink.NeedsBypass () && !bot::game.Is (bot::GameFlags::Metamod)) {
    table->pfnCreateNamedEntity = [] (string_t classname) YSTL_FORCE_STACK_ALIGN {
      if (bot::entlink.IsPaused ()) {
        bot::entlink.Enable ();
        bot::entlink.SetPaused (false);
      }
      return engfuncs.pfnCreateNamedEntity (classname);
    };
  }

  if (bot::game.Is (bot::GameFlags::Legacy)) {
    table->pfnFindEntityByString = [] (edict_t *edict_start_search_after, const char *field, const char *value) YSTL_FORCE_STACK_ALIGN {
      // round starts in counter-strike 1.5
      if (ystl::StringRef (value) == "info_map_parameters") {
        bot::game_state.RoundStart ();
      }

      if (bot::game.Is (bot::GameFlags::Metamod)) {
        RETURN_META_VALUE (MRES_IGNORED, static_cast<edict_t *> (nullptr));
      }
      return engfuncs.pfnFindEntityByString (edict_start_search_after, field, value);
    };

    table->pfnChangeLevel = [] (char *s1, char *s2) YSTL_FORCE_STACK_ALIGN {
      // this function gets called when server is changing a level

      // kick off all the bot::bots, needed for legacy engine versions
      bot::bots.KickEveryone (true, false);

      // save the bot::practice data
      bot::practice.Save ();

      if (bot::game.Is (bot::GameFlags::Metamod)) {
        RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnChangeLevel (s1, s2);
    };
  }

  if (!bot::game.Is (bot::GameFlags::Legacy)) {
    table->pfnLightStyle = [] (int style, char *val) YSTL_FORCE_STACK_ALIGN {
      // this function update lightstyle for the bot::bots

      bot::illum.UpdateLight (style, val);

      if (bot::game.Is (bot::GameFlags::Metamod)) {
        RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnLightStyle (style, val);
    };

    table->pfnGetPlayerAuthId = [] (edict_t *e) YSTL_FORCE_STACK_ALIGN {
      if (bot::bots[e]) {
        auto authid = bot::util.GetFakeSteamId (e);

        if (bot::game.Is (bot::GameFlags::Metamod)) {
          RETURN_META_VALUE (MRES_SUPERCEDE, authid.chars ());
        }
        return authid.chars ();
      }

      if (bot::game.Is (bot::GameFlags::Metamod)) {
        RETURN_META_VALUE (MRES_IGNORED, "");
      }
      return engfuncs.pfnGetPlayerAuthId (e);
    };
  }

  // clang-format off
   table->pfnEmitSound = [] (edict_t *entity, int channel,
      const char *sample, float volume, float attenuation, int flags, int pitch) YSTL_FORCE_STACK_ALIGN {

      // this function tells the engine that the entity pointed to by "entity", is emitting a sound
      // which fileName is "sample", at level "channel" (CHAN_VOICE, etc...), with "volume" as
      // loudness multiplicator (normal volume VOL_NORM is 1.0), with a pitch of "pitch" (normal
      // pitch PITCH_NORM is 100.0), and that this sound has to be attenuated by distance in air
      // according to the value of "attenuation" (normal attenuation ATTN_NORM is 0.8 ; ATTN_NONE
      // means no attenuation with distance). Optionally flags "fFlags" can be passed, which I don't
      // know the heck of the purpose. After we tell the engine to emit the sound, we have to call
      // SoundAttachToThreat() to bring the sound to the ears of the bot::bots. Since bot::bots have no client DLL
      // to handle this for them, such a job has to be done manually

       bot::sounds.Acquire (entity, sample, volume, attenuation);

      if (bot::game.Is (bot::GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnEmitSound (entity, channel, sample, volume, attenuation, flags, pitch);
   };
  // clang-format on

  table->pfnMessageBegin = [] (int msg_dest, int msg_type, const float *origin, edict_t *ed) YSTL_FORCE_STACK_ALIGN {
    // this function called each time a message is about to sent
    bot::msgs.Start (ed, msg_type);

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    engfuncs.pfnMessageBegin (msg_dest, msg_type, origin, ed);
  };

  if (!bot::game.Is (bot::GameFlags::Metamod)) {
    table->pfnMessageEnd = [] () YSTL_FORCE_STACK_ALIGN {
      engfuncs.pfnMessageEnd ();

      // this allows us to send messages right in handler code
      bot::msgs.Stop ();
    };
  }

  table->pfnWriteByte = [] (int value) YSTL_FORCE_STACK_ALIGN {
    // if this message is for a bot, call the client message function
    bot::msgs.Collect (value);

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    engfuncs.pfnWriteByte (value);
  };

  table->pfnWriteChar = [] (int value) YSTL_FORCE_STACK_ALIGN {
    // if this message is for a bot, call the client message function
    bot::msgs.Collect (value);

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    engfuncs.pfnWriteChar (value);
  };

  table->pfnWriteShort = [] (int value) YSTL_FORCE_STACK_ALIGN {
    // if this message is for a bot, call the client message function
    bot::msgs.Collect (value);

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    engfuncs.pfnWriteShort (value);
  };

  table->pfnWriteLong = [] (int value) YSTL_FORCE_STACK_ALIGN {
    // if this message is for a bot, call the client message function
    bot::msgs.Collect (value);

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    engfuncs.pfnWriteLong (value);
  };

  table->pfnWriteAngle = [] (float value) YSTL_FORCE_STACK_ALIGN {
    // if this message is for a bot, call the client message function
    bot::msgs.Collect (value);

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    engfuncs.pfnWriteAngle (value);
  };

  table->pfnWriteCoord = [] (float value) YSTL_FORCE_STACK_ALIGN {
    // if this message is for a bot, call the client message function
    bot::msgs.Collect (value);

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    engfuncs.pfnWriteCoord (value);
  };

  table->pfnWriteString = [] (const char *sz) YSTL_FORCE_STACK_ALIGN {
    // if this message is for a bot, call the client message function
    bot::msgs.Collect (sz);

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    engfuncs.pfnWriteString (sz);
  };

  table->pfnWriteEntity = [] (int value) YSTL_FORCE_STACK_ALIGN {
    // if this message is for a bot, call the client message function
    bot::msgs.Collect (value);

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    engfuncs.pfnWriteEntity (value);
  };

  // very ancient engine versions (pre 2xxx builds) needs this to work correctly
  table->pfnClientCommand = Hooks::HandlerEngClientCommand;

  if (!bot::game.Is (bot::GameFlags::Metamod)) {
    table->pfnRegUserMsg = [] (const char *name, int size) YSTL_FORCE_STACK_ALIGN {
      // this function registers a "user message" by the engine side. User messages are network
      // messages the bot::game DLL asks the engine to send to bot::clients. Since many MODs have completely
      // different client features (Counter-Strike has a radar and a timer, for example), network
      // messages just can't be the same for every MOD. Hence here the MOD DLL tells the engine,
      // "Hey, you have to know that I use a network message whose name is pszName and it is size
      // packets long". The engine books it, and returns the ID number under which he recorded that
      // custom message. Thus every time the MOD DLL will be wanting to send a message named pszName
      // using pfnMessageBegin (), it will know what message ID number to send, and the engine will
      // know what to do, only for non-metamod version

      return bot::msgs.Add (name, engfuncs.pfnRegUserMsg (name, size)); // return previously registered message
    };
  }

  table->pfnClientPrintf = [] (edict_t *ent, PRINT_TYPE print_type, const char *message) YSTL_FORCE_STACK_ALIGN {
    // this function prints the text message string pointed to by message by the client side of
    // the client entity pointed to by ent, in a manner depending of printType (print_console,
    // print_center or print_chat). Be certain never to try to feed a bot with this function,
    // as it will crash your server. Why would you, anyway ? bot::bots have no client DLL as far as
    // we know, right ? But since stupidity rules this world, we do a preventive check :)

    if (bot::game.IsFakeClientEntity (ent)) {
      if (bot::game.Is (bot::GameFlags::Metamod)) {
        RETURN_META (MRES_SUPERCEDE);
      }
      return;
    }

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    engfuncs.pfnClientPrintf (ent, print_type, message);
  };

  table->pfnCmd_Args = [] () YSTL_FORCE_STACK_ALIGN {
    // this function returns a pointer to the whole current client command string. Since bot::bots
    // have no client DLL and we may want a bot to execute a client command, we had to implement
    // a argv string in the bot DLL for holding the bot::bots' commands, and also keep track of the
    // argument count. Hence this hook not to let the engine ask an unexistent client DLL for a
    // command we are holding here. Of course, real bot::clients commands are still retrieved the
    // normal way, by asking the bot::game

    // is this a bot issuing that client command?
    if (bot::game.IsBotCmd ()) {
      if (bot::game.Is (bot::GameFlags::Metamod)) {
        RETURN_META_VALUE (MRES_SUPERCEDE, bot::game.BotArgs ());
      }
      return bot::game.BotArgs (); // else return the whole bot client command string we know
    }

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META_VALUE (MRES_IGNORED, static_cast<const char *> (nullptr));
    }
    return engfuncs.pfnCmd_Args (); // ask the client command string to the engine
  };

  table->pfnCmd_Argv = [] (int argc) YSTL_FORCE_STACK_ALIGN {
    // this function returns a pointer to a certain argument of the current client command. Since
    // bot::bots have no client DLL and we may want a bot to execute a client command, we had to
    // implement a argv string in the bot DLL for holding the bot::bots' commands, and also keep
    // track of the argument count. Hence this hook not to let the engine ask an unexistent client
    // DLL for a command we are holding here. Of course, real bot::clients commands are still retrieved
    // the normal way, by asking the bot::game

    // is this a bot issuing that client command?
    if (bot::game.IsBotCmd ()) {
      if (bot::game.Is (bot::GameFlags::Metamod)) {
        RETURN_META_VALUE (MRES_SUPERCEDE, bot::game.BotArgv (argc));
      }
      return bot::game.BotArgv (argc); // if so, then return the wanted argument we know
    }

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META_VALUE (MRES_IGNORED, static_cast<const char *> (nullptr));
    }
    return engfuncs.pfnCmd_Argv (argc); // ask the argument number "argc" to the engine
  };

  table->pfnCmd_Argc = [] () YSTL_FORCE_STACK_ALIGN {
    // this function returns the number of arguments the current client command string has. Since
    // bot::bots have no client DLL and we may want a bot to execute a client command, we had to
    // implement a argv string in the bot DLL for holding the bot::bots' commands, and also keep
    // track of the argument count. Hence this hook not to let the engine ask an unexistent client
    // DLL for a command we are holding here. Of course, real bot::clients commands are still retrieved
    // the normal way, by asking the bot::game

    // is this a bot issuing that client command?
    if (bot::game.IsBotCmd ()) {
      if (bot::game.Is (bot::GameFlags::Metamod)) {
        RETURN_META_VALUE (MRES_SUPERCEDE, bot::game.BotArgc ());
      }
      return bot::game.BotArgc (); // if so, then return the argument count we know
    }

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META_VALUE (MRES_IGNORED, 0);
    }
    return engfuncs.pfnCmd_Argc (); // ask the engine how many arguments there are
  };

  table->pfnSetClientMaxspeed = [] (const edict_t *ent, float new_maxspeed) YSTL_FORCE_STACK_ALIGN {
    auto bot = bot::bots[const_cast<edict_t *> (ent)];

    // check wether it's not a bot
    if (bot != nullptr) {
      bot->pev->maxspeed = new_maxspeed;
    }

    if (bot::game.Is (bot::GameFlags::Metamod)) {
      RETURN_META (MRES_IGNORED);
    }
    engfuncs.pfnSetClientMaxspeed (ent, new_maxspeed);
  };
  return HLTrue;
}

YSTL_EXPORT int GetNewDLLFunctions (newgamefuncs_t *table, int *interface_version) {
  // it appears that an extra function table has been added in the engine to gamedll interface
  // since the date where the first enginefuncs table standard was frozen. These ones are
  // facultative and we don't hook them, but since some MODs might be featuring it, we have to
  // pass them too, else the DLL interfacing wouldn't be complete and the bot::game possibly wouldn't
  // run properly

  ystl::memzero (table, sizeof (newgamefuncs_t));

  if (!bot::game.Is (bot::GameFlags::Metamod)) {
    auto api_get_new_dll_functions = bot::game.Lib ().resolve<decltype (&GetNewDLLFunctions)> (__func__);

    // pass other DLLs engine callbacks to function table
    if (!api_get_new_dll_functions || api_get_new_dll_functions (&newapi, interface_version) == 0) {
      ystl::logger.error ("Could not resolve symbol \"%s\" in the bot::game dll. Continuing...", __func__);

      return HLFalse;
    }
    dllfuncs.newapi_table = &newapi;
    memcpy (table, &newapi, sizeof (newgamefuncs_t));
  }

  if (!bot::game.Is (bot::GameFlags::Legacy)) {
    table->pfnOnFreeEntPrivateData = [] (edict_t *ent) YSTL_FORCE_STACK_ALIGN {
      for (auto &bot : bot::bots) {
        if (bot.enemy_ == ent) {
          bot.enemy_ = nullptr;
          bot.last_enemy_ = nullptr;
        }
        else if (bot.Ent () == ent) {
          bot.MarkStale ();
        }
      }

      if (bot::game.Is (bot::GameFlags::Metamod)) {
        RETURN_META (MRES_IGNORED);
      }
      newapi.pfnOnFreeEntPrivateData (ent);
    };
  }
  return HLTrue;
}

YSTL_C_LINKAGE int GetEngineFunctionsPost (enginefuncs_t *table, int *) {
  ystl::memzero (table, sizeof (enginefuncs_t));

  table->pfnMessageEnd = [] () YSTL_FORCE_STACK_ALIGN {
    bot::msgs.Stop (); // this allows us to send messages right in handler code

    RETURN_META (MRES_IGNORED);
  };

  table->pfnRegUserMsg = [] (const char *name, int) YSTL_FORCE_STACK_ALIGN {
    // this function registers a "user message" by the engine side. User messages are network
    // messages the bot::game DLL asks the engine to send to bot::clients. Since many MODs have completely
    // different client features (Counter-Strike has a radar and a timer, for example), network
    // messages just can't be the same for every MOD. Hence here the MOD DLL tells the engine,
    // "Hey, you have to know that I use a network message whose name is pszName and it is size
    // packets long". The engine books it, and returns the ID number under which he recorded that
    // custom message. Thus every time the MOD DLL will be wanting to send a message named pszName
    // using pfnMessageBegin (), it will know what message ID number to send, and the engine will
    // know what to do, only for non-metamod version

    // register message for our needs
    bot::msgs.Add (name, META_RESULT_ORIG_RET (int));

    RETURN_META_VALUE (MRES_IGNORED, 0);
  };
  return HLTrue;
}

YSTL_EXPORT int Meta_Query (char *ifvers, plugin_info_t **p_plug_info, mutil_funcs_t *p_meta_util_funcs) {
  // this function is the second function called by metamod in the plugin DLL. Its purpose
  // is for metamod to retrieve basic information about the plugin, such as its meta-interface
  // version, for ensuring compatibility with the current version of the running metamod

  gpMetaUtilFuncs = p_meta_util_funcs;
  *p_plug_info = &Plugin_info;

  // check for interface version compatibility
  if (ystl::StringRef (ifvers) != Plugin_info.ifvers) {
    auto mdll = ystl::String (ifvers).split (":");
    auto pdll = ystl::String (META_INTERFACE_VERSION).split (":");

    gpMetaUtilFuncs->pfnLogError (
      PLID, "%s: meta-interface version mismatch (metamod: %s, %s: %s)", Plugin_info.name, ifvers, Plugin_info.name, Plugin_info.ifvers);

    const auto mmajor = mdll[0].as<int> ();
    const auto mminor = mdll[1].as<int> ();
    const auto pmajor = pdll[0].as<int> ();
    const auto pminor = pdll[1].as<int> ();

    if (pmajor > mmajor || (pmajor == mmajor && pminor > mminor)) {
      gpMetaUtilFuncs->pfnLogError (PLID, "metamod version is too old for this plugin; update metamod");
      return HLFalse;
    }

    // if plugin has older major interface version, it's incompatible (update plugin)
    else if (pmajor < mmajor) {
      gpMetaUtilFuncs->pfnLogError (PLID, "metamod version is incompatible with this plugin; please find a newer version of this plugin");
      return HLFalse;
    }
  }
  return HLTrue; // tell metamod this plugin looks safe
}

YSTL_EXPORT int Meta_Attach (PLUG_LOADTIME now, metamod_funcs_t *function_table, meta_globals_t *p_m_globals, gamedll_funcs_t *p_gamedll_funcs) {
  // this function is called when metamod attempts to load the plugin. Since it's the place
  // where we can tell if the plugin will be allowed to run or not, we wait until here to make
  // our initialization stuff, like registering CVARs and dedicated server commands

  // metamod engine & dllapi function tables
  static constinit metamod_funcs_t metamod_function_table = {
    .pfnGetEntityAPI = GetEntityAPI,
    .pfnGetEntityAPI_Post = GetEntityApiPost,
    .pfnGetEntityAPI2 = nullptr,
    .pfnGetEntityAPI2_Post = nullptr,
    .pfnGetNewDLLFunctions = GetNewDLLFunctions,
    .pfnGetNewDLLFunctions_Post = nullptr,
    .pfnGetEngineFunctions = GetEngineFunctions,
    .pfnGetEngineFunctions_Post = GetEngineFunctionsPost,
  };

  if (now > Plugin_info.loadable) {
    gpMetaUtilFuncs->pfnLogError (PLID, "%s: plugin NOT attaching (can't load plugin right now)", Plugin_info.name);
    return HLFalse; // returning FALSE prevents metamod from attaching this plugin
  }

  // keep track of the pointers to engine function tables metamod gives us
  gpMetaGlobals = p_m_globals;
  memcpy (function_table, &metamod_function_table, sizeof (metamod_funcs_t));
  gpGamedllFuncs = p_gamedll_funcs;

  return HLTrue; // returning true enables metamod to attach this plugin
}

YSTL_EXPORT int Meta_Detach (PLUG_LOADTIME now, PL_UNLOAD_REASON reason) {
  // this function is called when metamod unloads the plugin. A basic check is made in order
  // to prevent unloading the plugin if its processing should not be interrupted

  if (now > Plugin_info.unloadable && reason != PNL_CMD_FORCED) {
    gpMetaUtilFuncs->pfnLogError (PLID, "%s: plugin NOT detaching (can't unload plugin right now)", Plugin_info.name);
    return HLFalse; // returning FALSE prevents metamod from unloading this plugin
  }
  // stop the bot::worker
  bot::worker.Shutdown ();

  // kick all bot::bots off this server
  bot::bots.KickEveryone (true);

  // save collected bot::practice on shutdown
  bot::practice.Save ();

  // disable hooks
  bot::fakequeries.Disable ();

  // make sure all stuff cleared
  bot::bots.Destroy ();

  return HLTrue;
}

YSTL_EXPORT void Meta_Init () {
  // this function is called by metamod, before any other interface functions. Purpose of this
  // function to give plugin a chance to determine is plugin running under metamod or not

  bot::game.AddGameFlag (bot::GameFlags::Metamod);
}

// games GiveFnptrsToDll is a bit tricky
#if defined(YSTL_WINDOWS)
  #if defined(YSTL_CXX_MSVC) || defined(YSTL_CXX_CLANG_CL)
    #if defined(YSTL_ARCH_X32)
      #pragma comment(linker, "/EXPORT:GiveFnptrsToDll=_GiveFnptrsToDll@8,@1")
    #endif
    #pragma comment(linker, "/SECTION:.data,RW")
  #endif
  #if defined(YSTL_CXX_MSVC) && !defined(YSTL_ARCH_X64)
    #define DLL_GIVEFNPTRSTODLL YSTL_C_LINKAGE void YSTL_STDCALL
  #elif defined(YSTL_CXX_CLANG) || defined(YSTL_CXX_GCC) || defined(YSTL_ARCH_X64)
    #define DLL_GIVEFNPTRSTODLL YSTL_EXPORT void YSTL_STDCALL
  #endif
#else
  #define DLL_GIVEFNPTRSTODLL YSTL_EXPORT void
#endif

DLL_GIVEFNPTRSTODLL GiveFnptrsToDll (enginefuncs_t *table, globalvars_t *glob) {
  // this is the very first function that is called in the bot::game DLL by the bot::game. Its purpose
  // is to set the functions interfacing up, by exchanging the functionTable function list
  // along with a pointer to the engine's global variables structure pGlobals, with the bot::game
  // DLL. We can there decide if we want to load the normal bot::game DLL just behind our bot DLL,
  // or any other bot::game DLL that is present, such as Will Day's metamod. Also, since there is
  // a known bug on Win32 platforms that prevent hook DLLs (such as our bot DLL) to be used in
  // single player games (because they don't export all the stuff they should), we may need to
  // build our own array of exported symbols from the actual bot::game DLL in order to use it as
  // such if necessary. Nothing really bot-related is done in this function. The actual bot
  // initialization stuff will be done later, when we'll be certain to have a multilayer bot::game

  // get the engine functions from the bot::game
  memcpy (&engfuncs, table, sizeof (enginefuncs_t));
  globals = glob;

  if (bot::game.Postload ()) {
    return;
  }
  auto api_give_fnptrs_to_dll = bot::game.Lib ().resolve<decltype (&GiveFnptrsToDll)> (__func__);

  if (!api_give_fnptrs_to_dll) {
    ystl::logger.fatal ("Could not resolve symbol \"%s\" in the bot::game dll.", __func__);
  }
  GetEngineFunctions (table, nullptr);

  // initialize dynamic linkents (no memory hacking with xash3d)
  if (!bot::game.Is (bot::GameFlags::Xash3D)) {
    bot::entlink.Initialize ();
  }

  // give the engine functions to the other DLL
  api_give_fnptrs_to_dll (table, glob);
}

YSTL_EXPORT int Server_GetBlendingInterface (
  int version, struct sv_blending_interface_s **ppinterface, struct engine_studio_api_s *pstudio, float *rotationmatrix, float *bonetransform) {
  // this function synchronizes the studio model animation blending interface (i.e, what parts
  // of the body move, which bones, which hitboxes and how) between the server and the bot::game DLL
  // some MODs can be using a different hitbox scheme than the standard one

  auto api_get_blending_interface = bot::game.Lib ().resolve<decltype (&Server_GetBlendingInterface)> (__func__);

  if (!api_get_blending_interface) {
    ystl::logger.error ("Could not resolve symbol \"%s\" in the bot::game dll. Continuing...", __func__);
    return HLFalse;
  }
  return api_get_blending_interface (version, ppinterface, pstudio, rotationmatrix, bonetransform);
}

YSTL_EXPORT int Server_GetPhysicsInterface (int version, server_physics_api_t *physics_api, physics_interface_t *table) {
  // this function handle the custom xash3d physics interface, that we're uses just for resolving
  // entities between bot::game and engine

  if (!table || !physics_api || version != SV_PHYSICS_INTERFACE_VERSION) {
    return HLFalse;
  }
  table->version = SV_PHYSICS_INTERFACE_VERSION;

  table->SV_CreateEntity = [] (edict_t *ent, const char *name) -> int {
    auto func = bot::game.Lib ().resolve<bot::EntityProto> (name); // lookup symbol in bot::game dll

    // found one in bot::game dll ?
    if (func) {
      func (&ent->v);
      return HLTrue;
    }
    return -1;
  };

  table->SV_PhysicsEntity = [] (edict_t *) -> int {
    return HLFalse;
  };
  return HLTrue;
}

// add linkents for android
#include "entities.cpp"

// override new/delete globally, need to be included in .cpp file
#include <ystl/memory_override.h>
