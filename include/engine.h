//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#pragma once

// line draw
enum DrawLineType : int {
   DRAW_SIMPLE,
   DRAW_ARROW,
   DRAW_NUM
};

// trace ignore
enum TraceIgnore : int {
   TRACE_IGNORE_NONE = 0,
   TRACE_IGNORE_GLASS = cr::bit (0),
   TRACE_IGNORE_MONSTERS = cr::bit (1),
   TRACE_IGNORE_EVERYTHING = TRACE_IGNORE_GLASS | TRACE_IGNORE_MONSTERS
};

// variable type
enum VarType : int {
   VT_NORMAL = 0,
   VT_READONLY,
   VT_PASSWORD,
   VT_NOSERVER,
   VT_NOREGISTER
};

// netmessage functions
enum NetMsgId : int {
   NETMSG_UNDEFINED = -1,
   NETMSG_VGUI = 1,
   NETMSG_SHOWMENU = 2,
   NETMSG_WEAPONLIST = 3,
   NETMSG_CURWEAPON = 4,
   NETMSG_AMMOX = 5,
   NETMSG_AMMOPICKUP = 6,
   NETMSG_DAMAGE = 7,
   NETMSG_MONEY = 8,
   NETMSG_STATUSICON = 9,
   NETMSG_DEATH = 10,
   NETMSG_SCREENFADE = 11,
   NETMSG_HLTV = 12,
   NETMSG_TEXTMSG = 13,
   NETMSG_TEAMINFO = 14,
   NETMSG_BARTIME = 15,
   NETMSG_SENDAUDIO = 17,
   NETMSG_SAYTEXT = 18,
   NETMSG_BOTVOICE = 19,
   NETMSG_NVGTOGGLE = 20,
   NETMSG_FLASHBAT = 21,
   NETMSG_FLASHLIGHT = 22,
   NETMSG_ITEMSTATUS = 23,
   NETMSG_NUM = 25
};

// supported cs's
enum GameFlags : int {
   GAME_CSTRIKE16 = cr::bit (0), // counter-strike 1.6 and above
   GAME_XASH_ENGINE = cr::bit (1), // counter-strike 1.6 under the xash engine (additional flag)
   GAME_CZERO = cr::bit (2), // counter-strike: condition zero
   GAME_LEGACY = cr::bit (3), // counter-strike 1.3-1.5 with/without steam
   GAME_MOBILITY = cr::bit (4), // additional flag that bot is running on android (additional flag)
   GAME_OFFICIAL_CSBOT = cr::bit (5), // additional flag that indicates official cs bots are in game
   GAME_METAMOD = cr::bit (6), // game running under meta\mod
   GAME_CSDM = cr::bit (7), // csdm mod currently in use
   GAME_CSDM_FFA = cr::bit (8), // csdm mod with ffa mode
   GAME_REGAMEDLL = cr::bit (9), // server dll is a regamedll
   GAME_SUPPORT_SVC_PINGS = cr::bit (10), // on that game version we can fake bots pings
   GAME_SUPPORT_BOT_VOICE = cr::bit (11) // on that game version we can use chatter
};


// defines map type
enum MapFlags : int {
   MAP_AS = cr::bit (0),
   MAP_CS = cr::bit (1),
   MAP_DE = cr::bit (2),
   MAP_ES = cr::bit (3),
   MAP_KA = cr::bit (4),
   MAP_FY = cr::bit (5),

   // additional flags
   MAP_HAS_DOORS = cr::bit (6)
};

// variable reg pair
struct VarPair {
   VarType type;
   cvar_t reg;
   class ConVar *self;

   bool regMissing;
   const char *regVal;
};

// network message block
struct MessageBlock {
   int bot;
   int state;
   int msg;
   int regMsgs[NETMSG_NUM];
};

// referentia vector info
struct RefVector {
   Vector forward, right, up;
};

// entity prototype
using EntityFunction = void (*) (entvars_t *);

// compare language
struct LangComprarer {
   size_t operator () (const String &key) const {
      char *str = const_cast <char *> (key.chars ());
      size_t hash = key.length ();

      while (*str++) {
         if (!isalpha (*str)) {
            continue;
         }
         hash = ((*str << 5) + hash) + *str;
      }
      return hash;
   }
};

// provides utility functions to not call original engine (less call-cost)
class Game : public Singleton <Game> {
private:
   int m_drawModels[DRAW_NUM];
   int m_spawnCount[TEAM_UNASSIGNED];

   // bot client command
   char m_arguments[256];
   bool m_isBotCommand;
   int m_argumentCount;

   edict_t *m_startEntity;
   edict_t *m_localEntity;

   Array <VarPair> m_cvars;
   HashMap <String, String, LangComprarer> m_language;

   Library m_gameLib;
   MessageBlock m_msgBlock;
   bool m_precached;

   int m_gameFlags;
   int m_mapFlags;

   float m_slowFrame; // per second updated frame
public:
   RefVector vec;

public:
   Game (void);
   ~Game (void);

public:
   // precaches internal stuff
   void precache (void);

