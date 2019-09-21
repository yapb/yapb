//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) Yet Another POD-Bot Contributors <yapb@entix.io>.
//
// This software is licensed under the MIT license.
// Additional exceptions apply. For full license details, see LICENSE.txt
//

#include <yapb.h>

ConVar yb_version ("yb_version", PRODUCT_VERSION, Var::ReadOnly);

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

namespace variadic {
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
      auto buffer = strings.chars ();

      va_start (ap, format);
      vsnprintf (buffer, StringBuffer::StaticBufferSize, format, ap);
      va_end (ap);

      if (ent && (ent->v.flags & (FL_FAKECLIENT | FL_DORMANT))) {
         auto bot = bots[ent];

         if (bot) {
            bot->issueCommand (buffer);
         }

         if (game.is (GameFlags::Metamod)) {
            RETURN_META (MRES_SUPERCEDE); // prevent bots to be forced to issue client commands
         }
         return;
      }

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnClientCommand (ent, buffer);
   }
}

CR_EXPORT int GetEntityAPI2 (gamefuncs_t *table, int *) {
   // this function is called right after GiveFnptrsToDll() by the engine in the game DLL (or
   // what it BELIEVES to be the game DLL), in order to copy the list of MOD functions that can
   // be called by the engine, into a memory block pointed to by the functionTable pointer
   // that is passed into this function (explanation comes straight from botman). This allows
   // the Half-Life engine to call these MOD DLL functions when it needs to spawn an entity,
   // connect or disconnect a player, call Think() functions, Touch() functions, or Use()
   // functions, etc. The bot DLL passes its OWN list of these functions back to the Half-Life
   // engine, and then calls the MOD DLL's version of GetEntityAPI to get the REAL gamedll
   // functions this time (to use in the bot code).

   plat.bzero (table, sizeof (gamefuncs_t));

   if (!(game.is (GameFlags::Metamod))) {
      auto api_GetEntityAPI = game.lib ().resolve <int (*) (gamefuncs_t *, int)> ("GetEntityAPI");

      // pass other DLLs engine callbacks to function table...
      if (!api_GetEntityAPI || api_GetEntityAPI (&dllapi, INTERFACE_VERSION) == 0) {
         logger.fatal ("Could not resolve symbol \"%s\" in the game dll.", "GetEntityAPI");
      }
      dllfuncs.dllapi_table = &dllapi;
      gpGamedllFuncs = &dllfuncs;

      memcpy (table, &dllapi, sizeof (gamefuncs_t));
   }

   table->pfnGameInit = [] () {
      // this function is a one-time call, and appears to be the second function called in the
      // DLL after GiveFntprsToDll() has been called. Its purpose is to tell the MOD DLL to
      // initialize the game before the engine actually hooks into it with its video frames and
      // clients connecting. Note that it is a different step than the *server* initialization.
      // This one is called once, and only once, when the game process boots up before the first
      // server is enabled. Here is a good place to do our own game session initialization, and
      // to register by the engine side the server commands we need to administrate our bots.

      // register logger
      logger.initialize (strings.format ("%slogs/yapb.log", graph.getDataDirectory (false)), [] (const char *msg) {
         game.print (msg);
      });

      // set correct version string
      yb_version.set (strings.format ("%d.%d.%d", PRODUCT_VERSION_DWORD_INTERNAL, util.buildNumber ()));

      // execute main config
      conf.loadMainConfig ();
      conf.adjustWeaponPrices ();

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnGameInit ();
   };

   table->pfnSpawn = [] (edict_t *ent) {
      // this function asks the game DLL to spawn (i.e, give a physical existence in the virtual
      // world, in other words to 'display') the entity pointed to by ent in the game. The
      // Spawn() function is one of the functions any entity is supposed to have in the game DLL,
      // and any MOD is supposed to implement one for each of its entities.

      game.precache ();

      if (game.is (GameFlags::Metamod)) {
         RETURN_META_VALUE (MRES_IGNORED, 0);
      }
      int result = dllapi.pfnSpawn (ent); // get result

      if (ent->v.rendermode == kRenderTransTexture) {
         ent->v.flags &= ~FL_WORLDBRUSH; // clear the FL_WORLDBRUSH flag out of transparent ents
      }
      return result;
   };

   table->pfnTouch = [] (edict_t *pentTouched, edict_t *pentOther) {
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

      if (!game.isNullEntity (pentTouched) && pentOther != game.getStartEntity ()) {
         auto bot = bots[pentTouched];

         if (bot && game.isShootableBreakable (pentOther)) {
            bot->checkBreakable (pentOther);
         }
      }

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnTouch (pentTouched, pentOther);
   };

   table->pfnClientConnect = [] (edict_t *ent, const char *name, const char *addr, char rejectReason[128]) {
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

         // if not dedicated set the default editor for graph
         if (!game.isDedicated ()) {
            graph.setEditor (ent);
         }
      }

      if (game.is (GameFlags::Metamod)) {
         RETURN_META_VALUE (MRES_IGNORED, 0);
      }
      return dllapi.pfnClientConnect (ent, name, addr, rejectReason);
   };

   table->pfnClientDisconnect = [] (edict_t *ent) {
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

      for (auto &bot : bots) {
         if (bot->pev == &ent->v) {
            bot->showChaterIcon (false);

            conf.clearUsedName (bot.get ()); // clear the bot name
            bots.erase (bot.get ()); // remove the bot from bots array

            break;
         }
      }

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnClientDisconnect (ent);
   };

   table->pfnClientUserInfoChanged = [] (edict_t  *ent, char *infobuffer) {
      // this function is called when a player changes model, or changes team. Occasionally it
      // enforces rules on these changes (for example, some MODs don't want to allow players to
      // change their player model). But most commonly, this function is in charge of handling
      // team changes, recounting the teams population, etc...

      ctrl.assignAdminRights (ent, infobuffer);

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnClientUserInfoChanged (ent, infobuffer);
   };

   table->pfnClientCommand = [] (edict_t *ent) {
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
         if (game.is (GameFlags::Metamod)) {
            RETURN_META (MRES_SUPERCEDE);
         }
         return;
      }

      else if (ctrl.handleMenuCommands (ent)) {
         if (game.is (GameFlags::Metamod)) {
            RETURN_META (MRES_SUPERCEDE);
         }
         return;
      }

      // record stuff about radio and chat
      bots.captureChatRadio (engfuncs.pfnCmd_Argv (0), engfuncs.pfnCmd_Argv (1), ent);

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnClientCommand (ent);
   };

   table->pfnServerActivate = [] (edict_t *pentEdictList, int edictCount, int clientMax) {
      // this function is called when the server has fully loaded and is about to manifest itself
      // on the network as such. Since a mapchange is actually a server shutdown followed by a
      // restart, this function is also called when a new map is being loaded. Hence it's the
      // perfect place for doing initialization stuff for our bots, such as reading the BSP data,
      // loading the bot profiles, and drawing the world map (ie, filling the navigation hashtable).
      // Once this function has been called, the server can be considered as "running".

      conf.loadConfigs (); // initialize all config files

      // do a level initialization
      game.levelInitialize (pentEdictList, edictCount);

      // update worldmodel
      illum.resetWorldModel ();

      // do level initialization stuff here...
      graph.loadGraphData ();

      // execute main config
      conf.loadMainConfig ();

      if (File::exists (strings.format ("%s/maps/%s_yapb.cfg", game.getModName (), game.getMapName ()))) {
         game.serverCommand ("exec maps/%s_yapb.cfg", game.getMapName ());
         game.print ("Executing Map-Specific config file");
      }
      bots.initQuota ();

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnServerActivate (pentEdictList, edictCount, clientMax);

      graph.rebuildVisibility ();
   };

   table->pfnServerDeactivate = [] () {
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
      graph.savePractice ();

      // destroy global killer entity
      bots.destroyKillerEntity ();

      // set state to unprecached
      game.setUnprecached ();

      // enable lightstyle animations on level change
      illum.enableAnimation (true);

      // send message on new map
      util.setNeedForWelcome (false);

      // xash is not kicking fakeclients on changelevel
      if (game.is (GameFlags::Xash3D)) {
         bots.kickEveryone (true, false);
      }
      graph.initGraph ();

      // clear all the bots
      bots.destroy ();

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnServerDeactivate ();
   };

   table->pfnStartFrame = [] () {
      // this function starts a video frame. It is called once per video frame by the game. If
      // you run Half-Life at 90 fps, this function will then be called 90 times per second. By
      // placing a hook on it, we have a good place to do things that should be done continuously
      // during the game, for example making the bots think (yes, because no Think() function exists
      // for the bots by the MOD side, remember). Also here we have control on the bot population,
      // for example if a new player joins the server, we should disconnect a bot, and if the
      // player population decreases, we should fill the server with other bots.

      // update lightstyle animations
      illum.animateLight ();

      // update some stats for clients
      util.updateClients ();

      if (graph.hasEditFlag (GraphEdit::On) && graph.hasEditor ()) {
         graph.frame ();
      }

      // run stuff periodically
      game.slowFrame ();

      if (bots.hasBotsOnline ()) {
         // keep track of grenades on map
         bots.updateActiveGrenade ();

         // keep track of intresting entities
         bots.updateIntrestingEntities ();
      }

      // keep bot number up to date
      bots.maintainQuota ();
      
      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnStartFrame ();

      // run the bot ai
      bots.frame ();
   };

   table->pfnCmdStart = [] (const edict_t *player, usercmd_t *cmd, unsigned int random_seed) {
      auto ent = const_cast <edict_t *> (player);
      
      // if we're handle pings for bots and clients, clear IN_SCORE button so SV_ShouldUpdatePing engine function return false
      // and SV_EmitPings will not overwrite our results
      if (game.is (GameFlags::HasFakePings) && yb_show_latency.int_ () == 2) {
         if ((cmd->buttons & IN_SCORE) || (ent->v.oldbuttons & IN_SCORE)) {
            cmd->buttons &= ~IN_SCORE;

            // send our version of pings
            util.sendPings (ent);
         }
      }
      
      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnCmdStart (player, cmd, random_seed);
   };

   table->pfnPM_Move = [] (playermove_t *playerMove, int server) {
      // this is the player movement code clients run to predict things when the server can't update
      // them often enough (or doesn't want to). The server runs exactly the same function for
      // moving players. There is normally no distinction between them, else client-side prediction
      // wouldn't work properly (and it doesn't work that well, already...)

      illum.setWorldModel (playerMove->physents[0].model);

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      dllapi.pfnPM_Move (playerMove, server);
   };
   return true;
}

