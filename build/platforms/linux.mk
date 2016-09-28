SO_EXT                    = so
SO_LDFLAGS                = -shared -Wl,-z,defs
SO_CFLAGS                 = -fpic
SO_LIBS                  := -ldl

CFLAGS                   += -DSHARED_EXT=\"$(SO_EXT)\"
CFLAGS                   += -D_GNU_SOURCE -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_XOPEN_SOURCE

LDFLAGS                  +=

MUMBLE_LIBS              ?= -lrt

game_CFLAGS              += -DLUA_USE_LINUX
testall_CFLAGS           += -DLUA_USE_LINUX
