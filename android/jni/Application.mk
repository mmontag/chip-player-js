APP_ABI               := armeabi-v7a arm64-v8a x86 x86_64
APP_MODULES           := ADLMIDI ADLMIDIrt
APP_PLATFORM          := android-14
APP_OPTIM             := release
APP_CPPFLAGS          += -std=c++11
APP_STL               := c++_static
APP_CFLAGS            := -Ofast
NDK_TOOLCHAIN_VERSION := clang