CR_LINKAGE_C int GetEntityAPI2_Post (gamefuncs_t *table, int *) {
   // this function is called right after GiveFnptrsToDll() by the engine in the game DLL (or
   // what it BELIEVES to be the game DLL), in order to copy the list of MOD functions that can
   // be called by the engine, into a memory block pointed to by the functionTable pointer
   // that is passed into this function (explanation comes straight from botman). This allows
   // the Half-Life engine to call these MOD DLL functions when it needs to spawn an entity,
   // connect or disconnect a player, call Think() functions, Touch() functions, or Use()
   // functions, etc. The bot DLL passes its OWN list of these functions back to the Half-Life
   // engine, and then calls the MOD DLL's version of GetEntityAPI to get the REAL gamedll
   // functions this time (to use in the bot code). Post version, called only by metamod.

   plat.bzero (table, sizeof (gamefuncs_t));

   table->pfnSpawn = [] (edict_t *ent) {
      // this function asks the game DLL to spawn (i.e, give a physical existence in the virtual
      // world, in other words to 'display') the entity pointed to by ent in the game. The
      // Spawn() function is one of the functions any entity is supposed to have in the game DLL,
      // and any MOD is supposed to implement one for each of its entities. Post version called
      // only by metamod.

      // solves the bots unable to see through certain types of glass bug.
      if (ent->v.rendermode == kRenderTransTexture) {
         ent->v.flags &= ~FL_WORLDBRUSH; // clear the FL_WORLDBRUSH flag out of transparent ents
      }
      RETURN_META_VALUE (MRES_HANDLED, 0);
   };

   table->pfnStartFrame = [] () {
      // this function starts a video frame. It is called once per video frame by the game. If
      // you run Half-Life at 90 fps, this function will then be called 90 times per second. By
      // placing a hook on it, we have a good place to do things that should be done continuously
      // during the game, for example making the bots think (yes, because no Think() function exists
      // for the bots by the MOD side, remember).  Post version called only by metamod.

      // run the bot ai
      bots.frame ();

      RETURN_META (MRES_IGNORED);
   };

   table->pfnServerActivate = [] (edict_t *, int, int) {
      // this function is called when the server has fully loaded and is about to manifest itself
      // on the network as such. Since a mapchange is actually a server shutdown followed by a
      // restart, this function is also called when a new map is being loaded. Hence it's the
      // perfect place for doing initialization stuff for our bots, such as reading the BSP data,
      // loading the bot profiles, and drawing the world map (ie, filling the navigation hashtable).
      // Once this function has been called, the server can be considered as "running". Post version
      // called only by metamod.

      graph.rebuildVisibility ();

      RETURN_META (MRES_IGNORED);
   };

   return true;
}

