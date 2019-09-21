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

#ifndef CONST_H
#define CONST_H
//
// Constants shared by the engine and dlls
// This header file included by engine files and DLL files.
// Most came from server.h

#define INTERFACE_VERSION 140

// Dot products for view cone checking
#define VIEW_FIELD_FULL (float)-1.0 // +-180 degrees
#define VIEW_FIELD_WIDE (float)-0.7 // +-135 degrees 0.1 // +-85 degrees, used for full FOV checks
#define VIEW_FIELD_NARROW (float)0.7 // +-45 degrees, more narrow check used to set up ranged attacks
#define VIEW_FIELD_ULTRA_NARROW (float)0.9 // +-25 degrees, more narrow check used to set up ranged attacks

#define FCVAR_ARCHIVE (1 << 0) // set to cause it to be saved to vars.rc
#define FCVAR_USERINFO (1 << 1) // changes the client's info string
#define FCVAR_SERVER (1 << 2) // notifies players when changed
#define FCVAR_EXTDLL (1 << 3) // defined by external DLL
#define FCVAR_CLIENTDLL (1 << 4) // defined by the client dll
#define FCVAR_PROTECTED (1 << 5) // It's a server cvar, but we don't send the data since it's a password, etc.  Sends 1 if it's not bland/zero, 0 otherwise as value
#define FCVAR_SPONLY (1 << 6) // This cvar cannot be changed by clients connected to a multiplayer server.
#define FCVAR_PRINTABLEONLY (1 << 7) // This cvar's string cannot contain unprintable characters ( e.g., used for player name etc ).
#define FCVAR_UNLOGGED (1 << 8) // If this is a FCVAR_SERVER, don't log changes to the log file / console if we are creating a log

// edict->flags
#define FL_FLY (1 << 0) // Changes the SV_Movestep() behavior to not need to be on ground
#define FL_SWIM (1 << 1) // Changes the SV_Movestep() behavior to not need to be on ground (but stay in water)
#define FL_CONVEYOR (1 << 2)
#define FL_CLIENT (1 << 3)
#define FL_INWATER (1 << 4)
#define FL_MONSTER (1 << 5)
#define FL_GODMODE (1 << 6)
#define FL_NOTARGET (1 << 7)
#define FL_SKIPLOCALHOST (1 << 8) // Don't send entity to local host, it's predicting this entity itself
#define FL_ONGROUND (1 << 9) // At rest / on the ground
#define FL_PARTIALGROUND (1 << 10) // not all corners are valid
#define FL_WATERJUMP (1 << 11) // player jumping out of water
#define FL_FROZEN (1 << 12) // Player is frozen for 3rd person camera
#define FL_FAKECLIENT (1 << 13) // JAC: fake client, simulated server side; don't send network messages to them
#define FL_DUCKING (1 << 14) // Player flag -- Player is fully crouched
#define FL_FLOAT (1 << 15) // Apply floating force to this entity when in water
#define FL_GRAPHED (1 << 16) // worldgraph has this ent listed as something that blocks a connection

// UNDONE: Do we need these?
#define FL_IMMUNE_WATER (1 << 17)
#define FL_IMMUNE_SLIME (1 << 18)
#define FL_IMMUNE_LAVA (1 << 19)

#define FL_PROXY (1 << 20) // This is a spectator proxy
#define FL_ALWAYSTHINK (1 << 21) // Brush model flag -- call think every frame regardless of nextthink - ltime (for constantly changing velocity/path)
#define FL_BASEVELOCITY (1 << 22) // Base velocity has been applied this frame (used to convert base velocity into momentum)
#define FL_MONSTERCLIP (1 << 23) // Only collide in with monsters who have FL_MONSTERCLIP set
#define FL_ONTRAIN (1 << 24) // Player is _controlling_ a train, so movement commands should be ignored on client during prediction.
#define FL_WORLDBRUSH (1 << 25) // Not moveable/removeable brush entity (really part of the world, but represented as an entity for transparency or something)
#define FL_SPECTATOR (1 << 26) // This client is a spectator, don't run touch functions, etc.
#define FL_CUSTOMENTITY (1 << 29) // This is a custom entity
#define FL_KILLME (1 << 30) // This entity is marked for death -- This allows the engine to kill ents at the appropriate time
#define FL_DORMANT (1 << 31) // Entity is dormant, no updates to client

