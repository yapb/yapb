//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#include <yapb.h>

ConVar yb_version ("yb_version", PRODUCT_VERSION, VT_READONLY);

gamefuncs_t dllapi;
enginefuncs_t engfuncs;
gamedll_funcs_t dllfuncs;

meta_globals_t *gpMetaGlobals = nullptr;
gamedll_funcs_t *gpGamedllFuncs = nullptr;
mutil_funcs_t *gpMetaUtilFuncs = nullptr;
globalvars_t *globals = nullptr;

// metamod plugin information
plugin_info_t Plugin_info = {
   META_INTERFACE_VERSION, // interface version
   PRODUCT_SHORT_NAME, // plugin name
   PRODUCT_VERSION, // plugin version
   PRODUCT_DATE, // date of creation
   PRODUCT_AUTHOR, // plugin author
   PRODUCT_URL, // plugin URL
   PRODUCT_LOGTAG, // plugin logtag
   PT_CHANGELEVEL, // when loadable
   PT_ANYTIME, // when unloadable
};

namespace VariadicCallbacks {
   void clientCommand (edict_t *ent, char const *format, ...) {
      // this function forces the client whose player entity is ent to issue a client command.
      // How it works is that clients all have a argv global string in their client DLL that
      // stores the command string; if ever that string is filled with characters, the client DLL
      // sends it to the engine as a command to be executed. When the engine has executed that
      // command, this argv string is reset to zero. Here is somehow a curious implementation of
      // ClientCommand: the engine sets the command it wants the client to issue in his argv, then
      // the client DLL sends it back to the engine, the engine receives it then executes the
      // command therein. Don't ask me why we need all this complicated crap. Anyhow since bots have
      // no client DLL, be certain never to call this function upon a bot entity, else it will just
      // make the server crash. Since hordes of uncautious, not to say stupid, programmers don't
      // even imagine some players on their servers could be bots, this check is performed less than
      // sometimes actually by their side, that's why we strongly recommend to check it here too. In
      // case it's a bot asking for a client command, we handle it like we do for bot commands

      va_list ap;
      char buffer[MAX_PRINT_BUFFER];

      va_start (ap, format);
      _vsnprintf (buffer, cr::bufsize (buffer), format, ap);
      va_end (ap);

      if (ent && (ent->v.flags & (FL_FAKECLIENT | FL_DORMANT))) {
         if (bots.getBot (ent)) {
            game.execBotCmd (ent, buffer);
         }

         if (game.is (GAME_METAMOD)) {
            RETURN_META (MRES_SUPERCEDE); // prevent bots to be forced to issue client commands
         }
         return;
      }

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnClientCommand (ent, buffer);
   }
}

