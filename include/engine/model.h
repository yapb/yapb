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

// stripped down version of com_model.h & pm_info.h

#ifndef MODEL_H
#define MODEL_H

typedef vec_t vec2_t[2];
typedef vec_t vec4_t[4];

typedef int qboolean;
typedef unsigned char byte;

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

typedef struct mplane_s {
   vec3_t normal;
   float dist;
   byte type; // for fast side tests
   byte signbits; // signx + (signy<<1) + (signz<<1)
   byte pad[2];
} mplane_t;

typedef struct {
   vec3_t position;
} mvertex_t;

typedef struct {
   float vecs[2][4]; // [s/t] unit vectors in world space [i][3] is the s/t offset relative to the origin s or t = dot( 3Dpoint, vecs[i] ) + vecs[i][3]
   float mipadjust;  // mipmap limits for very small surfaces
   struct texture_t *texture;
   int flags; // sky or slime, no lightmap or 256 subdivision
} mtexinfo_t;

typedef struct mnode_hw_s {
   int contents; // 0, to differentiate from leafs
   int visframe; // node needs to be traversed if current
   float minmaxs[6]; // for bounding box culling
   struct mnode_s *parent;
   mplane_t *plane;
   struct mnode_s *children[2];
   unsigned short firstsurface;
   unsigned short numsurfaces;
} mnode_hw_t;

typedef struct mnode_s {
   int contents; // 0, to differentiate from leafs
   int visframe; // node needs to be traversed if current
   short minmaxs[6]; // for bounding box culling
   struct mnode_s *parent;
   mplane_t *plane;
   struct mnode_s *children[2];
   unsigned short firstsurface;
   unsigned short numsurfaces;
} mnode_t;

typedef struct {
   byte r, g, b;
} color24;

typedef struct msurface_hw_s {
   int visframe; // should be drawn when node is crossed
   mplane_t *plane; // pointer to shared plane
   int flags; // see SURF_ #defines
   int firstedge; // look up in model->surfedges[], negative numbers
   int numedges;  // are backwards edges
   short texturemins[2];
   short extents[2];
   int light_s, light_t; // gl lightmap coordinates
   struct glpoly_t *polys; // multiple if warped
   struct msurface_s *texturechain;
   mtexinfo_t *texinfo;
   int dlightframe; // last frame the surface was checked by an animated light
   int dlightbits;  // dynamically generated. Indicates if the surface illumination is modified by an animated light.
   int lightmaptexturenum;
   byte styles[MAXLIGHTMAPS];
   int cached_light[MAXLIGHTMAPS]; // values currently used in lightmap
   qboolean   cached_dlight; // true if dynamic light in cache
   color24 *samples; // note: this is the actual lightmap data for this surface
   struct decal_t *pdecals;
} msurface_hw_t;

typedef struct msurface_s {
   int visframe; // should be drawn when node is crossed
   int dlightframe;	// last frame the surface was checked by an animated light
   int dlightbits; // dynamically generated. Indicates if the surface illumination is modified by an animated light.
   mplane_t *plane; // pointer to shared plane			
   int flags; // see SURF_ #defines
   int firstedge;	// look up in model->surfedges[], negative numbers
   int numedges;	// are backwards edges
   struct surfcache_s *cachespots[4]; // surface generation data
   short texturemins[2]; // smallest s/t position on the surface.
   short extents[2];		// ?? s/t texture size, 1..256 for all non-sky surfaces
   mtexinfo_t *texinfo;
   byte styles[MAXLIGHTMAPS]; // index into d_lightstylevalue[] for animated lights  no one surface can be effected by more than 4 animated lights.
   color24 *samples;
   struct decal_t *pdecals;
} msurface_t;

typedef struct cache_user_s {
   void* data; // extradata
} cache_user_t;

typedef struct hull_s {
   struct dclipnode_t *clipnodes;
   mplane_t *planes;
   int firstclipnode;
   int lastclipnode;
   vec3_t clip_mins;
   vec3_t clip_maxs;
} hull_t;

