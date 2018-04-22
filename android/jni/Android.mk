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

include $(CLEAR_VARS)
LOCAL_MODULE     := ADLMIDIrt

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/src \
	$(LOCAL_PATH)/include

LOCAL_ARM_MODE   := arm
LOCAL_CPP_FEATURES := exceptions
LOCAL_CFLAGS := -DADLMIDI_DISABLE_MIDI_SEQUENCER -DADLMIDI_DISABLE_MUS_SUPPORT -DADLMIDI_DISABLE_XMI_SUPPORT -DDISABLE_EMBEDDED_BANKS
LOCAL_LDLIBS     := -llog
LOCAL_SRC_FILES := src/adlmidi_load.cpp \
                   src/adlmidi_midiplay.cpp \
                   src/adlmidi_opl3.cpp \
                   src/adlmidi_private.cpp \
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