SHARED_LIBRARAY_EXPORT int GetEntityAPI2 (gamefuncs_t *functionTable, int *) {
   // this function is called right after GiveFnptrsToDll() by the engine in the game DLL (or
   // what it BELIEVES to be the game DLL), in order to copy the list of MOD functions that can
   // be called by the engine, into a memory block pointed to by the functionTable pointer
   // that is passed into this function (explanation comes straight from botman). This allows
   // the Half-Life engine to call these MOD DLL functions when it needs to spawn an entity,
   // connect or disconnect a player, call Think() functions, Touch() functions, or Use()
   // functions, etc. The bot DLL passes its OWN list of these functions back to the Half-Life
   // engine, and then calls the MOD DLL's version of GetEntityAPI to get the REAL gamedll
   // functions this time (to use in the bot code).

   memset (functionTable, 0, sizeof (gamefuncs_t));

   if (!(game.is (GAME_METAMOD))) {
      auto api_GetEntityAPI = game.getLib ().resolve <int (*) (gamefuncs_t *, int)> ("GetEntityAPI");

      // pass other DLLs engine callbacks to function table...
      if (api_GetEntityAPI (&dllapi, INTERFACE_VERSION) == 0) {
         util.logEntry (true, LL_FATAL, "GetEntityAPI2: ERROR - Not Initialized.");
         return FALSE; // error initializing function table!!!
      }
      dllfuncs.dllapi_table = &dllapi;
      gpGamedllFuncs = &dllfuncs;

      memcpy (functionTable, &dllapi, sizeof (gamefuncs_t));
   }

   functionTable->pfnGameInit = [] (void) {
      // this function is a one-time call, and appears to be the second function called in the
      // DLL after GiveFntprsToDll() has been called. Its purpose is to tell the MOD DLL to
      // initialize the game before the engine actually hooks into it with its video frames and
      // clients connecting. Note that it is a different step than the *server* initialization.
      // This one is called once, and only once, when the game process boots up before the first
      // server is enabled. Here is a good place to do our own game session initialization, and
      // to register by the engine side the server commands we need to administrate our bots.

      conf.initWeapons ();

      // register server command(s)
      game.registerCmd ("yapb", BotControl::handleEngineCommands);
      game.registerCmd ("yb", BotControl::handleEngineCommands);

      // set correct version string
      yb_version.set (util.format ("%d.%d.%d", PRODUCT_VERSION_DWORD_INTERNAL, util.buildNumber ()));

      // execute main config
      conf.load (true);

      // register fake metamod command handler if we not! under mm
      if (!(game.is (GAME_METAMOD))) {
         game.registerCmd ("meta", [] (void) {
            game.print ("You're launched standalone version of yapb. Metamod is not installed or not enabled!");
            });
      }
      conf.adjustWeaponPrices ();

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnGameInit ();
   };

   functionTable->pfnSpawn = [] (edict_t *ent) {
      // this function asks the game DLL to spawn (i.e, give a physical existence in the virtual
      // world, in other words to 'display') the entity pointed to by ent in the game. The
      // Spawn() function is one of the functions any entity is supposed to have in the game DLL,
      // and any MOD is supposed to implement one for each of its entities.

      game.precache ();

      if (game.is (GAME_METAMOD)) {
         RETURN_META_VALUE (MRES_IGNORED, 0);
      }
      int result = dllapi.pfnSpawn (ent); // get result

      if (ent->v.rendermode == kRenderTransTexture) {
         ent->v.flags &= ~FL_WORLDBRUSH; // clear the FL_WORLDBRUSH flag out of transparent ents
      }
      return result;
   };

   functionTable->pfnTouch = [] (edict_t *pentTouched, edict_t *pentOther) {
      // this function is called when two entities' bounding boxes enter in collision. For example,
      // when a player walks upon a gun, the player entity bounding box collides to the gun entity
      // bounding box, and the result is that this function is called. It is used by the game for
      // taking the appropriate action when such an event occurs (in our example, the player who
      // is walking upon the gun will "pick it up"). Entities that "touch" others are usually
      // entities having a velocity, as it is assumed that static entities (entities that don't
      // move) will never touch anything. Hence, in our example, the pentTouched will be the gun
      // (static entity), whereas the pentOther will be the player (as it is the one moving). When
      // the two entities both have velocities, for example two players colliding, this function
      // is called twice, once for each entity moving.

      if (!game.isNullEntity (pentTouched) && (pentTouched->v.flags & FL_FAKECLIENT) && pentOther != game.getStartEntity ()) {
         Bot *bot = bots.getBot (pentTouched);

         if (bot != nullptr && pentOther != bot->ent ()) {

            if (util.isPlayer (pentOther) && util.isAlive (pentOther)) {
               bot->avoidIncomingPlayers (pentOther);
            }
            else {
               bot->processBreakables (pentOther);
            }
         }
      }

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnTouch (pentTouched, pentOther);
   };

   functionTable->pfnClientConnect = [] (edict_t *ent, const char *name, const char *addr, char rejectReason[128]) {
      // this function is called in order to tell the MOD DLL that a client attempts to connect the
      // game. The entity pointer of this client is ent, the name under which he connects is
      // pointed to by the pszName pointer, and its IP address string is pointed by the pszAddress
      // one. Note that this does not mean this client will actually join the game ; he could as
      // well be refused connection by the server later, because of latency timeout, unavailable
      // game resources, or whatever reason. In which case the reason why the game DLL (read well,
      // the game DLL, *NOT* the engine) refuses this player to connect will be printed in the
      // rejectReason string in all letters. Understand that a client connecting process is done
      // in three steps. First, the client requests a connection from the server. This is engine
      // internals. When there are already too many players, the engine will refuse this client to
      // connect, and the game DLL won't even notice. Second, if the engine sees no problem, the
      // game DLL is asked. This is where we are. Once the game DLL acknowledges the connection,
      // the client downloads the resources it needs and synchronizes its local engine with the one
      // of the server. And then, the third step, which comes *AFTER* ClientConnect (), is when the
      // client officially enters the game, through the ClientPutInServer () function, later below.
      // Here we hook this function in order to keep track of the listen server client entity,
      // because a listen server client always connects with a "loopback" address string. Also we
      // tell the bot manager to check the bot population, in order to always have one free slot on
      // the server for incoming clients.

      // check if this client is the listen server client
      if (strcmp (addr, "loopback") == 0) {
         game.setLocalEntity (ent); // save the edict of the listen server client...
      }

      if (game.is (GAME_METAMOD)) {
         RETURN_META_VALUE (MRES_IGNORED, 0);
      }
      return dllapi.pfnClientConnect (ent, name, addr, rejectReason);
   };

   functionTable->pfnClientDisconnect = [] (edict_t *ent) {
      // this function is called whenever a client is VOLUNTARILY disconnected from the server,
      // either because the client dropped the connection, or because the server dropped him from
      // the game (latency timeout). The effect is the freeing of a client slot on the server. Note
      // that clients and bots disconnected because of a level change NOT NECESSARILY call this
      // function, because in case of a level change, it's a server shutdown, and not a normal
      // disconnection. I find that completely stupid, but that's it. Anyway it's time to update
      // the bots and players counts, and in case the client disconnecting is a bot, to back its
      // brain(s) up to disk. We also try to notice when a listenserver client disconnects, so as
      // to reset his entity pointer for safety. There are still a few server frames to go once a
      // listen server client disconnects, and we don't want to send him any sort of message then.

      int index = game.indexOfEntity (ent) - 1;

      if (index >= 0 && index < MAX_ENGINE_PLAYERS) {
         auto bot = bots.getBot (index);

         // check if its a bot
         if (bot != nullptr && bot->pev == &ent->v) {
            bot->showChaterIcon (false);
            bots.destroy (index);
         }
      }
      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnClientDisconnect (ent);
   };

   functionTable->pfnClientUserInfoChanged = [] (edict_t  *ent, char *infobuffer) {
      // this function is called when a player changes model, or changes team. Occasionally it
      // enforces rules on these changes (for example, some MODs don't want to allow players to
      // change their player model). But most commonly, this function is in charge of handling
      // team changes, recounting the teams population, etc...

      ctrl.assignAdminRights (ent, infobuffer);

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnClientUserInfoChanged (ent, infobuffer);
   };

   functionTable->pfnClientCommand = [] (edict_t *ent) {
      // this function is called whenever the client whose player entity is ent issues a client
      // command. How it works is that clients all have a global string in their client DLL that
      // stores the command string; if ever that string is filled with characters, the client DLL
      // sends it to the engine as a command to be executed. When the engine has executed that
      // command, that string is reset to zero. By the server side, we can access this string
      // by asking the engine with the CmdArgv(), CmdArgs() and CmdArgc() functions that work just
      // like executable files argument processing work in C (argc gets the number of arguments,
      // command included, args returns the whole string, and argv returns the wanted argument
      // only). Here is a good place to set up either bot debug commands the listen server client
      // could type in his game console, or real new client commands, but we wouldn't want to do
      // so as this is just a bot DLL, not a MOD. The purpose is not to add functionality to
      // clients. Hence it can lack of commenting a bit, since this code is very subject to change.

      if (ctrl.handleClientCommands (ent)) {
         if (game.is (GAME_METAMOD)) {
            RETURN_META (MRES_IGNORED);
         }
         return;
      }

      else if (ctrl.handleMenuCommands (ent)) {
         if (game.is (GAME_METAMOD)) {
            RETURN_META (MRES_IGNORED);
         }
         return;
      }

      // record stuff about radio and chat
      bots.captureChatRadio (engfuncs.pfnCmd_Argv (0), engfuncs.pfnCmd_Argv (1), ent);

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnClientCommand (ent);
   };

   functionTable->pfnServerActivate = [] (edict_t *pentEdictList, int edictCount, int clientMax) {
      // this function is called when the server has fully loaded and is about to manifest itself
      // on the network as such. Since a mapchange is actually a server shutdown followed by a
      // restart, this function is also called when a new map is being loaded. Hence it's the
      // perfect place for doing initialization stuff for our bots, such as reading the BSP data,
      // loading the bot profiles, and drawing the world map (ie, filling the navigation hashtable).
      // Once this function has been called, the server can be considered as "running".

      bots.destroy ();
      conf.load (false); // initialize all config files

      // do a level initialization
      game.levelInitialize (pentEdictList, edictCount);

      // update worldmodel
      illum.resetWorldModel ();

      // do level initialization stuff here...
      waypoints.init ();
      waypoints.load ();

      // execute main config
      conf.load (true);

      if (File::exists (util.format ("%s/maps/%s_yapb.cfg", game.getModName (), game.getMapName ()))) {
         game.execCmd ("exec maps/%s_yapb.cfg", game.getMapName ());
         game.print ("Executing Map-Specific config file");
      }
      bots.initQuota ();

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnServerActivate (pentEdictList, edictCount, clientMax);

      waypoints.rebuildVisibility ();
   };

   functionTable->pfnServerDeactivate = [] (void) {
      // this function is called when the server is shutting down. A particular note about map
      // changes: changing the map means shutting down the server and starting a new one. Of course
      // this process is transparent to the user, but either in single player when the hero reaches
      // a new level and in multiplayer when it's time for a map change, be aware that what happens
      // is that the server actually shuts down and restarts with a new map. Hence we can use this
      // function to free and deinit anything which is map-specific, for example we free the memory
      // space we m'allocated for our BSP data, since a new map means new BSP data to interpret. In
      // any case, when the new map will be booting, ServerActivate() will be called, so we'll do
      // the loading of new bots and the new BSP data parsing there.

      // save collected experience on shutdown
      waypoints.saveExperience ();
      waypoints.saveVisibility ();

      // destroy global killer entity
      bots.destroyKillerEntity ();

      // set state to unprecached
      game.setUnprecached ();

      // enable lightstyle animations on level change
      illum.enableAnimation (true);

      // send message on new map
      util.setNeedForWelcome (false);

      // xash is not kicking fakeclients on changelevel
      if (game.is (GAME_XASH_ENGINE)) {
         bots.kickEveryone (true, false);
         bots.destroy ();
      }
      waypoints.init ();

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnServerDeactivate ();
   };

   functionTable->pfnStartFrame = [] (void) {
      // this function starts a video frame. It is called once per video frame by the game. If
      // you run Half-Life at 90 fps, this function will then be called 90 times per second. By
      // placing a hook on it, we have a good place to do things that should be done continuously
      // during the game, for example making the bots think (yes, because no Think() function exists
      // for the bots by the MOD side, remember). Also here we have control on the bot population,
      // for example if a new player joins the server, we should disconnect a bot, and if the
      // player population decreases, we should fill the server with other bots.

      // run periodic update of bot states
      bots.frame ();

      // update lightstyle animations
      illum.animateLight ();

      // update some stats for clients
      util.updateClients ();

      if (waypoints.hasEditFlag (WS_EDIT_ENABLED) && !game.isDedicated () && !game.isNullEntity (game.getLocalEntity ())) {
         waypoints.frame ();
      }
      bots.updateDeathMsgState (false);

      // run stuff periodically
      game.slowFrame ();

      if (bots.getBotCount () > 0) {
         // keep track of grenades on map
         bots.updateActiveGrenade ();

         // keep track of intresting entities
         bots.updateIntrestingEntities ();
      }

      // keep bot number up to date
      bots.maintainQuota ();

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnStartFrame ();

      // run the bot ai
      bots.slowFrame ();
   };

   functionTable->pfnUpdateClientData = [] (const struct edict_s *ent, int sendweapons, struct clientdata_s *cd) {
      extern ConVar yb_latency_display;

      if (game.is (GAME_SUPPORT_SVC_PINGS) && yb_latency_display.integer () == 2) {
         bots.sendPingOffsets (const_cast <edict_t *> (ent));
      }

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnUpdateClientData (ent, sendweapons, cd);
   };

   functionTable->pfnPM_Move = [] (playermove_t *playerMove, int server) {
      // this is the player movement code clients run to predict things when the server can't update
      // them often enough (or doesn't want to). The server runs exactly the same function for
      // moving players. There is normally no distinction between them, else client-side prediction
      // wouldn't work properly (and it doesn't work that well, already...)

      illum.setWorldModel (playerMove->physents[0].model);

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnPM_Move (playerMove, server);
   };
   return TRUE;
}