// Goes into globalvars_t.trace_flags
#define FTRACE_SIMPLEBOX (1 << 0) // Traceline with a simple box

// walkmove modes
#define WALKMOVE_NORMAL 0 // normal walkmove
#define WALKMOVE_WORLDONLY 1 // doesn't hit ANY entities, no matter what the solid type
#define WALKMOVE_CHECKONLY 2 // move, but don't touch triggers

// edict->movetype values
#define MOVETYPE_NONE 0 // never moves
#define MOVETYPE_WALK 3 // Player only - moving on the ground
#define MOVETYPE_STEP 4 // gravity, special edge handling -- monsters use this
#define MOVETYPE_FLY 5 // No gravity, but still collides with stuff
#define MOVETYPE_TOSS 6 // gravity/collisions
#define MOVETYPE_PUSH 7 // no clip to world, push and crush
#define MOVETYPE_NOCLIP 8 // No gravity, no collisions, still do velocity/avelocity
#define MOVETYPE_FLYMISSILE 9 // extra size to monsters
#define MOVETYPE_BOUNCE 10 // Just like Toss, but reflect velocity when contacting surfaces
#define MOVETYPE_BOUNCEMISSILE 11 // bounce w/o gravity
#define MOVETYPE_FOLLOW 12 // track movement of aiment
#define MOVETYPE_PUSHSTEP 13 // BSP model that needs physics/world collisions (uses nearest hull for world collision)

// edict->solid values
// NOTE: Some movetypes will cause collisions independent of SOLID_NOT/SOLID_TRIGGER when the entity moves
// SOLID only effects OTHER entities colliding with this one when they move - UGH!
#define SOLID_NOT 0 // no interaction with other objects
#define SOLID_TRIGGER 1 // touch on edge, but not blocking
#define SOLID_BBOX 2 // touch on edge, block
#define SOLID_SLIDEBOX 3 // touch on edge, but not an onground
#define SOLID_BSP 4 // bsp clip, touch on edge, block

// edict->deadflag values
#define DEAD_NO 0 // alive
#define DEAD_DYING 1 // playing death animation or still falling off of a ledge waiting to hit ground
#define DEAD_DEAD 2 // dead. lying still.
#define DEAD_RESPAWNABLE 3
#define DEAD_DISCARDBODY 4

#define DAMAGE_NO 0
#define DAMAGE_YES 1
#define DAMAGE_AIM 2

// entity effects
#define EF_BRIGHTFIELD 1 // swirling cloud of particles
#define EF_MUZZLEFLASH 2 // single frame ELIGHT on entity attachment 0
#define EF_BRIGHTLIGHT 4 // DLIGHT centered at entity origin
#define EF_DIMLIGHT 8 // player flashlight
#define EF_INVLIGHT 16 // get lighting from ceiling
#define EF_NOINTERP 32 // don't interpolate the next frame
#define EF_LIGHT 64 // rocket flare glow sprite
#define EF_NODRAW 128 // don't draw entity

// entity flags
#define EFLAG_SLERP 1 // do studio interpolation of this entity

//
// temp entity events
//
#define TE_BEAMPOINTS 0 // beam effect between two points
// coord coord coord (start position)
// coord coord coord (end position)
// short (sprite index)
// byte (starting frame)
// byte (frame rate in 0.1's)
// byte (life in 0.1's)
// byte (line width in 0.1's)
// byte (noise amplitude in 0.01's)
// byte,byte,byte (color)
// byte (brightness)
// byte (scroll speed in 0.1's)