CR_LINKAGE_C int GetEngineFunctions (enginefuncs_t *table, int *) {
   if (game.is (GameFlags::Metamod)) {
      plat.bzero (table, sizeof (enginefuncs_t));
   }

   table->pfnChangeLevel = [] (char *s1, char *s2) {
      // the purpose of this function is to ask the engine to shutdown the server and restart a
      // new one running the map whose name is s1. It is used ONLY IN SINGLE PLAYER MODE and is
      // transparent to the user, because it saves the player state and equipment and restores it
      // back in the new level. The "changelevel trigger point" in the old level is linked to the
      // new level's spawn point using the s2 string, which is formatted as follows: "trigger_name
      // to spawnpoint_name", without spaces (for example, "tr_1atotr_2lm" would tell the engine
      // the player has reached the trigger point "tr_1a" and has to spawn in the next level on the
      // spawn point named "tr_2lm".

      // save collected experience on map change
      graph.savePractice ();

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnChangeLevel (s1, s2);
   };

   table->pfnLightStyle = [] (int style, char *val) {
      // ths function update lightstyle for the bots

      illum.updateLight (style, val);

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnLightStyle (style, val);
   };

   if (game.is (GameFlags::Legacy)) {
      table->pfnFindEntityByString = [] (edict_t *edictStartSearchAfter, const char *field, const char *value) {
         // round starts in counter-strike 1.5
         if (strcmp (value, "info_map_parameters") == 0) {
            bots.initRound ();
         }

         if (game.is (GameFlags::Metamod)) {
            RETURN_META_VALUE (MRES_IGNORED, static_cast <edict_t *> (nullptr));
         }
         return engfuncs.pfnFindEntityByString (edictStartSearchAfter, field, value);
      };
   }

   table->pfnEmitSound = [] (edict_t *entity, int channel, const char *sample, float volume, float attenuation, int flags, int pitch) {
      // this function tells the engine that the entity pointed to by "entity", is emitting a sound
      // which fileName is "sample", at level "channel" (CHAN_VOICE, etc...), with "volume" as
      // loudness multiplicator (normal volume VOL_NORM is 1.0), with a pitch of "pitch" (normal
      // pitch PITCH_NORM is 100.0), and that this sound has to be attenuated by distance in air
      // according to the value of "attenuation" (normal attenuation ATTN_NORM is 0.8 ; ATTN_NONE
      // means no attenuation with distance). Optionally flags "fFlags" can be passed, which I don't
      // know the heck of the purpose. After we tell the engine to emit the sound, we have to call
      // SoundAttachToThreat() to bring the sound to the ears of the bots. Since bots have no client DLL
      // to handle this for them, such a job has to be done manually.

      util.listenNoise (entity, sample, volume);

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnEmitSound (entity, channel, sample, volume, attenuation, flags, pitch);
   };

   table->pfnMessageBegin = [] (int msgDest, int msgType, const float *origin, edict_t *ed) {
      // this function called each time a message is about to sent.

      msgs.start (ed, msgType);

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnMessageBegin (msgDest, msgType, origin, ed);
   };

   if (!game.is (GameFlags::Metamod)) {
      table->pfnMessageEnd = [] () {
         engfuncs.pfnMessageEnd ();

         // this allows us to send messages right in handler code
         msgs.stop ();
      };
   }

   table->pfnWriteByte = [] (int value) {
      // if this message is for a bot, call the client message function...
      msgs.collect (value);

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnWriteByte (value);
   };

   table->pfnWriteChar = [] (int value) {
      // if this message is for a bot, call the client message function...
      msgs.collect (value);

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnWriteChar (value);
   };

   table->pfnWriteShort = [] (int value) {
      // if this message is for a bot, call the client message function...
      msgs.collect (value);

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnWriteShort (value);
   };

   table->pfnWriteLong = [] (int value) {
      // if this message is for a bot, call the client message function...
      msgs.collect (value);

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnWriteLong (value);
   };

   table->pfnWriteAngle = [] (float value) {
      // if this message is for a bot, call the client message function...
      msgs.collect (value);

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnWriteAngle (value);
   };

   table->pfnWriteCoord = [] (float value) {
      // if this message is for a bot, call the client message function...
      msgs.collect (value);

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnWriteCoord (value);
   };

   table->pfnWriteString = [] (const char *sz) {
      // if this message is for a bot, call the client message function...
      msgs.collect (sz);

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnWriteString (sz);
   };

   table->pfnWriteEntity = [] (int value) {
      // if this message is for a bot, call the client message function...
      msgs.collect (value);

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnWriteEntity (value);
   };

   if (!game.is (GameFlags::Metamod)) {
      table->pfnRegUserMsg = [] (const char *name, int size) {
         // this function registers a "user message" by the engine side. User messages are network
         // messages the game DLL asks the engine to send to clients. Since many MODs have completely
         // different client features (Counter-Strike has a radar and a timer, for example), network
         // messages just can't be the same for every MOD. Hence here the MOD DLL tells the engine,
         // "Hey, you have to know that I use a network message whose name is pszName and it is size
         // packets long". The engine books it, and returns the ID number under which he recorded that
         // custom message. Thus every time the MOD DLL will be wanting to send a message named pszName
         // using pfnMessageBegin (), it will know what message ID number to send, and the engine will
         // know what to do, only for non-metamod version

         return msgs.add (name, engfuncs.pfnRegUserMsg (name, size)); // return privously registered message
      };
   }

   table->pfnClientPrintf = [] (edict_t *ent, PRINT_TYPE printType, const char *message) {
      // this function prints the text message string pointed to by message by the client side of
      // the client entity pointed to by ent, in a manner depending of printType (print_console,
      // print_center or print_chat). Be certain never to try to feed a bot with this function,
      // as it will crash your server. Why would you, anyway ? bots have no client DLL as far as
      // we know, right ? But since stupidity rules this world, we do a preventive check :)

      if (util.isFakeClient (ent)) {
         if (game.is (GameFlags::Metamod)) {
            RETURN_META (MRES_SUPERCEDE);
         }
         return;
      }

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnClientPrintf (ent, printType, message);
   };

   table->pfnCmd_Args = [] () {
      // this function returns a pointer to the whole current client command string. Since bots
      // have no client DLL and we may want a bot to execute a client command, we had to implement
      // a argv string in the bot DLL for holding the bots' commands, and also keep track of the
      // argument count. Hence this hook not to let the engine ask an unexistent client DLL for a
      // command we are holding here. Of course, real clients commands are still retrieved the
      // normal way, by asking the game.

      // is this a bot issuing that client command?
      if (game.isBotCmd ()) {
         if (game.is (GameFlags::Metamod)) {
            RETURN_META_VALUE (MRES_SUPERCEDE, game.botArgs ());
         }
         return game.botArgs (); // else return the whole bot client command string we know
      }

      if (game.is (GameFlags::Metamod)) {
         RETURN_META_VALUE (MRES_IGNORED, static_cast <const char *> (nullptr));
      }
      return engfuncs.pfnCmd_Args (); // ask the client command string to the engine
   };

   table->pfnCmd_Argv = [] (int argc) {
      // this function returns a pointer to a certain argument of the current client command. Since
      // bots have no client DLL and we may want a bot to execute a client command, we had to
      // implement a argv string in the bot DLL for holding the bots' commands, and also keep
      // track of the argument count. Hence this hook not to let the engine ask an unexistent client
      // DLL for a command we are holding here. Of course, real clients commands are still retrieved
      // the normal way, by asking the game.

      // is this a bot issuing that client command?
      if (game.isBotCmd ()) {
         if (game.is (GameFlags::Metamod)) {
            RETURN_META_VALUE (MRES_SUPERCEDE, game.botArgv (argc));
         }
         return game.botArgv (argc); // if so, then return the wanted argument we know
      }

      if (game.is (GameFlags::Metamod)) {
         RETURN_META_VALUE (MRES_IGNORED, static_cast <const char *> (nullptr));
      }
      return engfuncs.pfnCmd_Argv (argc); // ask the argument number "argc" to the engine
   };

   table->pfnCmd_Argc = [] () {
      // this function returns the number of arguments the current client command string has. Since
      // bots have no client DLL and we may want a bot to execute a client command, we had to
      // implement a argv string in the bot DLL for holding the bots' commands, and also keep
      // track of the argument count. Hence this hook not to let the engine ask an unexistent client
      // DLL for a command we are holding here. Of course, real clients commands are still retrieved
      // the normal way, by asking the game.

      // is this a bot issuing that client command?
      if (game.isBotCmd ()) {
         if (game.is (GameFlags::Metamod)) {
            RETURN_META_VALUE (MRES_SUPERCEDE, game.botArgc ());
         }
         return game.botArgc (); // if so, then return the argument count we know
      }

      if (game.is (GameFlags::Metamod)) {
         RETURN_META_VALUE (MRES_IGNORED, 0);
      }
      return engfuncs.pfnCmd_Argc (); // ask the engine how many arguments there are
   };

   table->pfnSetClientMaxspeed = [] (const edict_t *ent, float newMaxspeed) {
      auto bot = bots[const_cast <edict_t *> (ent)];

      // check wether it's not a bot
      if (bot != nullptr) {
         bot->pev->maxspeed = newMaxspeed;
      }

      if (game.is (GameFlags::Metamod)) {
         RETURN_META (MRES_IGNORED);
      }
      engfuncs.pfnSetClientMaxspeed (ent, newMaxspeed);
   };

   table->pfnClientCommand = variadic::clientCommand;

   return true;
}