SHARED_LIBRARAY_EXPORT int GetEntityAPI2_Post (gamefuncs_t *functionTable, int *) {
   // this function is called right after FuncPointers_t() by the engine in the game DLL (or
   // what it BELIEVES to be the game DLL), in order to copy the list of MOD functions that can
   // be called by the engine, into a memory block pointed to by the functionTable pointer
   // that is passed into this function (explanation comes straight from botman). This allows
   // the Half-Life engine to call these MOD DLL functions when it needs to spawn an entity,
   // connect or disconnect a player, call Think() functions, Touch() functions, or Use()
   // functions, etc. The bot DLL passes its OWN list of these functions back to the Half-Life
   // engine, and then calls the MOD DLL's version of GetEntityAPI to get the REAL gamedll
   // functions this time (to use in the bot code). Post version, called only by metamod.

   memset (functionTable, 0, sizeof (gamefuncs_t));

   functionTable->pfnSpawn = [] (edict_t *ent) {
      // this function asks the game DLL to spawn (i.e, give a physical existence in the virtual
      // world, in other words to 'display') the entity pointed to by ent in the game. The
      // Spawn() function is one of the functions any entity is supposed to have in the game DLL,
      // and any MOD is supposed to implement one for each of its entities. Post version called
      // only by metamod.

      // solves the bots unable to see through certain types of glass bug.
      if (ent->v.rendermode == kRenderTransTexture) {
         ent->v.flags &= ~FL_WORLDBRUSH; // clear the FL_WORLDBRUSH flag out of transparent ents
      }
      RETURN_META_VALUE (MRES_IGNORED, 0);
   };

   functionTable->pfnStartFrame = [] (void) {
      // this function starts a video frame. It is called once per video frame by the game. If
      // you run Half-Life at 90 fps, this function will then be called 90 times per second. By
      // placing a hook on it, we have a good place to do things that should be done continuously
      // during the game, for example making the bots think (yes, because no Think() function exists
      // for the bots by the MOD side, remember).  Post version called only by metamod.

      // run the bot ai
      bots.slowFrame ();
      RETURN_META (MRES_IGNORED);
   };

   functionTable->pfnServerActivate = [] (edict_t *, int, int) {
      // this function is called when the server has fully loaded and is about to manifest itself
      // on the network as such. Since a mapchange is actually a server shutdown followed by a
      // restart, this function is also called when a new map is being loaded. Hence it's the
      // perfect place for doing initialization stuff for our bots, such as reading the BSP data,
      // loading the bot profiles, and drawing the world map (ie, filling the navigation hashtable).
      // Once this function has been called, the server can be considered as "running". Post version
      // called only by metamod.

      waypoints.rebuildVisibility ();

      RETURN_META (MRES_IGNORED);
   };

   return TRUE;
}