typedef struct model_s {
   char name[64]; // model name
   qboolean needload; // bmodels and sprites don't cache normally
   int type; // model type
   int numframes; // sprite's framecount
   byte *mempool; // private mempool (was synctype)
   int flags; // hl compatibility
   vec3_t mins, maxs; // bounding box at angles '0 0 0'
   float radius;
   int firstmodelsurface;
   int nummodelsurfaces;
   int numsubmodels;
   struct dmodel_t *submodels; // or studio animations
   int numplanes;
   mplane_t *planes;
   int numleafs; // number of visible leafs, not counting 0
   struct mleaf_t *leafs;
   int numvertexes;
   mvertex_t *vertexes;
   int numedges;
   struct medge_t *edges;
   int numnodes;
   mnode_t *nodes;
   int numtexinfo;
   mtexinfo_t *texinfo;
   int numsurfaces;
   msurface_t *surfaces;
   int numsurfedges;
   int *surfedges;
   int numclipnodes;
   struct dclipnode_t *clipnodes;
   int nummarksurfaces;
   msurface_t **marksurfaces;
   hull_t hulls[MAX_MAP_HULLS];
   int numtextures;
   texture_t **textures;
   byte *visdata;
   color24 *lightdata;
   char *entities;
   cache_user_t cache; // only access through Mod_Extradata
} model_t;

struct lightstyle_t {
   int length;
   char map[MAX_LIGHTSTYLES];
};

typedef struct physent_s {
   char name[32]; // name of model, or "player" or "world".
   int player;
   vec3_t origin; // model's origin in world coordinates.
   struct model_s* model; // only for bsp models
} physent_t;

typedef struct playermove_s {
   int player_index; // So we don't try to run the PM_CheckStuck nudging too quickly.
   qboolean server; // For debugging, are we running physics code on server side?
   qboolean multiplayer; // 1 == multiplayer server
   float time; // realtime on host, for reckoning duck timing
   float frametime; // Duration of this frame
   vec3_t forward, right, up; // Vectors for angles
   vec3_t origin; // Movement origin.
   vec3_t angles; // Movement view angles.
   vec3_t oldangles; // Angles before movement view angles were looked at.
   vec3_t velocity; // Current movement direction.
   vec3_t movedir; // For waterjumping, a forced forward velocity so we can fly over lip of ledge.
   vec3_t basevelocity; // Velocity of the conveyor we are standing, e.g.
   vec3_t view_ofs; // Our eye position.
   float flDuckTime; // Time we started duck
   qboolean bInDuck; // In process of ducking or ducked already?
   int flTimeStepSound; // Next time we can play a step sound
   int iStepLeft;
   float flFallVelocity;
   vec3_t punchangle;
   float flSwimTime;
   float flNextPrimaryAttack;
   int effects; // MUZZLE FLASH, e.g.
   int flags; // FL_ONGROUND, FL_DUCKING, etc.
   int usehull; // 0 = regular player hull, 1 = ducked player hull, 2 = point hull
   float gravity; // Our current gravity and friction.
   float friction;
   int oldbuttons; // Buttons last usercmd
   float waterjumptime; // Amount of time left in jumping out of water cycle.
   qboolean dead; // Are we a dead player?
   int deadflag;
   int spectator; // Should we use spectator physics model?
   int movetype;  // Our movement type, NOCLIP, WALK, FLY
   int onground;
   int waterlevel;
   int watertype;
   int oldwaterlevel;
   char sztexturename[256];
   char chtexturetype;
   float maxspeed;
   float clientmaxspeed; // Player specific maxspeed
   int iuser1;
   int iuser2;
   int iuser3;
   int iuser4;
   float fuser1;
   float fuser2;
   float fuser3;
   float fuser4;
   vec3_t vuser1;
   vec3_t vuser2;
   vec3_t vuser3;
   vec3_t vuser4;
   int numphysent;
   physent_t physents[MAX_PHYSENTS];
} playermove_t;

#endif