CR_EXPORT int GetNewDLLFunctions (newgamefuncs_t *table, int *interfaceVersion) {
   // it appears that an extra function table has been added in the engine to gamedll interface
   // since the date where the first enginefuncs table standard was frozen. These ones are
   // facultative and we don't hook them, but since some MODs might be featuring it, we have to
   // pass them too, else the DLL interfacing wouldn't be complete and the game possibly wouldn't
   // run properly.

   auto api_GetNewDLLFunctions = game.lib ().resolve <int (*) (newgamefuncs_t *, int *)> (__FUNCTION__);

   if (!api_GetNewDLLFunctions || !api_GetNewDLLFunctions (table, interfaceVersion)) {
      logger.error ("Could not resolve symbol \"%s\" in the game dll. Continuing...", __FUNCTION__);
      return false;
   }

   dllfuncs.newapi_table = table;
   return true;
}

CR_LINKAGE_C int GetEngineFunctions_Post (enginefuncs_t *table, int *) {
   plat.bzero (table, sizeof (enginefuncs_t));

   table->pfnMessageEnd = [] () {
      msgs.stop (); // this allows us to send messages right in handler code

      RETURN_META (MRES_IGNORED);
   };
   
   table->pfnRegUserMsg = [] (const char *name, int) {
      // this function registers a "user message" by the engine side. User messages are network
      // messages the game DLL asks the engine to send to clients. Since many MODs have completely
      // different client features (Counter-Strike has a radar and a timer, for example), network
      // messages just can't be the same for every MOD. Hence here the MOD DLL tells the engine,
      // "Hey, you have to know that I use a network message whose name is pszName and it is size
      // packets long". The engine books it, and returns the ID number under which he recorded that
      // custom message. Thus every time the MOD DLL will be wanting to send a message named pszName
      // using pfnMessageBegin (), it will know what message ID number to send, and the engine will
      // know what to do, only for non-metamod version

      // register message for our needs
      msgs.add (name, META_RESULT_ORIG_RET (int));

      RETURN_META_VALUE (MRES_IGNORED, 0);
   };

   return true;
}

