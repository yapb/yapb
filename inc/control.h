//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2023 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

// command handler status
CR_DECLARE_SCOPED_ENUM (BotCommandResult,
   Handled = 0, // command successfully handled 
   ListenServer, // command is only avaialble on listen server
   BadFormat // wrong params
)

// print queue destination
CR_DECLARE_SCOPED_ENUM (PrintQueueDestination,
   ServerConsole, // use server console
   ClientConsole // use client console
);

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
      explicit BotCmd () = default;

      BotCmd (StringRef name, StringRef format, StringRef help, Handler handler) : name (name), format (format), help (help), handler (cr::move (handler)) 
      { }
   };

   // single bot menu
   struct BotMenu {
      int ident, slots;
      String text;
      MenuHandler handler;

   public:
      explicit BotMenu (int ident, int slots, StringRef text, MenuHandler handler) : ident (ident), slots (slots), text (text), handler (cr::move (handler))
      { }
   };

   // queued text message to prevent overflow with rapid output
   struct PrintQueue {
      int32_t destination {};
      String text;

   public:
     explicit PrintQueue () = default;

      PrintQueue (int32_t destination, StringRef text) : destination (destination), text (text) 
      { }
   };

private:
   StringArray m_args {};
   Array <BotCmd> m_cmds {};
   Array <BotMenu> m_menus {};
   Deque <PrintQueue> m_printQueue {};
   IntArray m_campIterator {};

   edict_t *m_ent {};
   Bot *m_djump {};

   bool m_isFromConsole {};
   bool m_rapidOutput {};
   bool m_isMenuFillCommand {};
   bool m_ignoreTranslate {};

   int m_menuServerFillTeam {};
   int m_interMenuData[4] = { 0, };

   float m_printQueueFlushTimestamp {};

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
   int cmdShowCustom ();
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
   int cmdNodeShowStats ();
   int cmdNodeFileInfo ();
   int cmdAdjustHeight ();

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
   int menuGraphDebug (int item);
   int menuGraphFlag (int item);
   int menuGraphPath (int item);
   int menuCampDirections (int item);
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
   void closeMenu ();

   void kickBotByMenu (int page);
   void assignAdminRights (edict_t *ent, char *infobuffer);
   void maintainAdminRights ();
   void flushPrintQueue ();

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

   void resetFlushTimestamp () {
      m_printQueueFlushTimestamp = 0.0f;
   }

   int intValue (size_t arg) const {
      if (!hasArg (arg)) {
         return 0;
      }
      return m_args[arg].int_ ();
   }

   float floatValue (size_t arg) const {
      if (!hasArg (arg)) {
         return 0.0f;
      }
      return m_args[arg].float_ ();
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

   bool ignoreTranslate () const {
      return m_ignoreTranslate;
   }

   void collectArgs () {
      m_args.clear ();

      for (int i = 0; i < engfuncs.pfnCmd_Argc (); ++i) {
         m_args.emplace (engfuncs.pfnCmd_Argv (i));
      }
   }

   edict_t *getIssuer() {
      return m_ent;
   }

   // global heloer for sending message to correct channel
   template <typename ...Args> void msg (const char *fmt, Args &&...args);

public:

   // for the server commands
   void handleEngineCommands ();

   // wrapper for menus and commands
   bool handleClientSideCommandsWrapper (edict_t *ent, bool isMenus);

   // for the client commands
   bool handleClientCommands (edict_t *ent);

   // for the client menu commands
   bool handleMenuCommands (edict_t *ent);
};

// global heloer for sending message to correct channel
template <typename ...Args> inline void BotControl::msg (const char *fmt, Args &&...args) {
   m_ignoreTranslate = game.isDedicated () && game.isNullEntity (m_ent);

   auto result = strings.format (conf.translate (fmt), cr::forward <Args> (args)...);

   // if no receiver or many message have to appear, just print to server console
   if (game.isNullEntity (m_ent)) {

      if (m_rapidOutput) {
         m_printQueue.emplaceLast (PrintQueueDestination::ServerConsole, result);
      }
      else {
         game.print (result); // print the info
      }
      return;
   }

   if (m_isFromConsole || strnlen (result, StringBuffer::StaticBufferSize) > 96 || m_rapidOutput) {
      if (m_rapidOutput) {
         m_printQueue.emplaceLast (PrintQueueDestination::ClientConsole, result);
      }
      else {
         game.clientPrint (m_ent, result);
      }
   }
   else {
      game.centerPrint (m_ent, result);
      game.clientPrint (m_ent, result);
   }
}

// explose global
CR_EXPOSE_GLOBAL_SINGLETON (BotControl, ctrl);
