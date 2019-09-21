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

#ifndef EXTDLL_H
#define EXTDLL_H


typedef int func_t; //
typedef float vec_t; // needed before including progdefs.h
typedef vec_t vec2_t[2];
typedef vec_t vec4_t[4];
typedef int qboolean;
typedef unsigned char byte;

#include <crlib/cr-vector.h>

// idea from regamedll
class string_t final {
private:
   int offset;

public:
   explicit string_t () : offset (0) { }
   string_t (int offset) : offset (offset) { }

   ~string_t () = default;

public:
   const char *chars (size_t shift = 0) const;

public:
   operator int () const {
      return offset;
   }

   string_t &operator = (const string_t &rhs) {
      offset = rhs.offset;
      return *this;
   }
};

typedef cr::Vector vec3_t;
using namespace cr::types;

typedef struct edict_s edict_t;

#include "const.h"
#include "progdefs.h"
#include "model.h"

typedef struct link_s {
   struct link_s *prev, *next;
} link_t;

typedef struct {
   vec3_t normal;
   float dist;
} plane_t;

struct edict_s {
   int free;
   int serialnumber;
   link_t area; // linked to a division node or leaf
   int headnode; // -1 to use normal leaf check
   int num_leafs;
   short leafnums[MAX_ENT_LEAFS];
   float freetime; // sv.time when the object was freed
   void *pvPrivateData; // Alloced and freed by engine, used by DLLs
   entvars_t v; // C exported fields from progs
};

#include "eiface.h"

extern globalvars_t *globals;
extern enginefuncs_t engfuncs;
extern gamefuncs_t dllapi;

// Use this instead of ALLOC_STRING on constant strings
#define STRING(offset) (const char *)(globals->pStringBase + (int)offset)

// form fwgs-hlsdk
#if defined (CR_ARCH_X64)
static inline int MAKE_STRING (const char *val) {
   long long ptrdiff = val - STRING (0);

   if (ptrdiff > INT_MAX || ptrdiff < INT_MIN) {
      return engfuncs.pfnAllocString (val);
   }
   return static_cast <int> (ptrdiff);
}
#else 
#define MAKE_STRING(str)	((uint64)(str) - (uint64)(STRING(0)))
#endif

inline const char *string_t::chars (size_t shift) const {
   return STRING (offset) + shift;
}

#endif // EXTDLL_H
