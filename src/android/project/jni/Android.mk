LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(LOCAL_PATH)/misc/Makefile
include $(LOCAL_PATH)/loaders/Makefile
include $(LOCAL_PATH)/loaders/prowizard/Makefile
include $(LOCAL_PATH)/player/Makefile

MISC_SOURCES	:= $(addprefix misc/,$(MISC_OBJS))
LOADERS_SOURCES := $(addprefix loaders/,$(LOADERS_OBJS))
PROWIZ_SOURCES	:= $(addprefix loaders/prowizard/,$(PROWIZ_OBJS))
PLAYER_SOURCES	:= $(addprefix player/,$(PLAYER_OBJS))

LOCAL_MODULE    := xmp
LOCAL_CFLAGS	:= -DHAVE_CONFIG_H -I$(LOCAL_PATH) -I$(LOCAL_PATH)/include
LOCAL_SRC_FILES := drivers/smix.c \
	$(MISC_SOURCES:.o=.c) \
	$(LOADERS_SOURCES:.o=.c) \
	$(PROWIZ_SOURCES:.o=.c) \
	$(PLAYER_SOURCES:.o=.c)

include $(BUILD_SHARED_LIBRARY)
