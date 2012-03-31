SO_EXT                    = so
SO_LDFLAGS                = -shared -Wl,-Bsymbolic,--no-undefined
SO_CCFLAGS                = -fpic
SO_LIBS                  := -ldl

#TODO check ANDROID_SDK and ANDROID_NDK environment variables

CCFLAGS                  += -DSHARED_EXT=\"$(SO_EXT)\"
CCFLAGS                  += -D_GNU_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE
CCFLAGS                  += --sysroot=$(ANDROID_NDK)/platforms/android-4/arch-arm
CCFLAGS                  += -march=armv5te -mtune=xscale -msoft-float
CCFLAGS                  += -fpic -ffunction-sections -funwind-tables
CCFLAGS                  += -finline-limit=300
CCFLAGS                  += -Os -mthumb-interwork
CCFLAGS                  += -D__ARM_ARCH_5__ -D__ARM_ARCH_5T__
CCFLAGS                  += -D__ARM_ARCH_5E__ -D__ARM_ARCH_5TE__
# supress 'mangling of 'va_list' has changed in GCC 4.4'
CCFLAGS                  += -Wno-psabi

LDFLAGS                  += --sysroot=$(ANDROID_NDK)/platforms/android-4/arch-arm
LDFLAGS                  += -mthumb-interwork

game_CFLAGS              += -DLUA_USE_LINUX
testall_CFLAGS           += -DLUA_USE_LINUX
