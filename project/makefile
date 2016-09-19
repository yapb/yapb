#
# Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
# Copyright (c) YaPB Development Team.
#
# This software is licensed under the BSD-style license.
# Additional exceptions apply. For full license details, see LICENSE.txt or visit:
#     https://yapb.jeefo.net/license
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

C_OPT_FLAGS = -Ofast -DNDEBUG -pipe -fno-strict-aliasing -mtune=generic
C_DEBUG_FLAGS = -D_DEBUG -DDEBUG -g -ggdb3

C_GCC_FLAGS = -fvisibility=hidden
CPP_GCC_FLAGS = -fvisibility-inlines-hidden

CPP = clang-3.8
CPP_MAC = o32-clang

LINK =
INCLUDE = -I../include -I../include/engine

ifeq "$(MAC)" "true"
	OS = Darwin
	CPP_MAC = o32-clang
else
	OS := $(shell uname -s)
endif

ifeq "$(OS)" "Darwin"
	CPP = $(CPP_MAC)
	LIB_EXT = dylib
	CFLAGS += -DOSX -D_OSX -DPOSIX
	LINK += -dynamiclib -lstdc++ -mmacosx-version-min=10.5 -arch i386
else
	LIB_EXT = so
	CFLAGS += -DLINUX -D_LINUX -DPOSIX
	LINK += -shared -lsupc++
endif

LINK += -m32 -lm -ldl

CFLAGS += -msse2 -std=c++11 -m32 -Wall -Werror -Wextra
CPPFLAGS += -fno-exceptions -fno-rtti

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
	CFLAGS += $(C_GCC_FLAGS) -D__extern_always_inline="extern __always_inline"
	CPPFLAGS += $(CPP_GCC_FLAGS)
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

release_macos:
	mkdir -p release
	$(MAKE) main MAC=true DEBUG=false

debug_macos:
	mkdir -p debug
	$(MAKE) main MAC=true DEBUG=true

all_linux: release debug
all_macos: release_macos debug_macos
all: all_linux all_macos

default: all

clean:
	rm -rf release
	rm -rf debug