CR_EXPORT int Meta_Query (char *, plugin_info_t **pPlugInfo, mutil_funcs_t *pMetaUtilFuncs) {
   // this function is the first function ever called by metamod in the plugin DLL. Its purpose
   // is for metamod to retrieve basic information about the plugin, such as its meta-interface
   // version, for ensuring compatibility with the current version of the running metamod.

   gpMetaUtilFuncs = pMetaUtilFuncs;
   *pPlugInfo = &Plugin_info;

   return true; // tell metamod this plugin looks safe
}

CR_EXPORT int Meta_Attach (PLUG_LOADTIME now, metamod_funcs_t *functionTable, meta_globals_t *pMGlobals, gamedll_funcs_t *pGamedllFuncs) {
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

   if (now > Plugin_info.loadable) {
      logger.error ("%s: plugin NOT attaching (can't load plugin right now)", Plugin_info.name);
      return false; // returning FALSE prevents metamod from attaching this plugin
   }

   // keep track of the pointers to engine function tables metamod gives us
   gpMetaGlobals = pMGlobals;
   memcpy (functionTable, &metamodFunctionTable, sizeof (metamod_funcs_t));
   gpGamedllFuncs = pGamedllFuncs;

   return true; // returning true enables metamod to attach this plugin
}

CR_EXPORT int Meta_Detach (PLUG_LOADTIME now, PL_UNLOAD_REASON reason) {
   // this function is called when metamod unloads the plugin. A basic check is made in order
   // to prevent unloading the plugin if its processing should not be interrupted.

   if (now > Plugin_info.unloadable && reason != PNL_CMD_FORCED) {
      logger.error ("%s: plugin NOT detaching (can't unload plugin right now)", Plugin_info.name);
      return false; // returning FALSE prevents metamod from unloading this plugin
   }
   bots.kickEveryone (true); // kick all bots off this server

   // save collected experience on shutdown
   graph.savePractice ();
   util.disableSendTo ();

   // make sure all stuff cleared
   bots.destroy ();

   return true;
}

