LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := adlmidi_shared
LOCAL_SRC_FILES := libs/$(TARGET_ARCH_ABI)/libADLMIDI.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../src $(LOCAL_PATH)/../include
TARGET_PLATFORM = android-14

include $(CLEAR_VARS)
LOCAL_MODULE := adlmidi_RT_shared
LOCAL_SRC_FILES := libs/$(TARGET_ARCH_ABI)/libADLMIDIrt.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../src $(LOCAL_PATH)/../include
TARGET_PLATFORM = android-14

include $(PREBUILT_SHARED_LIBRARY)
