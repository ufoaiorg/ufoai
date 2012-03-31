SO_EXT                    = so
SO_LDFLAGS                = -shared
SO_CFLAGS                 = -fpic
SO_LIBS                  := -ldl

CCFLAGS                  += -DSHARED_EXT=\"$(SO_EXT)\"
CCFLAGS                  += -D_GNU_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE

LDFLAGS                  +=

game_CFLAGS              += -DLUA_USE_LINUX
testall_CFLAGS           += -DLUA_USE_LINUX