CR_EXPORT void Meta_Init () {
   // this function is called by metamod, before any other interface functions. Purpose of this
   // function to give plugin a chance to determine is plugin running under metamod or not.

   game.addGameFlag (GameFlags::Metamod);
}

// games GiveFnptrsToDll is a bit tricky
#if defined(CR_WINDOWS)
#  if defined(CR_CXX_MSVC) || defined (CR_CXX_MSVC)
#     if defined (CR_ARCH_X86)
#        pragma comment(linker, "/EXPORT:GiveFnptrsToDll=_GiveFnptrsToDll@8,@1")
#     endif
#     pragma comment(linker, "/SECTION:.data,RW")
#  endif
#  if defined(CR_CXX_MSVC) && !defined(CR_ARCH_X64)
#     define DLL_GIVEFNPTRSTODLL CR_LINKAGE_C void CR_STDCALL
#  elif defined(CR_CXX_CLANG) || defined(CR_ARCH_X64)
#     define DLL_GIVEFNPTRSTODLL CR_EXPORT void CR_STDCALL
#  endif
#elif defined(CR_LINUX) || defined (CR_OSX) || defined (CR_ANDROID)
#  define DLL_GIVEFNPTRSTODLL CR_EXPORT void
#endif

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
   auto api_GiveFnptrsToDll = game.lib ().resolve <void (CR_STDCALL *) (enginefuncs_t *, globalvars_t *)> (__FUNCTION__);

   if (!api_GiveFnptrsToDll) {
      logger.fatal ("Could not resolve symbol \"%s\" in the game dll.", __FUNCTION__);
   }
   GetEngineFunctions (functionTable, nullptr);

   // initialize dynamic linkents
   ents.initialize ();

   // give the engine functions to the other DLL...
   if (api_GiveFnptrsToDll) {
      api_GiveFnptrsToDll (functionTable, pGlobals);
   }
}

// add linkents for android
#include "android.cpp"
