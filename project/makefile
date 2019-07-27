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

COMPILER_FLAGS = -std=c++11 -m32 -Wall -Wextra -Werror -fno-exceptions -fno-rtti
LINKER_FLAGS = -m32 -ldl

ifeq "$(DEBUG)" "true"
	COMPILER_FLAGS += -g3 -DCR_DEBUG
	BINARY_DIR = debug
else
	COMPILER_FLAGS += -pipe -O3 -march=core2 -msse2 -mfpmath=sse -ffast-math -fno-builtin -fno-threadsafe-statics -funroll-loops -fomit-frame-pointer -fno-stack-protector -fvisibility=hidden -fvisibility-inlines-hidden
	BINARY_DIR = release
endif

INCLUDE = -I../include
COMPILER = $(CC)

ifeq "$(shell uname -s)" "Darwin"
	OSX = true
endif

ifeq "$(OSX)" "true"
	LIBRARY_EXT = dylib
	COMPILER_FLAGS += -mmacosx-version-min=10.9
	LINKER_FLAGS += -dynamiclib -lstdc++ -mmacosx-version-min=10.9 -arch i386
else
	LIBRARY_EXT = so
	LINKER_FLAGS += -shared
endif

BINARY_OUTPUT = $(PROJECT).$(LIBRARY_EXT)

ifeq ($(findstring clang,$(COMPILER)),clang)
	ifeq "$(OSX)" "false"
		ifeq "$(DEBUG)" "true"
			LINKER_FLAGS += -lstdc++
		else
			LINKER_FLAGS += -static-libgcc -nostdlib++ -Wunused-command-line-argument -fuse-ld=lld -Wl,-z,notext --no-undefined
		endif
	endif
else ifeq ($(findstring gcc,$(COMPILER)),gcc)
	ifneq "$(OSX)" "false"
		ifneq "$(DEBUG)" "true"
			LINKER_FLAGS += -static-libgcc -Wl,--no-undefined -flto=thin
			COMPILER_FLAGS += -funroll-all-loops -flto=thin
		endif
	endif
else ifeq ($(findstring icc,$(COMPILER)),icc)
	LINKER_FLAGS += -static-intel -no-intel-extensions

	ifneq "$(DEBUG)" "true"
		COMPILER_FLAGS += -funroll-all-loops -ipo -wd11076 -wd11074
		LINKER_FLAGS += -static-libgcc -cxxlib-nostd -Wl,--no-undefined,-z,notext,--gc-sections -ipo 
	endif
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