#define TE_BEAMENTPOINT 1 // beam effect between point and entity
// short (start entity)
// coord coord coord (end position)
// short (sprite index)
// byte (starting frame)
// byte (frame rate in 0.1's)
// byte (life in 0.1's)
// byte (line width in 0.1's)
// byte (noise amplitude in 0.01's)
// byte,byte,byte (color)
// byte (brightness)
// byte (scroll speed in 0.1's)

#define TE_GUNSHOT 2 // particle effect plus ricochet sound
// coord coord coord (position)

#define TE_EXPLOSION 3 // additive sprite, 2 dynamic lights, flickering particles, explosion sound, move vertically 8 pps
// coord coord coord (position)
// short (sprite index)
// byte (scale in 0.1's)
// byte (framerate)
// byte (flags)
//
// The Explosion effect has some flags to control performance/aesthetic features:
#define TE_EXPLFLAG_NONE 0 // all flags clear makes default Half-Life explosion
#define TE_EXPLFLAG_NOADDITIVE 1 // sprite will be drawn opaque (ensure that the sprite you send is a non-additive sprite)
#define TE_EXPLFLAG_NODLIGHTS 2 // do not render dynamic lights
#define TE_EXPLFLAG_NOSOUND 4 // do not play client explosion sound
#define TE_EXPLFLAG_NOPARTICLES 8 // do not draw particles

#define TE_TAREXPLOSION 4 // Quake1 "tarbaby" explosion with sound
// coord coord coord (position)

#define TE_SMOKE 5 // alphablend sprite, move vertically 30 pps
// coord coord coord (position)
// short (sprite index)
// byte (scale in 0.1's)
// byte (framerate)

#define TE_TRACER 6 // tracer effect from point to point
// coord, coord, coord (start)
// coord, coord, coord (end)

#define TE_LIGHTNING 7 // TE_BEAMPOINTS with simplified parameters
// coord, coord, coord (start)
// coord, coord, coord (end)
// byte (life in 0.1's)
// byte (width in 0.1's)
// byte (amplitude in 0.01's)
// short (sprite model index)

#define TE_BEAMENTS 8
// short (start entity)
// short (end entity)
// short (sprite index)
// byte (starting frame)
// byte (frame rate in 0.1's)
// byte (life in 0.1's)
// byte (line width in 0.1's)
// byte (noise amplitude in 0.01's)
// byte,byte,byte (color)
// byte (brightness)
// byte (scroll speed in 0.1's)

#define TE_SPARKS 9 // 8 random tracers with gravity, ricochet sprite
// coord coord coord (position)

#define TE_LAVASPLASH 10 // Quake1 lava splash
// coord coord coord (position)

#define TE_TELEPORT 11 // Quake1 teleport splash
// coord coord coord (position)

#define TE_EXPLOSION2 12 // Quake1 colormaped (base palette) particle explosion with sound
// coord coord coord (position)
// byte (starting color)
// byte (num colors)

#define TE_BSPDECAL 13 // Decal from the .BSP file
// coord, coord, coord (x,y,z), decal position (center of texture in world)
// short (texture index of precached decal texture name)
// short (entity index)
// [optional - only included if previous short is non-zero (not the world)] short (index of model of above entity)

#define TE_IMPLOSION 14 // tracers moving toward a point
// coord, coord, coord (position)
// byte (radius)
// byte (count)
// byte (life in 0.1's)

#define TE_SPRITETRAIL 15 // line of moving glow sprites with gravity, fadeout, and collisions
// coord, coord, coord (start)
// coord, coord, coord (end)
// short (sprite index)
// byte (count)
// byte (life in 0.1's)
// byte (scale in 0.1's)
// byte (velocity along Vector in 10's)
// byte (randomness of velocity in 10's)

#define TE_BEAM 16 // obsolete

#define TE_SPRITE 17 // additive sprite, plays 1 cycle
// coord, coord, coord (position)
// short (sprite index)
// byte (scale in 0.1's)
// byte (brightness)

#define TE_BEAMSPRITE 18 // A beam with a sprite at the end
// coord, coord, coord (start position)
// coord, coord, coord (end position)
// short (beam sprite index)
// short (end sprite index)

