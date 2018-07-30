#
# Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
# Copyright (c) YaPB Development Team.
#
# This software is licensed under the BSD-style license.
# Additional exceptions apply. For full license details, see LICENSE.txt or visit:
#     https://yapb.jeefo.net/license
#

PROJECT = yapb
SOURCES = ../source
OBJECTS = $(wildcard $(SOURCES)/*.cpp)

COMPILER_FLAGS = -std=c++11 -m32 -Wall -Werror -Wextra -fno-exceptions -fno-rtti
LINKER_FLAGS = -m32 -lm -ldl

ifeq "$(SSE_VERSION)" ""
	COMPILER_SSE_VERSION = 2
else
	COMPILER_SSE_VERSION = $(SSE_VERSION)
endif

ifeq "$(DEBUG)" "true"
	COMPILER_FLAGS += -D_DEBUG -DDEBUG -g3
	BINARY_DIR = debug
else
	COMPILER_FLAGS += -DNDEBUG -pipe -O2 -msse$(COMPILER_SSE_VERSION) -funroll-loops -fomit-frame-pointer -fno-stack-protector -fvisibility=hidden -fvisibility-inlines-hidden
	BINARY_DIR = release
endif

INCLUDE = -I../include -I../include/engine
COMPILER = $(CC)

ifeq "$(shell uname -s)" "Darwin"
	MACOS = true
endif

ifeq "$(MACOS)" "true"
	LIBRARY_EXT = dylib
	COMPILER_FLAGS += -DOSX -D_OSX -DPOSIX
	LINKER_FLAGS += -dynamiclib -lstdc++ -mmacosx-version-min=10.5 -arch i386
else
	LIBRARY_EXT = so
	COMPILER_FLAGS += -DLINUX -D_LINUX -DPOSIX
	LINKER_FLAGS += -shared -lsupc++
endif

BINARY_OUTPUT = $(PROJECT).$(LIBRARY_EXT)

ifeq ($(findstring clang,$(COMPILER)),clang)
	COMPILER_FLAGS += -D__extern_always_inline="extern __always_inline"
	ifeq "$(MACOS)" "false"
		LINKER_FLAGS += -lgcc_eh
	endif
else ifeq ($(findstring gcc,$(COMPILER)),gcc)
	ifneq "$(MACOS)" "false"
		LINKER_FLAGS += -static-libgcc
		COMPILER_FLAGS += -funroll-all-loops
	endif
else ifeq ($(findstring icpc,$(COMPILER)),icpc)
	COMPILER_FLAGS += -funroll-all-loops -no-prec-div -no-inline-min-size -no-inline-max-size -wd11076 -wd11074
	LINKER_FLAGS += -static-intel -no-intel-extensions
else
	$(error Compiler unrecognized. Specify CC.)
endif

OBJECTS_BIN := $(OBJECTS:%.cpp=$(BINARY_DIR)/%.o)

$(BINARY_DIR)/%.o: %.cpp
	$(COMPILER) $(INCLUDE) $(COMPILER_FLAGS) -o $(subst $(SOURCES)/,,$@) -c $<

compile:
	mkdir -p $(BINARY_DIR)
	$(MAKE) $(PROJECT)

$(PROJECT): $(OBJECTS_BIN)
	$(COMPILER) $(INCLUDE) $(subst $(SOURCES)/,,$(OBJECTS_BIN)) $(LINKER_FLAGS) -o $(BINARY_DIR)/$(BINARY_OUTPUT)

release:
	$(MAKE) compile DEBUG=false

debug:
	$(MAKE) compile DEBUG=true

release_macos:
	$(MAKE) compile MACOS=true DEBUG=false

debug_macos:
	$(MAKE) compile MACOS=true DEBUG=true

all_linux:
	$(MAKE) compile DEBUG=true
	$(MAKE) compile DEBUG=false

all_macos:
	$(MAKE) compile MACOS=true DEBUG=false
	$(MAKE) compile MACOS=true DEBUG=true

all: all_linux all_macos
default: all_linux

clean:
	rm -rf release/*.o
	rm -rf release/$(BINARY_OUTPUT)
	rm -rf debug/*.o
	rm -rf debug/$(BINARY_OUTPUT)
