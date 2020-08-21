//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2020 YaPB Development Team <team@yapb.ru>.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//

#pragma once

// command handler status
CR_DECLARE_SCOPED_ENUM (BotCommandResult,
   Handled = 0, // command successfully handled 
   ListenServer, // command is only avaialble on listen server
   BadFormat // wrong params
)

// bot command manager
class BotControl final : public Singleton <BotControl> {
public:
   using Handler = int (BotControl::*) ();
   using MenuHandler = int (BotControl::*) (int);

public:
   // generic bot command
   struct BotCmd {
      String name, format, help;
      Handler handler = nullptr;

   public:
      BotCmd () = default;
      BotCmd (StringRef name, StringRef format, StringRef help, Handler handler) : name (name), format (format), help (help), handler (cr::move (handler)) { }
   };

   // single bot menu
   struct BotMenu {
      int ident, slots;
      String text;
      MenuHandler handler;

   public:
      BotMenu (int ident, int slots, StringRef text, MenuHandler handler) : ident (ident), slots (slots), text (text), handler (cr::move (handler)) { }
   };

private:
   StringArray m_args;
   Array <BotCmd> m_cmds;
   Array <BotMenu> m_menus;
   IntArray m_campIterator;

   edict_t *m_ent;
   Bot *m_djump;

   bool m_isFromConsole;
   bool m_rapidOutput;
   bool m_isMenuFillCommand;

   int m_menuServerFillTeam;
   int m_interMenuData[4] = { 0, };

public:
   BotControl ();
   ~BotControl () = default;

private:
   int cmdAddBot ();
   int cmdKickBot ();
   int cmdKickBots ();
   int cmdKillBots ();
   int cmdFill ();
   int cmdVote ();
   int cmdWeaponMode ();
   int cmdVersion ();
   int cmdNodeMenu ();
   int cmdMenu ();
   int cmdList ();
   int cmdCvars ();
   int cmdNode ();
   int cmdNodeOn ();
   int cmdNodeOff ();
   int cmdNodeAdd ();
   int cmdNodeAddBasic ();
   int cmdNodeSave ();
   int cmdNodeLoad ();
   int cmdNodeErase ();
   int cmdNodeDelete ();
   int cmdNodeCheck ();
   int cmdNodeCache ();
   int cmdNodeClean ();
   int cmdNodeSetRadius ();
   int cmdNodeSetFlags ();
   int cmdNodeTeleport ();
   int cmdNodePathCreate ();
   int cmdNodePathDelete ();
   int cmdNodePathSetAutoDistance ();
   int cmdNodeAcquireEditor ();
   int cmdNodeReleaseEditor ();
   int cmdNodeUpload ();
   int cmdNodeIterateCamp ();

private:
   int menuMain (int item);
   int menuFeatures (int item);
   int menuControl (int item);
   int menuWeaponMode (int item);
   int menuPersonality (int item);
   int menuDifficulty (int item);
   int menuTeamSelect (int item);
   int menuClassSelect (int item);
   int menuCommands (int item);
   int menuGraphPage1 (int item);
   int menuGraphPage2 (int item);
   int menuGraphRadius (int item);
   int menuGraphType (int item);
   int menuGraphFlag (int item);
   int menuGraphPath (int item);
   int menuAutoPathDistance (int item);
   int menuKickPage1 (int item);
   int menuKickPage2 (int item);
   int menuKickPage3 (int item);
   int menuKickPage4 (int item);

private:
   void enableDrawModels (bool enable);
   void createMenus ();

public:
   bool executeCommands ();
   bool executeMenus ();

   void showMenu (int id);
   void kickBotByMenu (int page);
   void assignAdminRights (edict_t *ent, char *infobuffer);
   void maintainAdminRights ();

public:
   void setFromConsole (bool console) {
      m_isFromConsole = console;
   }

   void setRapidOutput (bool force) {
      m_rapidOutput = force;
   }

   void setIssuer (edict_t *ent) {
      m_ent = ent;
   }

   int intValue (size_t arg) const {
      if (!hasArg (arg)) {
         return 0;
      }
      return m_args[arg].int_ ();
   }

   StringRef strValue (size_t arg) {
      if (!hasArg (arg)) {
         return "";
      }
      return m_args[arg];
   }

   bool hasArg (size_t arg) const {
      return arg < m_args.length ();
   }

   void collectArgs () {
      m_args.clear ();

      for (int i = 0; i < engfuncs.pfnCmd_Argc (); ++i) {
         m_args.emplace (engfuncs.pfnCmd_Argv (i));
      }
   }

   // global heloer for sending message to correct channel
   template <typename ...Args> void msg (const char *fmt, Args &&...args);

public:

   // for the server commands
   void handleEngineCommands ();

   // for the client commands
   bool handleClientCommands (edict_t *ent);

   // for the client menu commands
   bool handleMenuCommands (edict_t *ent);
};

// global heloer for sending message to correct channel
template <typename ...Args> inline void BotControl::msg (const char *fmt, Args &&...args) {
   auto result = strings.format (conf.translate (fmt), cr::forward <Args> (args)...);

   // if no receiver or many message have to appear, just print to server console
   if (game.isNullEntity (m_ent) || m_rapidOutput) {
      game.print (result);
      return;
   }

   if (m_isFromConsole || strlen (result) > 56) {
      game.clientPrint (m_ent, result);
   }
   else {
      game.centerPrint (m_ent, result);
      game.clientPrint (m_ent, result);
   }
}

// explose global
CR_EXPOSE_GLOBAL_SINGLETON (BotControl, ctrl);
