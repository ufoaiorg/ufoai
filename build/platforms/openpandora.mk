SO_EXT                    = so
SO_LDFLAGS                = -shared -Wl,-z,defs
SO_CFLAGS                 = -fpic
SO_LIBS                  := -ldl
PANDORA                  := 1

OPENGL_LIBS              ?= -lGLES_CM -lEGL

CFLAGS                   += -DSHARED_EXT=\"$(SO_EXT)\"
CFLAGS                   += -D_GNU_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE
CFLAGS                   += -mcpu=cortex-a8 -mfpu=neon -mfloat-abi=softfp -fsigned-char -ffast-math -ftree-vectorize -Ofast
CFLAGS                   += -DPANDORA
CFLAGS                   += -DARM
CFLAGS                   += -DNEON
CFLAGS                   += -DHAVE_GLES

LDFLAGS                  +=

MUMBLE_LIBS              ?= -lrt

game_CFLAGS              += -DLUA_USE_LINUX
testall_CFLAGS           += -DLUA_USE_LINUX
