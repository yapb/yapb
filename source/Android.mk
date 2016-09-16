LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(XASH3D_CONFIG)

LOCAL_MODULE := yapb

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a-hard)
LOCAL_MODULE_FILENAME = libyapb_hardfp
endif

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include \
		    $(LOCAL_PATH)/../include/engine

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

LOCAL_SRC_FILES := \
	basecode.cpp \
	manager.cpp \
	chatlib.cpp \
	combat.cpp \
	globals.cpp \
	engine.cpp \
	interface.cpp \
	navigate.cpp \
	support.cpp \
	waypoint.cpp \

LOCAL_CFLAGS += -Ofast -std=c++11 -DLINUX -D_LINUX -DPOSIX -pipe -flto -fno-strict-aliasing -Wall -Werror
LOCAL_CPPFLAGS += -fno-exceptions -fno-rtti
LOCAL_LDFLAGS += -flto

include $(BUILD_SHARED_LIBRARY)
