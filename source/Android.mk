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
	message.cpp \
	chatlib.cpp \
	combat.cpp \
	control.cpp \
	engine.cpp \
	interface.cpp \
	navigate.cpp \
	support.cpp \
	graph.cpp \

LOCAL_CFLAGS += -O3 -std=c++11 -DLINUX -D_LINUX -DPOSIX -pipe -fno-strict-aliasing -Wall -Werror -Wno-array-bounds
LOCAL_CPPFLAGS += -fno-exceptions -fno-rtti
LOCAL_LDLIBS := -llog

include $(BUILD_SHARED_LIBRARY)
