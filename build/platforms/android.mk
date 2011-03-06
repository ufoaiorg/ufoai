SO_EXT                    = so
SO_LDFLAGS                = -shared -Wl,-Bsymbolic,--no-undefined
SO_CFLAGS                 = -fpic
SO_LIBS                  := -ldl

#TODO check ANDROID_SDK and ANDROID_NDK environment variables

CFLAGS                   += -DSHARED_EXT=\"$(SO_EXT)\"
CFLAGS                   += -D_GNU_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE
CFLAGS                   += --sysroot=$(ANDROID_NDK)/platforms/android-4/arch-arm
CFLAGS                   += -march=armv5te -mtune=xscale -msoft-float
CFLAGS                   += -fpic -ffunction-sections -funwind-tables
CFLAGS                   += -finline-limit=300
CFLAGS                   += -Os -mthumb-interwork
CFLAGS                   += -D__ARM_ARCH_5__ -D__ARM_ARCH_5T__
CFLAGS                   += -D__ARM_ARCH_5E__ -D__ARM_ARCH_5TE__
# supress 'mangling of 'va_list' has changed in GCC 4.4'
CFLAGS                   += -Wno-psabi

LDFLAGS                  += --sysroot=$(ANDROID_NDK)/platforms/android-4/arch-arm
LDFLAGS                  += -mthumb-interwork

game_CFLAGS              += -DLUA_USE_LINUX
testall_CFLAGS           += -DLUA_USE_LINUX
