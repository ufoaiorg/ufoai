LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := theora

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(LOCAL_PATH)/lib $(LOCAL_PATH) $(LOCAL_PATH)/../ogg/include $(LOCAL_PATH)/../vorbis/include
LOCAL_CFLAGS := -O3 -DHAVE_CONFIG_H

LOCAL_CPP_EXTENSION := .cpp

LOCAL_SRC_FILES := $(addprefix lib/, $(notdir $(wildcard $(LOCAL_PATH)/lib/*.c) $(wildcard $(LOCAL_PATH)/lib/*.cpp)))

LOCAL_STATIC_LIBRARIES := ogg vorbis

LOCAL_SHARED_LIBRARIES :=

LOCAL_LDLIBS :=

include $(BUILD_SHARED_LIBRARY)

