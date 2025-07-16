LOCAL_PATH := $(call my-dir)




include $(CLEAR_VARS)
LOCAL_MODULE := mono_dump


LOCAL_CFLAGS := -std=c17
LOCAL_CFLAGS += -fvisibility=hidden
LOCAL_CPPFLAGS := -std=c++17
LOCAL_CPPFLAGS += -fvisibility=hidden
LOCAL_CPPFLAGS += -fexceptions

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include

LOCAL_SRC_FILES := src/dump.cpp

LOCAL_LDLIBS := -llog -landroid

include $(BUILD_EXECUTABLE) #可执行文件