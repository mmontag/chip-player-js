LOCAL_PATH := $(call my-dir)/../..
include $(CLEAR_VARS)

LOCAL_MODULE     := ADLMIDI

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/src \
	$(LOCAL_PATH)/include

LOCAL_ARM_MODE   := arm
LOCAL_CPP_FEATURES := exceptions
LOCAL_LDLIBS     := -llog
LOCAL_SRC_FILES := src/adldata.cpp \
                   src/adlmidi_load.cpp \
                   src/adlmidi_midiplay.cpp \
                   src/adlmidi_mus2mid.c \
                   src/adlmidi_opl3.cpp \
                   src/adlmidi_private.cpp \
                   src/adlmidi_xmi2mid.c \
                   src/adlmidi.cpp \
                   src/wopl/wopl_file.c \
                   src/chips/dosbox_opl3.cpp \
                   src/chips/nuked_opl3_v174.cpp \
                   src/chips/nuked_opl3.cpp \
                   src/chips/opl_chip_base.cpp \
                   src/chips/dosbox/dbopl.cpp \
                   src/chips/nuked/nukedopl3_174.c \
                   src/chips/nuked/nukedopl3.c

include $(BUILD_SHARED_LIBRARY)