SHARED_LIBRARAY_EXPORT int GetNewDLLFunctions (newgamefuncs_t *functionTable, int *interfaceVersion) {
   // it appears that an extra function table has been added in the engine to gamedll interface
   // since the date where the first enginefuncs table standard was frozen. These ones are
   // facultative and we don't hook them, but since some MODs might be featuring it, we have to
   // pass them too, else the DLL interfacing wouldn't be complete and the game possibly wouldn't
   // run properly.

   auto api_GetNewDLLFunctions = game.getLib ().resolve <int (*) (newgamefuncs_t *, int *)> ("GetNewDLLFunctions");

   if (api_GetNewDLLFunctions == nullptr) {
      return FALSE;
   }

   if (!api_GetNewDLLFunctions (functionTable, interfaceVersion)) {
      util.logEntry (true, LL_ERROR, "GetNewDLLFunctions: ERROR - Not Initialized.");
      return FALSE;
   }

   dllfuncs.newapi_table = functionTable;
   return TRUE;
}

SHARED_LIBRARAY_EXPORT int GetEngineFunctions (enginefuncs_t *functionTable, int *) {
   if (game.is (GAME_METAMOD)) {
      memset (functionTable, 0, sizeof (enginefuncs_t));
   }

   functionTable->pfnChangeLevel = [] (char *s1, char *s2) {
      // the purpose of this function is to ask the engine to shutdown the server and restart a
      // new one running the map whose name is s1. It is used ONLY IN SINGLE PLAYER MODE and is
      // transparent to the user, because it saves the player state and equipment and restores it
      // back in the new level. The "changelevel trigger point" in the old level is linked to the
      // new level's spawn point using the s2 string, which is formatted as follows: "trigger_name
      // to spawnpoint_name", without spaces (for example, "tr_1atotr_2lm" would tell the engine
      // the player has reached the trigger point "tr_1a" and has to spawn in the next level on the
      // spawn point named "tr_2lm".

      // save collected experience on map change
      waypoints.saveExperience ();
      waypoints.saveVisibility ();

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnChangeLevel (s1, s2);
   };

   functionTable->pfnFindEntityByString = [] (edict_t *edictStartSearchAfter, const char *field, const char *value) {
      // round starts in counter-strike 1.5
      if ((game.is (GAME_LEGACY)) && strcmp (value, "info_map_parameters") == 0) {
         bots.initRound ();
      }

      if (game.is (GAME_METAMOD)) {
         RETURN_META_VALUE (MRES_IGNORED, static_cast <edict_t *> (nullptr));
      }
      return engfuncs.pfnFindEntityByString (edictStartSearchAfter, field, value);
   };

   functionTable->pfnEmitSound = [] (edict_t *entity, int channel, const char *sample, float volume, float attenuation, int flags, int pitch) {
      // this function tells the engine that the entity pointed to by "entity", is emitting a sound
      // which fileName is "sample", at level "channel" (CHAN_VOICE, etc...), with "volume" as
      // loudness multiplicator (normal volume VOL_NORM is 1.0), with a pitch of "pitch" (normal
      // pitch PITCH_NORM is 100.0), and that this sound has to be attenuated by distance in air
      // according to the value of "attenuation" (normal attenuation ATTN_NORM is 0.8 ; ATTN_NONE
      // means no attenuation with distance). Optionally flags "fFlags" can be passed, which I don't
      // know the heck of the purpose. After we tell the engine to emit the sound, we have to call
      // SoundAttachToThreat() to bring the sound to the ears of the bots. Since bots have no client DLL
      // to handle this for them, such a job has to be done manually.

      util.attachSoundsToClients (entity, sample, volume);

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnEmitSound (entity, channel, sample, volume, attenuation, flags, pitch);
   };

   functionTable->pfnMessageBegin = [] (int msgDest, int msgType, const float *origin, edict_t *ed) {
      // this function called each time a message is about to sent.

      game.beginMessage (ed, msgDest, msgType);

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnMessageBegin (msgDest, msgType, origin, ed);
   };

   functionTable->pfnMessageEnd = [] (void) {
      game.resetMessages ();

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnMessageEnd ();

      // send latency fix
      bots.sendDeathMsgFix ();
   };

   functionTable->pfnWriteByte = [] (int value) {
      // if this message is for a bot, call the client message function...
      game.processMessages ((void *) &value);

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnWriteByte (value);
   };

   functionTable->pfnWriteChar = [] (int value) {
      // if this message is for a bot, call the client message function...
      game.processMessages ((void *) &value);

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnWriteChar (value);
   };

   functionTable->pfnWriteShort = [] (int value) {
      // if this message is for a bot, call the client message function...
      game.processMessages ((void *) &value);

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnWriteShort (value);
   };

   functionTable->pfnWriteLong = [] (int value) {
      // if this message is for a bot, call the client message function...
      game.processMessages ((void *) &value);

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnWriteLong (value);
   };

   functionTable->pfnWriteAngle = [] (float value) {
      // if this message is for a bot, call the client message function...
      game.processMessages ((void *) &value);

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnWriteAngle (value);
   };

   functionTable->pfnWriteCoord = [] (float value) {
      // if this message is for a bot, call the client message function...
      game.processMessages ((void *) &value);

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnWriteCoord (value);
   };

   functionTable->pfnWriteString = [] (const char *sz) {
      // if this message is for a bot, call the client message function...
      game.processMessages ((void *) sz);

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnWriteString (sz);
   };

   functionTable->pfnWriteEntity = [] (int value) {
      // if this message is for a bot, call the client message function...
      game.processMessages ((void *) &value);

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnWriteEntity (value);
   };

   functionTable->pfnRegUserMsg = [] (const char *name, int size) {
      // this function registers a "user message" by the engine side. User messages are network
      // messages the game DLL asks the engine to send to clients. Since many MODs have completely
      // different client features (Counter-Strike has a radar and a timer, for example), network
      // messages just can't be the same for every MOD. Hence here the MOD DLL tells the engine,
      // "Hey, you have to know that I use a network message whose name is pszName and it is size
      // packets long". The engine books it, and returns the ID number under which he recorded that
      // custom message. Thus every time the MOD DLL will be wanting to send a message named pszName
      // using pfnMessageBegin (), it will know what message ID number to send, and the engine will
      // know what to do, only for non-metamod version

      if (game.is (GAME_METAMOD)) {
         RETURN_META_VALUE (MRES_IGNORED, 0);
      }
      int message = engfuncs.pfnRegUserMsg (name, size);

      if (strcmp (name, "VGUIMenu") == 0) {
         game.setMessageId (NETMSG_VGUI, message);
      }
      else if (strcmp (name, "ShowMenu") == 0) {
         game.setMessageId (NETMSG_SHOWMENU, message);
      }
      else if (strcmp (name, "WeaponList") == 0) {
         game.setMessageId (NETMSG_WEAPONLIST, message);
      }
      else if (strcmp (name, "CurWeapon") == 0) {
         game.setMessageId (NETMSG_CURWEAPON, message);
      }
      else if (strcmp (name, "AmmoX") == 0) {
         game.setMessageId (NETMSG_AMMOX, message);
      }
      else if (strcmp (name, "AmmoPickup") == 0) {
         game.setMessageId (NETMSG_AMMOPICKUP, message);
      }
      else if (strcmp (name, "Damage") == 0) {
         game.setMessageId (NETMSG_DAMAGE, message);
      }
      else if (strcmp (name, "Money") == 0) {
         game.setMessageId (NETMSG_MONEY, message);
      }
      else if (strcmp (name, "StatusIcon") == 0) {
         game.setMessageId (NETMSG_STATUSICON, message);
      }
      else if (strcmp (name, "DeathMsg") == 0) {
         game.setMessageId (NETMSG_DEATH, message);
      }
      else if (strcmp (name, "ScreenFade") == 0) {
         game.setMessageId (NETMSG_SCREENFADE, message);
      }
      else if (strcmp (name, "HLTV") == 0) {
         game.setMessageId (NETMSG_HLTV, message);
      }
      else if (strcmp (name, "TextMsg") == 0) {
         game.setMessageId (NETMSG_TEXTMSG, message);
      }
      else if (strcmp (name, "TeamInfo") == 0) {
         game.setMessageId (NETMSG_TEAMINFO, message);
      }
      else if (strcmp (name, "BarTime") == 0) {
         game.setMessageId (NETMSG_BARTIME, message);
      }
      else if (strcmp (name, "SendAudio") == 0) {
         game.setMessageId (NETMSG_SENDAUDIO, message);
      }
      else if (strcmp (name, "SayText") == 0) {
         game.setMessageId (NETMSG_SAYTEXT, message);
      }
      else if (strcmp (name, "BotVoice") == 0) {
         game.setMessageId (NETMSG_BOTVOICE, message);
      }
      else if (strcmp (name, "NVGToggle") == 0) {
         game.setMessageId (NETMSG_NVGTOGGLE, message);
      }
      else if (strcmp (name, "FlashBat") == 0) {
         game.setMessageId (NETMSG_FLASHBAT, message);
      }
      else if (strcmp (name, "Flashlight") == 0) {
         game.setMessageId (NETMSG_FLASHLIGHT, message);
      }
      else if (strcmp (name, "ItemStatus") == 0) {
         game.setMessageId (NETMSG_ITEMSTATUS, message);
      }
      return message;
   };

   functionTable->pfnClientPrintf = [] (edict_t *ent, PRINT_TYPE printType, const char *message) {
      // this function prints the text message string pointed to by message by the client side of
      // the client entity pointed to by ent, in a manner depending of printType (print_console,
      // print_center or print_chat). Be certain never to try to feed a bot with this function,
      // as it will crash your server. Why would you, anyway ? bots have no client DLL as far as
      // we know, right ? But since stupidity rules this world, we do a preventive check :)

      if (util.isFakeClient (ent)) {
         if (game.is (GAME_METAMOD)) {
            RETURN_META (MRES_SUPERCEDE);
         }
         return;
      }

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnClientPrintf (ent, printType, message);
   };

   functionTable->pfnCmd_Args = [] (void) {
      // this function returns a pointer to the whole current client command string. Since bots
      // have no client DLL and we may want a bot to execute a client command, we had to implement
      // a argv string in the bot DLL for holding the bots' commands, and also keep track of the
      // argument count. Hence this hook not to let the engine ask an unexistent client DLL for a
      // command we are holding here. Of course, real clients commands are still retrieved the
      // normal way, by asking the game.

      // is this a bot issuing that client command?
      if (game.isBotCmd ()) {
         if (game.is (GAME_METAMOD)) {
            RETURN_META_VALUE (MRES_SUPERCEDE, game.botArgs ());
         }
         return game.botArgs (); // else return the whole bot client command string we know
      }

      if (game.is (GAME_METAMOD)) {
         RETURN_META_VALUE (MRES_IGNORED, static_cast <const char *> (nullptr));
      }
      return engfuncs.pfnCmd_Args (); // ask the client command string to the engine
   };

   functionTable->pfnCmd_Argv = [] (int argc) {
      // this function returns a pointer to a certain argument of the current client command. Since
      // bots have no client DLL and we may want a bot to execute a client command, we had to
      // implement a argv string in the bot DLL for holding the bots' commands, and also keep
      // track of the argument count. Hence this hook not to let the engine ask an unexistent client
      // DLL for a command we are holding here. Of course, real clients commands are still retrieved
      // the normal way, by asking the game.

      // is this a bot issuing that client command?
      if (game.isBotCmd ()) {
         if (game.is (GAME_METAMOD)) {
            RETURN_META_VALUE (MRES_SUPERCEDE, game.botArgv (argc));
         }
         return game.botArgv (argc); // if so, then return the wanted argument we know
      }

      if (game.is (GAME_METAMOD)) {
         RETURN_META_VALUE (MRES_IGNORED, static_cast <const char *> (nullptr));
      }
      return engfuncs.pfnCmd_Argv (argc); // ask the argument number "argc" to the engine
   };

   functionTable->pfnCmd_Argc = [] (void) {
      // this function returns the number of arguments the current client command string has. Since
      // bots have no client DLL and we may want a bot to execute a client command, we had to
      // implement a argv string in the bot DLL for holding the bots' commands, and also keep
      // track of the argument count. Hence this hook not to let the engine ask an unexistent client
      // DLL for a command we are holding here. Of course, real clients commands are still retrieved
      // the normal way, by asking the game.

      // is this a bot issuing that client command?
      if (game.isBotCmd ()) {
         if (game.is (GAME_METAMOD)) {
            RETURN_META_VALUE (MRES_SUPERCEDE, game.botArgc ());
         }
         return game.botArgc (); // if so, then return the argument count we know
      }

      if (game.is (GAME_METAMOD)) {
         RETURN_META_VALUE (MRES_IGNORED, 0);
      }
      return engfuncs.pfnCmd_Argc (); // ask the engine how many arguments there are
   };

   functionTable->pfnSetClientMaxspeed = [] (const edict_t *ent, float newMaxspeed) {
      Bot *bot = bots.getBot (const_cast <edict_t *> (ent));

      // check wether it's not a bot
      if (bot != nullptr) {
         bot->pev->maxspeed = newMaxspeed;
      }

      if (game.is (GAME_METAMOD)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnSetClientMaxspeed (ent, newMaxspeed);
   };
   return TRUE;
}

SHARED_LIBRARAY_EXPORT int GetEngineFunctions_Post (enginefuncs_t *functionTable, int *) {

   memset (functionTable, 0, sizeof (enginefuncs_t));

   functionTable->pfnMessageEnd = [] (void) {
      // send latency fix
      bots.sendDeathMsgFix ();

      RETURN_META (MRES_IGNORED);
   };
   return TRUE;
}

SHARED_LIBRARAY_EXPORT int Server_GetBlendingInterface (int version, void **ppinterface, void *pstudio, float (*rotationmatrix)[3][4], float (*bonetransform)[128][3][4]) {
   // this function synchronizes the studio model animation blending interface (i.e, what parts
   // of the body move, which bones, which hitboxes and how) between the server and the game DLL.
   // some MODs can be using a different hitbox scheme than the standard one.

   auto api_GetBlendingInterface = game.getLib ().resolve <int (*) (int, void **, void *, float(*)[3][4], float(*)[128][3][4])> ("Server_GetBlendingInterface");

   if (api_GetBlendingInterface == nullptr) {
      return FALSE;
   }
   return api_GetBlendingInterface (version, ppinterface, pstudio, rotationmatrix, bonetransform);
}

SHARED_LIBRARAY_EXPORT int Meta_Query (char *, plugin_info_t **pPlugInfo, mutil_funcs_t *pMetaUtilFuncs) {
   // this function is the first function ever called by metamod in the plugin DLL. Its purpose
   // is for metamod to retrieve basic information about the plugin, such as its meta-interface
   // version, for ensuring compatibility with the current version of the running metamod.

   gpMetaUtilFuncs = pMetaUtilFuncs;
   *pPlugInfo = &Plugin_info;

   return TRUE; // tell metamod this plugin looks safe
}

SHARED_LIBRARAY_EXPORT int Meta_Attach (PLUG_LOADTIME, metamod_funcs_t *functionTable, meta_globals_t *pMGlobals, gamedll_funcs_t *pGamedllFuncs) {
   // this function is called when metamod attempts to load the plugin. Since it's the place
   // where we can tell if the plugin will be allowed to run or not, we wait until here to make
   // our initialization stuff, like registering CVARs and dedicated server commands.

   // metamod engine & dllapi function tables
   static metamod_funcs_t metamodFunctionTable = {
      nullptr, // pfnGetEntityAPI ()
      nullptr, // pfnGetEntityAPI_Post ()
      GetEntityAPI2, // pfnGetEntityAPI2 ()
      GetEntityAPI2_Post, // pfnGetEntityAPI2_Post ()
      nullptr, // pfnGetNewDLLFunctions ()
      nullptr, // pfnGetNewDLLFunctions_Post ()
      GetEngineFunctions, // pfnGetEngineFunctions ()
      GetEngineFunctions_Post, // pfnGetEngineFunctions_Post ()
   };

   // keep track of the pointers to engine function tables metamod gives us
   gpMetaGlobals = pMGlobals;
   memcpy (functionTable, &metamodFunctionTable, sizeof (metamod_funcs_t));
   gpGamedllFuncs = pGamedllFuncs;

   return TRUE; // returning true enables metamod to attach this plugin
}

SHARED_LIBRARAY_EXPORT int Meta_Detach (PLUG_LOADTIME, PL_UNLOAD_REASON) {
   // this function is called when metamod unloads the plugin. A basic check is made in order
   // to prevent unloading the plugin if its processing should not be interrupted.

   bots.kickEveryone (true); // kick all bots off this server
   waypoints.init ();

   return TRUE;
}

SHARED_LIBRARAY_EXPORT void Meta_Init (void) {
   // this function is called by metamod, before any other interface functions. Purpose of this
   // function to give plugin a chance to determine is plugin running under metamod or not.

   game.addGameFlag (GAME_METAMOD);
}

DLL_GIVEFNPTRSTODLL GiveFnptrsToDll (enginefuncs_t *functionTable, globalvars_t *pGlobals) {
   // this is the very first function that is called in the game DLL by the game. Its purpose
   // is to set the functions interfacing up, by exchanging the functionTable function list
   // along with a pointer to the engine's global variables structure pGlobals, with the game
   // DLL. We can there decide if we want to load the normal game DLL just behind our bot DLL,
   // or any other game DLL that is present, such as Will Day's metamod. Also, since there is
   // a known bug on Win32 platforms that prevent hook DLLs (such as our bot DLL) to be used in
   // single player games (because they don't export all the stuff they should), we may need to
   // build our own array of exported symbols from the actual game DLL in order to use it as
   // such if necessary. Nothing really bot-related is done in this function. The actual bot
   // initialization stuff will be done later, when we'll be certain to have a multilayer game.

   // get the engine functions from the game...
   memcpy (&engfuncs, functionTable, sizeof (enginefuncs_t));
   globals = pGlobals;

   if (game.postload ()) {
      return;
   }
   auto api_GiveFnptrsToDll = game.getLib ().resolve <void (STD_CALL *) (enginefuncs_t *, globalvars_t *)> ("GiveFnptrsToDll");
   
   assert (api_GiveFnptrsToDll != nullptr);
   GetEngineFunctions (functionTable, nullptr);

   // give the engine functions to the other DLL...
   api_GiveFnptrsToDll (functionTable, pGlobals);
}

DLL_ENTRYPOINT {
   // dynamic library entry point, can be used for uninitialization stuff. NOT for initializing
   // anything because if you ever attempt to wander outside the scope of this function on a
   // DLL attach, LoadLibrary() will simply fail. And you can't do I/Os here either.

   // dynamic library detaching ??
   if (DLL_DETACHING) {
      waypoints.init (); // free everything that's freeable
   }
   DLL_RETENTRY; // the return data type is OS specific too
}

void helper_LinkEntity (EntityFunction &addr, const char *name, entvars_t *pev) {
   if (addr == nullptr) {
      addr = game.getLib ().resolve <EntityFunction> (name);
   }

   if (addr == nullptr) {
      return;
   }
   addr (pev);
}

#define LINK_ENTITY(entityName)                              \
   SHARED_LIBRARAY_EXPORT void entityName (entvars_t *pev) { \
      static EntityFunction addr;                             \
      helper_LinkEntity (addr, #entityName, pev);            \
   }

// entities in counter-strike...
LINK_ENTITY (DelayedUse)
LINK_ENTITY (ambient_generic)
LINK_ENTITY (ammo_338magnum)
LINK_ENTITY (ammo_357sig)
LINK_ENTITY (ammo_45acp)
LINK_ENTITY (ammo_50ae)
LINK_ENTITY (ammo_556nato)
LINK_ENTITY (ammo_556natobox)
LINK_ENTITY (ammo_57mm)
LINK_ENTITY (ammo_762nato)
LINK_ENTITY (ammo_9mm)
LINK_ENTITY (ammo_buckshot)
LINK_ENTITY (armoury_entity)
LINK_ENTITY (beam)
LINK_ENTITY (bodyque)
LINK_ENTITY (button_target)
LINK_ENTITY (cycler)
LINK_ENTITY (cycler_prdroid)
LINK_ENTITY (cycler_sprite)
LINK_ENTITY (cycler_weapon)
LINK_ENTITY (cycler_wreckage)
LINK_ENTITY (env_beam)
LINK_ENTITY (env_beverage)
LINK_ENTITY (env_blood)
LINK_ENTITY (env_bombglow)
LINK_ENTITY (env_bubbles)
LINK_ENTITY (env_debris)
LINK_ENTITY (env_explosion)
LINK_ENTITY (env_fade)
LINK_ENTITY (env_funnel)
LINK_ENTITY (env_global)
LINK_ENTITY (env_glow)
LINK_ENTITY (env_laser)
LINK_ENTITY (env_lightning)
LINK_ENTITY (env_message)
LINK_ENTITY (env_rain)
LINK_ENTITY (env_render)
LINK_ENTITY (env_shake)
LINK_ENTITY (env_shooter)
LINK_ENTITY (env_snow)
LINK_ENTITY (env_sound)
LINK_ENTITY (env_spark)
LINK_ENTITY (env_sprite)
LINK_ENTITY (fireanddie)
LINK_ENTITY (func_bomb_target)
LINK_ENTITY (func_breakable)
LINK_ENTITY (func_button)
LINK_ENTITY (func_buyzone)
LINK_ENTITY (func_conveyor)
LINK_ENTITY (func_door)
LINK_ENTITY (func_door_rotating)
LINK_ENTITY (func_escapezone)
LINK_ENTITY (func_friction)
LINK_ENTITY (func_grencatch)
LINK_ENTITY (func_guntarget)
LINK_ENTITY (func_healthcharger)
LINK_ENTITY (func_hostage_rescue)
LINK_ENTITY (func_illusionary)
LINK_ENTITY (func_ladder)
LINK_ENTITY (func_monsterclip)
LINK_ENTITY (func_mortar_field)
LINK_ENTITY (func_pendulum)
LINK_ENTITY (func_plat)
LINK_ENTITY (func_platrot)
LINK_ENTITY (func_pushable)
LINK_ENTITY (func_rain)
LINK_ENTITY (func_recharge)
LINK_ENTITY (func_rot_button)
LINK_ENTITY (func_rotating)
LINK_ENTITY (func_snow)
LINK_ENTITY (func_tank)
LINK_ENTITY (func_tankcontrols)
LINK_ENTITY (func_tanklaser)
LINK_ENTITY (func_tankmortar)
LINK_ENTITY (func_tankrocket)
LINK_ENTITY (func_trackautochange)
LINK_ENTITY (func_trackchange)
LINK_ENTITY (func_tracktrain)
LINK_ENTITY (func_train)
LINK_ENTITY (func_traincontrols)
LINK_ENTITY (func_vehicle)
LINK_ENTITY (func_vehiclecontrols)
LINK_ENTITY (func_vip_safetyzone)
LINK_ENTITY (func_wall)
LINK_ENTITY (func_wall_toggle)
LINK_ENTITY (func_water)
LINK_ENTITY (func_weaponcheck)
LINK_ENTITY (game_counter)
LINK_ENTITY (game_counter_set)
LINK_ENTITY (game_end)
LINK_ENTITY (game_player_equip)
LINK_ENTITY (game_player_hurt)
LINK_ENTITY (game_player_team)
LINK_ENTITY (game_score)
LINK_ENTITY (game_team_master)
LINK_ENTITY (game_team_set)
LINK_ENTITY (game_text)
LINK_ENTITY (game_zone_player)
LINK_ENTITY (gibshooter)
LINK_ENTITY (grenade)
LINK_ENTITY (hostage_entity)
LINK_ENTITY (info_bomb_target)
LINK_ENTITY (info_hostage_rescue)
LINK_ENTITY (info_intermission)
LINK_ENTITY (info_landmark)
LINK_ENTITY (info_map_parameters)
LINK_ENTITY (info_null)
LINK_ENTITY (info_player_deathmatch)
LINK_ENTITY (info_player_start)
LINK_ENTITY (info_target)
LINK_ENTITY (info_teleport_destination)
LINK_ENTITY (info_vip_start)
LINK_ENTITY (infodecal)
LINK_ENTITY (item_airtank)
LINK_ENTITY (item_airbox)
LINK_ENTITY (item_antidote)
LINK_ENTITY (item_assaultsuit)
LINK_ENTITY (item_battery)
LINK_ENTITY (item_healthkit)
LINK_ENTITY (item_kevlar)
LINK_ENTITY (item_longjump)
LINK_ENTITY (item_security)
LINK_ENTITY (item_sodacan)
LINK_ENTITY (item_suit)
LINK_ENTITY (item_thighpack)
LINK_ENTITY (light)
LINK_ENTITY (light_environment)
LINK_ENTITY (light_spot)
LINK_ENTITY (momentary_door)
LINK_ENTITY (momentary_rot_button)
LINK_ENTITY (monster_hevsuit_dead)
LINK_ENTITY (monster_mortar)
LINK_ENTITY (monster_scientist)
LINK_ENTITY (multi_manager)
LINK_ENTITY (multisource)
LINK_ENTITY (path_corner)
LINK_ENTITY (path_track)
LINK_ENTITY (player)
LINK_ENTITY (player_loadsaved)
LINK_ENTITY (player_weaponstrip)
LINK_ENTITY (point_clientcommand)
LINK_ENTITY (point_servercommand)
LINK_ENTITY (soundent)
LINK_ENTITY (spark_shower)
LINK_ENTITY (speaker)
LINK_ENTITY (target_cdaudio)
LINK_ENTITY (test_effect)
LINK_ENTITY (trigger)
LINK_ENTITY (trigger_auto)
LINK_ENTITY (trigger_autosave)
LINK_ENTITY (trigger_camera)
LINK_ENTITY (trigger_cdaudio)
LINK_ENTITY (trigger_changelevel)
LINK_ENTITY (trigger_changetarget)
LINK_ENTITY (trigger_counter)
LINK_ENTITY (trigger_endsection)
LINK_ENTITY (trigger_gravity)
LINK_ENTITY (trigger_hurt)
LINK_ENTITY (trigger_monsterjump)
LINK_ENTITY (trigger_multiple)
LINK_ENTITY (trigger_once)
LINK_ENTITY (trigger_push)
LINK_ENTITY (trigger_random)
LINK_ENTITY (trigger_random_time)
LINK_ENTITY (trigger_random_unique)
LINK_ENTITY (trigger_relay)
LINK_ENTITY (trigger_setorigin)
LINK_ENTITY (trigger_teleport)
LINK_ENTITY (trigger_transition)
LINK_ENTITY (weapon_ak47)
LINK_ENTITY (weapon_aug)
LINK_ENTITY (weapon_awp)
LINK_ENTITY (weapon_c4)
LINK_ENTITY (weapon_deagle)
LINK_ENTITY (weapon_elite)
LINK_ENTITY (weapon_famas)
LINK_ENTITY (weapon_fiveseven)
LINK_ENTITY (weapon_flashbang)
LINK_ENTITY (weapon_g3sg1)
LINK_ENTITY (weapon_galil)
LINK_ENTITY (weapon_glock18)
LINK_ENTITY (weapon_hegrenade)
LINK_ENTITY (weapon_knife)
LINK_ENTITY (weapon_m249)
LINK_ENTITY (weapon_m3)
LINK_ENTITY (weapon_m4a1)
LINK_ENTITY (weapon_mac10)
LINK_ENTITY (weapon_mp5navy)
LINK_ENTITY (weapon_p228)
LINK_ENTITY (weapon_p90)
LINK_ENTITY (weapon_scout)
LINK_ENTITY (weapon_sg550)
LINK_ENTITY (weapon_sg552)
LINK_ENTITY (weapon_shield)
LINK_ENTITY (weapon_shieldgun)
LINK_ENTITY (weapon_smokegrenade)
LINK_ENTITY (weapon_tmp)
LINK_ENTITY (weapon_ump45)
LINK_ENTITY (weapon_usp)
LINK_ENTITY (weapon_xm1014)
LINK_ENTITY (weaponbox)
LINK_ENTITY (world_items)
LINK_ENTITY (worldspawn)