#define TE_BEAMTORUS 19 // screen aligned beam ring, expands to max radius over lifetime
// coord coord coord (center position)
// coord coord coord (axis and radius)
// short (sprite index)
// byte (starting frame)
// byte (frame rate in 0.1's)
// byte (life in 0.1's)
// byte (line width in 0.1's)
// byte (noise amplitude in 0.01's)
// byte,byte,byte (color)
// byte (brightness)
// byte (scroll speed in 0.1's)

#define TE_BEAMDISK 20 // disk that expands to max radius over lifetime
// coord coord coord (center position)
// coord coord coord (axis and radius)
// short (sprite index)
// byte (starting frame)
// byte (frame rate in 0.1's)
// byte (life in 0.1's)
// byte (line width in 0.1's)
// byte (noise amplitude in 0.01's)
// byte,byte,byte (color)
// byte (brightness)
// byte (scroll speed in 0.1's)

#define TE_BEAMCYLINDER 21 // cylinder that expands to max radius over lifetime
// coord coord coord (center position)
// coord coord coord (axis and radius)
// short (sprite index)
// byte (starting frame)
// byte (frame rate in 0.1's)
// byte (life in 0.1's)
// byte (line width in 0.1's)
// byte (noise amplitude in 0.01's)
// byte,byte,byte (color)
// byte (brightness)
// byte (scroll speed in 0.1's)

#define TE_BEAMFOLLOW 22 // create a line of decaying beam segments until entity stops moving
// short (entity:attachment to follow)
// short (sprite index)
// byte (life in 0.1's)
// byte (line width in 0.1's)
// byte,byte,byte (color)
// byte (brightness)

#define TE_GLOWSPRITE 23
// coord, coord, coord (pos) short (model index) byte (scale / 10)

#define TE_BEAMRING 24 // connect a beam ring to two entities
// short (start entity)
// short (end entity)
// short (sprite index)
// byte (starting frame)
// byte (frame rate in 0.1's)
// byte (life in 0.1's)
// byte (line width in 0.1's)
// byte (noise amplitude in 0.01's)
// byte,byte,byte (color)
// byte (brightness)
// byte (scroll speed in 0.1's)

#define TE_STREAK_SPLASH 25 // oriented shower of tracers
// coord coord coord (start position)
// coord coord coord (direction Vector)
// byte (color)
// short (count)
// short (base speed)
// short (ramdon velocity)

#define TE_BEAMHOSE 26 // obsolete

#define TE_DLIGHT 27 // dynamic light, effect world, minor entity effect
// coord, coord, coord (pos)
// byte (radius in 10's)
// byte byte byte (color)
// byte (brightness)
// byte (life in 10's)
// byte (decay rate in 10's)

#define TE_ELIGHT 28 // point entity light, no world effect
// short (entity:attachment to follow)
// coord coord coord (initial position)
// coord (radius)
// byte byte byte (color)
// byte (life in 0.1's)
// coord (decay rate)

#define TE_TEXTMESSAGE 29
// short 1.2.13 x (-1 = center)
// short 1.2.13 y (-1 = center)
// byte Effect 0 = fade in/fade out
// 1 is flickery credits
// 2 is write out (training room)

// 4 bytes r,g,b,a color1   (text color)
// 4 bytes r,g,b,a color2   (effect color)
// ushort 8.8 fadein time
// ushort 8.8  fadeout time
// ushort 8.8 hold time
// optional ushort 8.8 fxtime   (time the highlight lags behing the leading text in effect 2)
// string text message       (512 chars max sz string)
#define TE_LINE 30
// coord, coord, coord        startpos
// coord, coord, coord        endpos
// short life in 0.1 s
// 3 bytes r, g, b

#define TE_BOX 31
// coord, coord, coord        boxmins
// coord, coord, coord        boxmaxs
// short life in 0.1 s
// 3 bytes r, g, b

#define TE_KILLBEAM 99 // kill all beams attached to entity
// short (entity)

