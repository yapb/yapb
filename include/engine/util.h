/***
 *
 *   Copyright (c) 1999-2005, Valve Corporation. All rights reserved.
 *
 *   This product contains software technology licensed from Id
 *   Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *   All Rights Reserved.
 *
 *   This source code contains proprietary and confidential information of
 *   Valve LLC and its suppliers.  Access to this code is restricted to
 *   persons who have executed a written SDK license with Valve.  Any access,
 *   use or distribution of this code by or to any unlicensed person is illegal.
 *
 ****/

#ifndef SDKUTIL_H
#define SDKUTIL_H

extern globalvars_t *g_pGlobals;
extern enginefuncs_t g_engfuncs;

// Use this instead of ALLOC_STRING on constant strings
#define STRING(offset) (const char *)(g_pGlobals->pStringBase + (int)offset)

// form fwgs-hlsdk
static inline int MAKE_STRING (const char *val) {
   long long ptrdiff = val - STRING (0);

   if (ptrdiff > INT_MAX || ptrdiff < INT_MIN) {
      return g_engfuncs.pfnAllocString (val);
   }
   return static_cast <int> (ptrdiff);
}

#define ENGINE_STR(str) (const_cast <char *> (STRING (g_engfuncs.pfnAllocString (str))))

// Dot products for view cone checking
#define VIEW_FIELD_FULL (float)-1.0 // +-180 degrees
#define VIEW_FIELD_WIDE (float)-0.7 // +-135 degrees 0.1 // +-85 degrees, used for full FOV checks
#define VIEW_FIELD_NARROW (float)0.7 // +-45 degrees, more narrow check used to set up ranged attacks
#define VIEW_FIELD_ULTRA_NARROW (float)0.9 // +-25 degrees, more narrow check used to set up ranged attacks

typedef enum { ignore_monsters = 1, dont_ignore_monsters = 0, missile = 2 } IGNORE_MONSTERS;
typedef enum { ignore_glass = 1, dont_ignore_glass = 0 } IGNORE_GLASS;
typedef enum { point_hull = 0, human_hull = 1, large_hull = 2, head_hull = 3 } HULL;

typedef struct hudtextparms_s {
   float x;
   float y;
   int effect;
   uint8 r1, g1, b1, a1;
   uint8 r2, g2, b2, a2;
   float fadeinTime;
   float fadeoutTime;
   float holdTime;
   float fxTime;
   int channel;
} hudtextparms_t;

#define AMBIENT_SOUND_STATIC 0 // medium radius attenuation
#define AMBIENT_SOUND_EVERYWHERE 1
#define AMBIENT_SOUND_SMALLRADIUS 2
#define AMBIENT_SOUND_MEDIUMRADIUS 4
#define AMBIENT_SOUND_LARGERADIUS 8
#define AMBIENT_SOUND_START_SILENT 16
#define AMBIENT_SOUND_NOT_LOOPING 32

#define SPEAKER_START_SILENT 1 // wait for trigger 'on' to start announcements

#define SND_SPAWNING (1 << 8) // duplicated in protocol.h we're spawing, used in some cases for ambients
#define SND_STOP (1 << 5) // duplicated in protocol.h stop sound
#define SND_CHANGE_VOL (1 << 6) // duplicated in protocol.h change sound vol
#define SND_CHANGE_PITCH (1 << 7) // duplicated in protocol.h change sound pitch

#define LFO_SQUARE 1
#define LFO_TRIANGLE 2
#define LFO_RANDOM 3

// func_rotating
#define SF_BRUSH_ROTATE_Y_AXIS 0
#define SF_BRUSH_ROTATE_INSTANT 1
#define SF_BRUSH_ROTATE_BACKWARDS 2
#define SF_BRUSH_ROTATE_Z_AXIS 4
#define SF_BRUSH_ROTATE_X_AXIS 8
#define SF_PENDULUM_AUTO_RETURN 16
#define SF_PENDULUM_PASSABLE 32

#define SF_BRUSH_ROTATE_SMALLRADIUS 128
#define SF_BRUSH_ROTATE_MEDIUMRADIUS 256
#define SF_BRUSH_ROTATE_LARGERADIUS 512

#define PUSH_BLOCK_ONLY_X 1
#define PUSH_BLOCK_ONLY_Y 2

#define VEC_HULL_MIN Vector (-16, -16, -36)
#define VEC_HULL_MAX Vector (16, 16, 36)
#define VEC_HUMAN_HULL_MIN Vector (-16, -16, 0)
#define VEC_HUMAN_HULL_MAX Vector (16, 16, 72)
#define VEC_HUMAN_HULL_DUCK Vector (16, 16, 36)

#define VEC_VIEW Vector (0, 0, 28)

#define VEC_DUCK_HULL_MIN Vector (-16, -16, -18)
#define VEC_DUCK_HULL_MAX Vector (16, 16, 18)
#define VEC_DUCK_VIEW Vector (0, 0, 12)

#define SVC_TEMPENTITY 23
#define SVC_CENTERPRINT 26
#define SVC_INTERMISSION 30
#define SVC_CDTRACK 32
#define SVC_WEAPONANIM 35
#define SVC_ROOMTYPE 37
#define SVC_DIRECTOR 51

// triggers
#define SF_TRIGGER_ALLOWMONSTERS 1 // monsters allowed to fire this trigger
#define SF_TRIGGER_NOCLIENTS 2 // players not allowed to fire this trigger
#define SF_TRIGGER_PUSHABLES 4 // only pushables can fire this trigger

// func breakable
#define SF_BREAK_TRIGGER_ONLY 1 // may only be broken by trigger
#define SF_BREAK_TOUCH 2 // can be 'crashed through' by running player (plate glass)
#define SF_BREAK_PRESSURE 4 // can be broken by a player standing on it
#define SF_BREAK_CROWBAR 256 // instant break if hit with crowbar

// func_pushable (it's also func_breakable, so don't collide with those flags)
#define SF_PUSH_BREAKABLE 128

#define SF_LIGHT_START_OFF 1

#define SPAWNFLAG_NOMESSAGE 1
#define SPAWNFLAG_NOTOUCH 1
#define SPAWNFLAG_DROIDONLY 4

#define SPAWNFLAG_USEONLY 1 // can't be touched, must be used (buttons)

#define TELE_PLAYER_ONLY 1
#define TELE_SILENT 2

#define SF_TRIG_PUSH_ONCE 1


#endif