   // initialize levels
   void levelInitialize (edict_t *ents, int max);

   // prints data to servers console
   void print (const char *fmt, ...);

   // prints chat message to all players
   void chatPrint (const char *fmt, ...);

   // prints center message to all players
   void centerPrint (const char *fmt, ...);

   // prints center message to specified player
   void centerPrint (edict_t *ent, const char *fmt, ...);

   // prints message to client console
   void clientPrint (edict_t *ent, const char *fmt, ...);

   // display world line
   void drawLine (edict_t *ent, const Vector &start, const Vector &end, int width, int noise, int red, int green, int blue, int brightness, int speed, int life, DrawLineType type = DRAW_SIMPLE);

   // test line
   void testLine (const Vector &start, const Vector &end, int ignoreFlags, edict_t *ignoreEntity, TraceResult *ptr);

   // test line
   void testHull (const Vector &start, const Vector &end, int ignoreFlags, int hullNumber, edict_t *ignoreEntity, TraceResult *ptr);

   // get's the wave length
   float getWaveLen (const char *fileName);

   // we are on dedicated server ?
   bool isDedicated (void);

   // get stripped down mod name
   const char *getModName (void);

   // get the valid mapname
   const char *getMapName (void);

   // get the "any" entity origin
   Vector getAbsPos (edict_t *ent);

   // send server command
   void execCmd (const char *fmt, ...);

   // registers a server command
   void registerCmd (const char *command, void func (void));

   // play's sound to client
   void playSound (edict_t *ent, const char *sound);

   // sends bot command
   void execBotCmd (edict_t *ent, const char *fmt, ...);

   // adds cvar to registration stack
   void pushVarToRegStack (const char *variable, const char *value, VarType varType, bool regMissing, const char *regVal, ConVar *self);

   // sends local registration stack for engine registration
   void pushRegStackToEngine (bool gameVars = false);

   // translates bot message into needed language
   const char *translate (const char *input);

   // do actual network message processing
   void processMessages (void *ptr);

   // checks whether softwared rendering is enabled
   bool isSoftwareRenderer (void);

   // load the cs binary in non metamod mode
   bool loadCSBinary (void);

   // do post-load stuff
   bool postload (void);

   // detects if csdm mod is in use
   void detectDeathmatch (void);

   // executes stuff every 1 second
   void slowFrame (void);

   // begin message handler
   void beginMessage (edict_t *ent, int dest, int type);

   // public inlines
public:
   // get the current time on server
   inline float timebase (void) {
      return globals->time;
   }

   // get "maxplayers" limit on server
   inline int maxClients (void) {
      return globals->maxClients;
   }

   // get the fakeclient command interface
   inline bool isBotCmd (void) {
      return m_isBotCommand;
   }

   // gets custom engine args for client command
   inline const char *botArgs (void) {
      if (strncmp ("say ", m_arguments, 4) == 0) {
         return &m_arguments[4];
      }
      else if (strncmp ("say_team ", m_arguments, 9) == 0) {
         return &m_arguments[9];
      }
      return m_arguments;
   }

   // gets custom engine argv for client command
   inline const char *botArgv (int num) {
      return getField (m_arguments, static_cast <size_t> (num));
   }

   // gets custom engine argc for client command
   inline int botArgc (void) {
      return m_argumentCount;
   }

   // gets edict pointer out of entity index
   inline edict_t *entityOfIndex (const int index) {
      return static_cast <edict_t *> (m_startEntity + index);
   };

   // gets edict index out of it's pointer
   inline int indexOfEntity (const edict_t *ent) {
      return static_cast <int> (ent - m_startEntity);
   };

   // verify entity isn't null
   inline bool isNullEntity (const edict_t *ent) {
      return !ent || !indexOfEntity (ent) || ent->free;
   }

   // get the wroldspawn entity
   inline edict_t *getStartEntity (void) {
      return m_startEntity;
   }

   // get spawn count for team
   inline int getSpawnCount (int team) {
      return m_spawnCount[team];
   }

   // gets the player team
   int getTeam (edict_t *ent);

   // adds translation pair from config
   inline void addTranslation (const String &original, const String &translated) {
      m_language.put (original, translated);
   }

   // resets the message capture mechanism
   inline void resetMessages (void) {
      m_msgBlock.msg = NETMSG_UNDEFINED;
      m_msgBlock.state = 0;
      m_msgBlock.bot = 0;
   };

   // sets the currently executed message
   inline void setCurrentMessageId (int message) {
      m_msgBlock.msg = message;
   }

   // set the bot entity that receive this message
   inline void setCurrentMessageOwner (int id) {
      m_msgBlock.bot = id;
   }

   // find registered message id
   inline int getMessageId (int type) {
      return m_msgBlock.regMsgs[type];
   }

   // assigns message id for message type
   inline void setMessageId (int type, int id) {
      m_msgBlock.regMsgs[type] = id;
   }

