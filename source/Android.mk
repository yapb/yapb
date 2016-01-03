LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(XASH3D_CONFIG)

LOCAL_MODULE := yapb

LOCAL_CONLYFLAGS += -std=c99

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include \
		    $(LOCAL_PATH)/../include/engine

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

LOCAL_SRC_FILES := \
	basecode.cpp \
	manager.cpp \
	chatlib.cpp \
	combat.cpp \
	globals.cpp \
	interface.cpp \
	navigate.cpp \
	netmsg.cpp \
	support.cpp \
	waypoint.cpp \

LOCAL_CFLAGS += -O2 -DLINUX -D_LINUX -DPOSIX -DHAVE_STDINT_H -D__extern_always_inline=inline -D_strdup=strdup -Dstricmp=strcasecmp -Dstrcmpi=strcasecmp -fno-strict-aliasing -Wall -Werror -Wno-uninitialized -Wno-unused -Wno-switch -Wno-c++11-compat-deprecated-writable-strings

LOCAL_CPPFLAGS += -w -Wno-invalid-offsetof -fno-exceptions -fno-rtti

include $(BUILD_SHARED_LIBRARY)