#define TE_LARGEFUNNEL 100
// coord coord coord (funnel position)
// short (sprite index)
// short (flags)

#define TE_BLOODSTREAM 101 // particle spray
// coord coord coord (start position)
// coord coord coord (spray Vector)
// byte (color)
// byte (speed)

#define TE_SHOWLINE 102 // line of particles every 5 units, dies in 30 seconds
// coord coord coord (start position)
// coord coord coord (end position)

#define TE_BLOOD 103 // particle spray
// coord coord coord (start position)
// coord coord coord (spray Vector)
// byte (color)
// byte (speed)

#define TE_DECAL 104 // Decal applied to a brush entity (not the world)
// coord, coord, coord (x,y,z), decal position (center of texture in world)
// byte (texture index of precached decal texture name)
// short (entity index)

#define TE_FIZZ 105 // create alpha sprites inside of entity, float upwards
// short (entity)
// short (sprite index)
// byte (density)

#define TE_MODEL 106 // create a moving model that bounces and makes a sound when it hits
// coord, coord, coord (position)
// coord, coord, coord (velocity)
// angle (initial yaw)
// short (model index)
// byte (bounce sound type)
// byte (life in 0.1's)

#define TE_EXPLODEMODEL 107 // spherical shower of models, picks from set
// coord, coord, coord (origin)
// coord (velocity)
// short (model index)
// short (count)
// byte (life in 0.1's)

#define TE_BREAKMODEL 108 // box of models or sprites
// coord, coord, coord (position)
// coord, coord, coord (size)
// coord, coord, coord (velocity)
// byte (random velocity in 10's)
// short (sprite or model index)
// byte (count)
// byte (life in 0.1 secs)
// byte (flags)

#define TE_GUNSHOTDECAL 109 // decal and ricochet sound
// coord, coord, coord (position)
// short (entity index???)
// byte (decal???)

#define TE_SPRITE_SPRAY 110 // spay of alpha sprites
// coord, coord, coord (position)
// coord, coord, coord (velocity)
// short (sprite index)
// byte (count)
// byte (speed)
// byte (noise)

#define TE_ARMOR_RICOCHET 111 // quick spark sprite, client ricochet sound.
// coord, coord, coord (position)
// byte (scale in 0.1's)

#define TE_PLAYERDECAL 112 // ???
// byte (playerindex)
// coord, coord, coord (position)
// short (entity???)
// byte (decal number???)
// [optional] short (model index???)

#define TE_BUBBLES 113 // create alpha sprites inside of box, float upwards
// coord, coord, coord (min start position)
// coord, coord, coord (max start position)
// coord (float height)
// short (model index)
// byte (count)
// coord (speed)

#define TE_BUBBLETRAIL 114 // create alpha sprites along a line, float upwards
// coord, coord, coord (min start position)
// coord, coord, coord (max start position)
// coord (float height)
// short (model index)
// byte (count)
// coord (speed)

#define TE_BLOODSPRITE 115 // spray of opaque sprite1's that fall, single sprite2 for 1..2 secs (this is a high-priority tent)
// coord, coord, coord (position)
// short (sprite1 index)
// short (sprite2 index)
// byte (color)
// byte (scale)

#define TE_WORLDDECAL 116 // Decal applied to the world brush
// coord, coord, coord (x,y,z), decal position (center of texture in world)
// byte (texture index of precached decal texture name)

#define TE_WORLDDECALHIGH 117 // Decal (with texture index > 256) applied to world brush
// coord, coord, coord (x,y,z), decal position (center of texture in world)
// byte (texture index of precached decal texture name - 256)

#define TE_DECALHIGH 118 // Same as TE_DECAL, but the texture index was greater than 256
// coord, coord, coord (x,y,z), decal position (center of texture in world)
// byte (texture index of precached decal texture name - 256)
// short (entity index)

#define TE_PROJECTILE 119 // Makes a projectile (like a nail) (this is a high-priority tent)
// coord, coord, coord (position)
// coord, coord, coord (velocity)
// short (modelindex)
// byte (life)
// byte (owner)  projectile won't collide with owner (if owner == 0, projectile will hit any client).

