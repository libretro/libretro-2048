LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

APP_DIR := ../

LOCAL_MODULE    := retro

ifeq ($(TARGET_ARCH),arm)
LOCAL_CFLAGS += -DANDROID_ARM
LOCAL_ARM_MODE := arm
endif

ifeq ($(TARGET_ARCH),x86)
LOCAL_CFLAGS +=  -DANDROID_X86
endif

ifeq ($(TARGET_ARCH),mips)
LOCAL_CFLAGS += -DANDROID_MIPS -D__mips__ -D__MIPSEL__
endif

LOCAL_SRC_FILES    += $(APP_DIR)/libretro.c $(APP_DIR)/game_noncairo.c $(APP_DIR)/game_shared.c
LOCAL_CFLAGS += -O2 -std=gnu99 -DINLINE=inline -D__LIBRETRO__

include $(BUILD_SHARED_LIBRARY)
