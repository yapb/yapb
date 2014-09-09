#
# Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
# Copyright (c) YaPB Development Team.
#
# This software is licensed under the BSD-style license.
# Additional exceptions apply. For full license details, see LICENSE.txt or visit:
#     http:#yapb.jeefo.net/license
#

MODNAME = yapb
SYSTEM = ../source

OBJ = ${SYSTEM}/basecode.o      \
      ${SYSTEM}/botmanager.o    \
      ${SYSTEM}/chatlib.o       \
      ${SYSTEM}/combat.o        \
      ${SYSTEM}/globals.o       \
      ${SYSTEM}/interface.o     \
      ${SYSTEM}/navigate.o      \
      ${SYSTEM}/netmsg.o        \
      ${SYSTEM}/support.o       \
      ${SYSTEM}/waypoint.o      \

CCOPT = -w -O3 -m32 -s -DNDEBUG=1 -ffast-math -fno-exceptions -fno-rtti -funroll-loops -fomit-frame-pointer -pipe -fvisibility-inlines-hidden -fvisibility=hidden
CCDEBUG = -ggdb -w -DDEBUG=1 -fpermissive

CFLAGS = $(CCOPT) -I../include/engine -I../include
#CFLAGS = $(CCDEBUG) -I../include/engine -I../include

BASEFLAGS = -Dstricmp=strcasecmp -Dstrcmpi=strcasecmp
CPPFLAGS = ${BASEFLAGS} ${CFLAGS}
OS := $(shell uname -s)

ifeq "$(OS)" "Darwin"
        CPP=clang
        SUFFIX=dylib
        LINK=-m32 -dynamiclib -mmacosx-version-min=10.5
        CPPLIB=-ldl -lm -lstdc++
else
        CPP=gcc
        SUFFIX=so
        LINK=-m32 -shared -static-libgcc
        CPPLIB=-ldl -lm -lsupc++
endif

BINARY=${MODNAME}.${SUFFIX}

${MODNAME}: ${OBJ}
	${CPP} ${LINK} ${OBJ} ${CPPLIB} -o ${BINARY}

clean:
	-rm -f ${SYSTEM}/*.o
	-rm -f ${BINARY}

%.o:    %.cpp
	${CPP} ${CPPFLAGS} -c $< -o $@