#define TE_SPRAY 120 // Throws a shower of sprites or models
// coord, coord, coord (position)
// coord, coord, coord (direction)
// short (modelindex)
// byte (count)
// byte (speed)
// byte (noise)
// byte (rendermode)

#define TE_PLAYERSPRITES 121 // sprites emit from a player's bounding box (ONLY use for players!)
// byte (playernum)
// short (sprite modelindex)
// byte (count)
// byte (variance) (0 = no variance in size) (10 = 10% variance in size)

#define TE_PARTICLEBURST 122 // very similar to lavasplash.
// coord (origin)
// short (radius)
// byte (particle color)
// byte (duration * 10) (will be randomized a bit)

#define TE_FIREFIELD 123 // makes a field of fire.
// coord (origin)
// short (radius) (fire is made in a square around origin. -radius, -radius to radius, radius)
// short (modelindex)
// byte (count)
// byte (flags)
// byte (duration (in seconds) * 10) (will be randomized a bit)
//
// to keep network traffic low, this message has associated flags that fit into a byte:
#define TEFIRE_FLAG_ALLFLOAT 1 // all sprites will drift upwards as they animate
#define TEFIRE_FLAG_SOMEFLOAT 2 // some of the sprites will drift upwards. (50% chance)
#define TEFIRE_FLAG_LOOP 4 // if set, sprite plays at 15 fps, otherwise plays at whatever rate stretches the animation over the sprite's duration.
#define TEFIRE_FLAG_ALPHA 8 // if set, sprite is rendered alpha blended at 50% else, opaque
#define TEFIRE_FLAG_PLANAR 16 // if set, all fire sprites have same initial Z instead of randomly filling a cube.

#define TE_PLAYERATTACHMENT 124 // attaches a TENT to a player (this is a high-priority tent)
// byte (entity index of player)
// coord (vertical offset) ( attachment origin.z = player origin.z + vertical offset )
// short (model index)
// short (life * 10 );

#define TE_KILLPLAYERATTACHMENTS 125 // will expire all TENTS attached to a player.
// byte (entity index of player)

#define TE_MULTIGUNSHOT 126 // much more compact shotgun message
// This message is used to make a client approximate a 'spray' of gunfire.
// Any weapon that fires more than one bullet per frame and fires in a bit of a spread is
// a good candidate for MULTIGUNSHOT use. (shotguns)
//
// NOTE: This effect makes the client do traces for each bullet, these client traces ignore
//         entities that have studio models.Traces are 4096 long.
//
// coord (origin)
// coord (origin)
// coord (origin)
// coord (direction)
// coord (direction)
// coord (direction)
// coord (x noise * 100)
// coord (y noise * 100)
// byte (count)
// byte (bullethole decal texture index)

#define TE_USERTRACER 127 // larger message than the standard tracer, but allows some customization.
// coord (origin)
// coord (origin)
// coord (origin)
// coord (velocity)
// coord (velocity)
// coord (velocity)
// byte ( life * 10 )
// byte ( color ) this is an index into an array of color vectors in the engine. (0 - )
// byte ( length * 10 )

#define MSG_BROADCAST 0 // unreliable to all
#define MSG_ONE 1 // reliable to one (msg_entity)
#define MSG_ALL 2 // reliable to all
#define MSG_INIT 3 // write to the init string
#define MSG_PVS 4 // Ents in PVS of org
#define MSG_PAS 5 // Ents in PAS of org
#define MSG_PVS_R 6 // Reliable to PVS
#define MSG_PAS_R 7 // Reliable to PAS
#define MSG_ONE_UNRELIABLE 8 // Send to one client, but don't put in reliable stream, put in unreliable datagram ( could be dropped )
#define MSG_SPEC 9 // Sends to all spectator proxies

// contents of a spot in the world
#define CONTENTS_EMPTY -1
#define CONTENTS_SOLID -2
#define CONTENTS_WATER -3
#define CONTENTS_SLIME -4
#define CONTENTS_LAVA -5
#define CONTENTS_SKY -6

