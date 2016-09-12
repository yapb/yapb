#
# Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
# Copyright (c) YaPB Development Team.
#
# This software is licensed under the BSD-style license.
# Additional exceptions apply. For full license details, see LICENSE.txt or visit:
#     http://yapb.jeefo.net/license
#
# Based on Makefile written by David "BAILOPAN" Anderson.
#

PROJECT = yapb
SRC_DIR = ../source
OBJECTS = $(SRC_DIR)/basecode.cpp	\
	  $(SRC_DIR)/manager.cpp	\
	  $(SRC_DIR)/chatlib.cpp	\
	  $(SRC_DIR)/combat.cpp		\
	  $(SRC_DIR)/globals.cpp	\
	  $(SRC_DIR)/engine.cpp		\
	  $(SRC_DIR)/interface.cpp	\
	  $(SRC_DIR)/navigate.cpp	\
	  $(SRC_DIR)/support.cpp	\
	  $(SRC_DIR)/waypoint.cpp	\

C_OPT_FLAGS =  -O3 -DNDEBUG=1 -fno-exceptions -fno-rtti -funroll-loops -fomit-frame-pointer -pipe -mtune=native
C_DEBUG_FLAGS = -D_DEBUG -DDEBUG -g -ggdb3
C_GCC4_FLAGS = -fvisibility=hidden
CPP_GCC4_FLAGS = -fvisibility-inlines-hidden
CPP = clang-3.7
CPP_OSX = clang

LINK =
INCLUDE = -I../include -I../include/engine

ifeq "$(OSX)" "true"
	OS = Darwin
	CPP_OSX = o32-clang
else
	OS := $(shell uname -s)
endif

ifeq "$(OS)" "Darwin"
	CPP = $(CPP_OSX)
	LIB_EXT = dylib
	CFLAGS += -DOSX -D_OSX -DPOSIX
	LINK += -dynamiclib -lstdc++ -mmacosx-version-min=10.5
else
	LIB_EXT = so
	CFLAGS += -DLINUX -D_LINUX -DPOSIX
	LINK += -shared -lsupc++
endif

LINK += -m32 -lm -ldl

CFLAGS += -std=c++11 -DHAVE_STDINT_H -D__extern_always_inline=inline -D_strdup=strdup -Dstricmp=strcasecmp -Dstrcmpi=strcasecmp -fno-strict-aliasing -m32 -Wall -Werror -Wextra
CPPFLAGS += -fno-exceptions -fno-rtti -fno-builtin

BINARY = $(PROJECT).$(LIB_EXT)

ifeq "$(DEBUG)" "true"
	BIN_DIR = debug
	CFLAGS += $(C_DEBUG_FLAGS)
else
	BIN_DIR = release
	CFLAGS += $(C_OPT_FLAGS)

	ifneq "$(OS)" "Darwin"
		LINK += -s
	endif
endif

IS_CLANG := $(shell $(CPP) --version | head -1 | grep clang > /dev/null && echo "1" || echo "0")

ifeq "$(IS_CLANG)" "1"
	CFLAGS += $(C_GCC4_FLAGS)
	CPPFLAGS += $(CPP_GCC4_FLAGS)
endif

# OS is Linux and not using clang
ifeq "$(shell expr $(OS) \= Linux \& $(IS_CLANG) \= 0)" "1"
	LINK += -static-libgcc
endif

# OS is Linux and using clang
ifeq "$(shell expr $(OS) \= Linux \& $(IS_CLANG) \= 1)" "1"
	LINK += -lgcc_eh
endif

OBJ_BIN := $(OBJECTS:%.cpp=$(BIN_DIR)/%.o)

$(BIN_DIR)/%.o: %.cpp
	$(CPP) $(INCLUDE) $(CFLAGS) $(CPPFLAGS) -o $(subst $(SRC_DIR)/,,$@) -c $<

main:
	$(MAKE) $(PROJECT)

$(PROJECT): $(OBJ_BIN)
	$(CPP) $(INCLUDE) $(subst $(SRC_DIR)/,,$(OBJ_BIN)) $(LINK) -o $(BIN_DIR)/$(BINARY)

debug:
	mkdir -p debug
	$(MAKE) main DEBUG=true

release:
	mkdir -p release
	$(MAKE) main DEBUG=false

release_osx:
	mkdir -p release
	$(MAKE) main OSX=true DEBUG=false

debug_osx:
	mkdir -p debug
	$(MAKE) main OSX=true DEBUG=true

all_linux: release debug
all_osx: release_osx debug_osx
all: all_linux all_osx

default: all

clean:
	rm -rf release
	rm -rf debug

