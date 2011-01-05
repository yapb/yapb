#
# Copyright (c) 2014, by Yet Another POD-Bot Development Team.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# $Id$
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