#define CONTENTS_LADDER -16

#define CONTENT_FLYFIELD -17
#define CONTENT_GRAVITY_FLYFIELD -18
#define CONTENT_FOG -19

#define CONTENT_EMPTY -1
#define CONTENT_SOLID -2
#define CONTENT_WATER -3
#define CONTENT_SLIME -4
#define CONTENT_LAVA -5
#define CONTENT_SKY -6

// channels
#define CHAN_AUTO 0
#define CHAN_WEAPON 1
#define CHAN_VOICE 2
#define CHAN_ITEM 3
#define CHAN_BODY 4
#define CHAN_STREAM 5 // allocate stream channel from the static or dynamic area
#define CHAN_STATIC 6 // allocate channel from the static area
#define CHAN_NETWORKVOICE_BASE 7 // voice data coming across the network
#define CHAN_NETWORKVOICE_END 500 // network voice data reserves slots (CHAN_NETWORKVOICE_BASE through CHAN_NETWORKVOICE_END).

// attenuation values
#define ATTN_NONE 0.0f
#define ATTN_NORM 0.8f
#define ATTN_IDLE 2f
#define ATTN_STATIC 1.25f

// pitch values
#define PITCH_NORM 100 // non-pitch shifted
#define PITCH_LOW 95 // other values are possible - 0-255, where 255 is very high
#define PITCH_HIGH 120

// volume values
#define VOL_NORM 1.0

// plats
#define PLAT_LOW_TRIGGER 1

// Trains
#define SF_TRAIN_WAIT_RETRIGGER 1
#define SF_TRAIN_START_ON 4 // Train is initially moving
#define SF_TRAIN_PASSABLE 8 // Train is not solid -- used to make water trains

// buttons
#define IN_ATTACK (1 << 0)
#define IN_JUMP (1 << 1)
#define IN_DUCK (1 << 2)
#define IN_FORWARD (1 << 3)
#define IN_BACK (1 << 4)
#define IN_USE (1 << 5)
#define IN_CANCEL (1 << 6)
#define IN_LEFT (1 << 7)
#define IN_RIGHT (1 << 8)
#define IN_MOVELEFT (1 << 9)
#define IN_MOVERIGHT (1 << 10)
#define IN_ATTACK2 (1 << 11)
#define IN_RUN (1 << 12)
#define IN_RELOAD (1 << 13)
#define IN_ALT1 (1 << 14)
#define IN_SCORE (1 << 15)

// Break Model Defines

#define BREAK_TYPEMASK 0x4F
#define BREAK_GLASS 0x01
#define BREAK_METAL 0x02
#define BREAK_FLESH 0x04
#define BREAK_WOOD 0x08

#define BREAK_SMOKE 0x10
#define BREAK_TRANS 0x20
#define BREAK_CONCRETE 0x40
#define BREAK_2 0x80

// Colliding temp entity sounds

#define BOUNCE_GLASS BREAK_GLASS
#define BOUNCE_METAL BREAK_METAL
#define BOUNCE_FLESH BREAK_FLESH
#define BOUNCE_WOOD BREAK_WOOD
#define BOUNCE_SHRAP 0x10
#define BOUNCE_SHELL 0x20
#define BOUNCE_CONCRETE BREAK_CONCRETE
#define BOUNCE_SHOTSHELL 0x80

// Temp entity bounce sound types
#define TE_BOUNCE_NULL 0
#define TE_BOUNCE_SHELL 1
#define TE_BOUNCE_SHOTSHELL 2

#define MAX_ENT_LEAFS 48

#define MAX_WEAPON_SLOTS 5 // hud item selection slots
#define MAX_ITEM_TYPES 6 // hud item selection slots

#define MAX_ITEMS 5 // hard coded item types

#define HIDEHUD_WEAPONS (1 << 0)
#define HIDEHUD_FLASHLIGHT (1 << 1)
#define HIDEHUD_ALL (1 << 2)
#define HIDEHUD_HEALTH (1 << 3)

