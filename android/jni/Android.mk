LOCAL_PATH := $(call my-dir)/../..
include $(CLEAR_VARS)

LOCAL_MODULE     := ADLMIDI

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/src \
	$(LOCAL_PATH)/include

LOCAL_ARM_MODE   := arm
LOCAL_CPP_FEATURES := exceptions
LOCAL_LDLIBS     := -llog
LOCAL_SRC_FILES := src/adldata.cpp src/adlmidi.cpp src/dbopl.cpp src/nukedopl3.c \
                   src/adlmidi_load.cpp src/adlmidi_midiplay.cpp \
                   src/adlmidi_opl3.cpp src/adlmidi_private.cpp \
                   src/adlmidi_xmi2mid.c src/adlmidi_mus2mid.c

include $(BUILD_SHARED_LIBRARY)
