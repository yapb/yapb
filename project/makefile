#
# Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
# Copyright (c) YaPB Development Team.
#
# This software is licensed under the BSD-style license.
# Additional exceptions apply. For full license details, see LICENSE.txt or visit:
#     https://yapb.ru/license
#

PROJECT = yapb
SOURCES = ../source
OBJECTS = $(wildcard $(SOURCES)/*.cpp)

COMPILER_FLAGS = -std=c++11 -m32 -Wall -Wextra -Werror -fno-exceptions -fno-rtti -DPOSIX
LINKER_FLAGS = -m32 -lm -ldl

ifeq "$(DEBUG)" "true"
	COMPILER_FLAGS += -D_DEBUG -DDEBUG -g3
	BINARY_DIR = debug
else
	COMPILER_FLAGS += -DNDEBUG -pipe -O3 -g -ffast-math -msse2 -funroll-loops -fomit-frame-pointer -fno-stack-protector -fvisibility=hidden -fvisibility-inlines-hidden -nostdinc++
	BINARY_DIR = release
endif

INCLUDE = -I../include -I../include/engine
COMPILER = $(CC)

ifeq "$(shell uname -s)" "Darwin"
	OSX = true
endif

ifeq "$(OSX)" "true"
	LIBRARY_EXT = dylib
	COMPILER_FLAGS += -DOSX -D_OSX
	LINKER_FLAGS += -dynamiclib -lstdc++ -mmacosx-version-min=10.13 -arch i386
else
	LIBRARY_EXT = so
	COMPILER_FLAGS += -DLINUX -D_LINUX
	LINKER_FLAGS += -shared
endif

BINARY_OUTPUT = $(PROJECT).$(LIBRARY_EXT)

ifeq ($(findstring clang,$(COMPILER)),clang)
	ifeq "$(OSX)" "false"
		LINKER_FLAGS += -lgcc_eh
	endif
else ifeq ($(findstring gcc,$(COMPILER)),gcc)
	ifneq "$(OSX)" "false"
		LINKER_FLAGS += -static-libgcc
		COMPILER_FLAGS += -funroll-all-loops -Wno-implicit-fallthrough
	endif
else ifeq ($(findstring icc,$(COMPILER)),icc)
	COMPILER_FLAGS += -funroll-all-loops -no-prec-div -no-inline-min-size -no-inline-max-size -wd11076 -wd11074
	LINKER_FLAGS += -cxxlib-nostd -static-intel -no-intel-extensions
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

all:
	$(MAKE) compile DEBUG=true
	$(MAKE) compile DEBUG=false
clean:
	rm -rf release/*.o
	rm -rf release/$(BINARY_OUTPUT)
	rm -rf debug/*.o
	rm -rf debug/$(BINARY_OUTPUT)