   // tries to set needed message id
   inline void captureMessage (int type, int msgId) {
      if (type == m_msgBlock.regMsgs[msgId]) {
         setCurrentMessageId (msgId);
      }
   }

   // sets the precache to uninitialize
   inline void setUnprecached (void) {
      m_precached = false;
   }

   // gets the local entity (host edict)
   inline edict_t *getLocalEntity (void) {
      return m_localEntity;
   }

   // sets the local entity (host edict)
   inline void setLocalEntity (edict_t *ent) {
      m_localEntity = ent;
   }

   //  builds referential vector
   inline void makeVectors (const Vector &in) {
      in.makeVectors (&vec.forward, &vec.right, &vec.up);
   }

   // what kind of map we're running ?
   inline bool isMap (const int map) const {
      return (m_mapFlags & map) == map;
   }

   // what kind of game engine / game dll / mod / tool we're running ?
   inline bool is (const int type) const {
      return (m_gameFlags & type) == type;
   }

   // adds game flag
   inline void addGameFlag (const int type) {
      m_gameFlags |= type;
   }

   // gets the map type
   inline bool mapIs (const int type) const {
      return (m_mapFlags & type) == type;
   }

   // get loaded gamelib
   inline Library &getLib (void) {
      return m_gameLib;
   }

   // static utility functions
private:
   const char *getField (const char *string, size_t id);
};

// simplify access for console variables
class ConVar {
public:
   cvar_t *m_eptr;

public:
   ConVar (const char *name, const char *initval, VarType type = VT_NOSERVER, bool regMissing = false, const char *regVal = nullptr) : m_eptr (nullptr) {
      Game::ref ().pushVarToRegStack (name, initval, type, regMissing, regVal, this);
   }

   inline bool boolean (void) const {
      return m_eptr->value > 0.0f;
   }

   inline int integer (void) const {
      return static_cast <int> (m_eptr->value);
   }

   inline float flt (void) const {
      return m_eptr->value;
   }

   inline const char *str (void) const {
      return m_eptr->string;
   }

   inline void set (float val) const {
      engfuncs.pfnCVarSetFloat (m_eptr->name, val);
   }

   inline void set (int val) const {
      set (static_cast <float> (val));
   }

   inline void set (const char *val) const {
      engfuncs.pfnCvar_DirectSet (m_eptr, const_cast <char *> (val));
   }
};

class MessageWriter {
private:
   bool m_autoDestruct { false };

public:
   MessageWriter (void) = default;

   MessageWriter (int dest, int type, const Vector &pos = Vector::null (), edict_t *to = nullptr) {
      start (dest, type, pos, to);
      m_autoDestruct = true;
   }

   virtual ~MessageWriter (void) {
      if (m_autoDestruct) {
         end ();
      }
   }

public:
   MessageWriter &start (int dest, int type, const Vector &pos = Vector::null (), edict_t *to = nullptr) {
      engfuncs.pfnMessageBegin (dest, type, pos, to);
      return *this;
   }

   void end (void) {
      engfuncs.pfnMessageEnd ();
   }

   MessageWriter &writeByte (int val) {
      engfuncs.pfnWriteByte (val);
      return *this;
   }

   MessageWriter &writeChar (int val) {
      engfuncs.pfnWriteChar (val);
      return *this;
   }

   MessageWriter &writeShort (int val) {
      engfuncs.pfnWriteShort (val);
      return *this;
   }

   MessageWriter &writeCoord (float val) {
      engfuncs.pfnWriteCoord (val);
      return *this;
   }

   MessageWriter &writeString (const char *val) {
      engfuncs.pfnWriteString (val);
      return *this;
   }

public:
   static inline uint16 fu16 (float value, float scale) {
      return cr::clamp <uint16> (static_cast <uint16> (value * scale), 0, 0xffff);
   }

   static inline short fs16 (float value, float scale) {
      return cr::clamp <short> (static_cast <short> (value * scale), -32767, 32767);
   }
};

class LightMeasure final : public Singleton <LightMeasure> {
private:
   lightstyle_t m_lightstyle[MAX_LIGHTSTYLES];
   int m_lightstyleValue[MAX_LIGHTSTYLEVALUE];
   bool m_doAnimation;

   SimpleColor m_point;
   model_t *m_worldModel;

public:
   LightMeasure (void) : m_doAnimation (false), m_worldModel (nullptr) {
      initializeLightstyles ();

      m_point.reset ();
   }

public:
   void initializeLightstyles (void);
   void animateLight (void);

   float getLightLevel (const Vector &point);
   float getSkyColor (void);

private:
   template <typename S, typename M> bool recursiveLightPoint (const M *node, const Vector &start, const Vector &end);

public:
   inline void resetWorldModel (void) {
      m_worldModel = nullptr;
   }

   inline void setWorldModel (model_t *model) {
      if (m_worldModel) {
         return;
      }
      m_worldModel = model;
   }

   inline void enableAnimation (bool enable) {
      m_doAnimation = enable;
   }
};