#define MAX_AMMO_TYPES 32 // ???
#define MAX_AMMO_SLOTS 32 // not really slots

#define HUD_PRINTNOTIFY 1
#define HUD_PRINTCONSOLE 2
#define HUD_PRINTTALK 3
#define HUD_PRINTCENTER 4

#define WEAPON_SUIT 31

#define VERTEXSIZE 7
#define MAXLIGHTMAPS 4
#define NUM_AMBIENTS 4
#define MAX_MAP_HULLS 4
#define MAX_PHYSINFO_STRING 256
#define MAX_PHYSENTS 600
#define MAX_MOVEENTS 64
#define MAX_LIGHTSTYLES 64
#define MAX_LIGHTSTYLEVALUE 256
#define SURF_DRAWTILED 0x20


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

#define VEC_HULL_MIN Vector(-16, -16, -36)
#define VEC_HULL_MAX Vector(16, 16, 36)
#define VEC_HUMAN_HULL_MIN Vector(-16, -16, 0)
#define VEC_HUMAN_HULL_MAX Vector(16, 16, 72)
#define VEC_HUMAN_HULL_DUCK Vector(16, 16, 36)

#define VEC_VIEW Vector(0, 0, 28)

#define VEC_DUCK_HULL_MIN Vector(-16, -16, -18)
#define VEC_DUCK_HULL_MAX Vector(16, 16, 18)
#define VEC_DUCK_VIEW Vector(0, 0, 12)

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

// Rendering constants
enum {
   kRenderNormal, // src
   kRenderTransColor, // c*a+dest*(1-a)
   kRenderTransTexture, // src*a+dest*(1-a)
   kRenderGlow, // src*a+dest -- No Z buffer checks
   kRenderTransAlpha, // src*srca+dest*(1-srca)
   kRenderTransAdd // src*a+dest
};

enum {
   kRenderFxNone = 0,
   kRenderFxPulseSlow,
   kRenderFxPulseFast,
   kRenderFxPulseSlowWide,
   kRenderFxPulseFastWide,
   kRenderFxFadeSlow,
   kRenderFxFadeFast,
   kRenderFxSolidSlow,
   kRenderFxSolidFast,
   kRenderFxStrobeSlow,
   kRenderFxStrobeFast,
   kRenderFxStrobeFaster,
   kRenderFxFlickerSlow,
   kRenderFxFlickerFast,
   kRenderFxNoDissipation,
   kRenderFxDistort, // Distort/scale/translate flicker
   kRenderFxHologram, // kRenderFxDistort + distance fade
   kRenderFxDeadPlayer, // kRenderAmt is the player index
   kRenderFxExplode, // Scale up really big!
   kRenderFxGlowShell, // Glowing Shell
   kRenderFxClampMinScale // Keep this sprite from getting very small (SPRITES only!)
};

typedef enum {
   ignore_monsters = 1,
   dont_ignore_monsters = 0,
   missile = 2
} IGNORE_MONSTERS;

typedef enum { 
   ignore_glass = 1,
   dont_ignore_glass = 0
} IGNORE_GLASS;

typedef enum {
   point_hull = 0,
   human_hull = 1,
   large_hull = 2,
   head_hull = 3
} HULL;

typedef enum {
   at_notice,
   at_console, // same as at_notice, but forces a ConPrintf, not a message box
   at_aiconsole, // same as at_console, but only shown if developer level is 2!
   at_warning,
   at_error,
   at_logged // Server print to console ( only in multiplayer games ).
} ALERT_TYPE;

// 4-22-98  JOHN: added for use in pfnClientPrintf
typedef enum {
   print_console,
   print_center,
   print_chat
} PRINT_TYPE;

// For integrity checking of content on clients
typedef enum {
   force_exactfile, // File on client must exactly match server's file
   force_model_samebounds, // For model files only, the geometry must fit in the same bbox
   force_model_specifybounds // For model files only, the geometry must fit in the specified bbox
} FORCE_TYPE;


#endif
