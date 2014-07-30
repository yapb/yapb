/*
 * Copyright (c) 2001-2006 Will Day <willday@hpgx.net>
 * See the file "dllapi.h" in this folder for full information
 */

// Simplified version by Wei Mingzhi

#ifndef SDK_UTIL_H
#define SDK_UTIL_H

#ifdef DEBUG
#undef DEBUG
#endif

#include <util.h>




short FixedSigned16 (float value, float scale);
unsigned short FixedUnsigned16 (float value, float scale);

#endif
