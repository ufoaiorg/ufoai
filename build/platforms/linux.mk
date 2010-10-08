SO_EXT                    = so
SO_LDFLAGS                = -shared
SO_CFLAGS                 = -fpic
SO_LIBS                  := -ldl

CFLAGS                   += -DSHARED_EXT=\"$(SO_EXT)\"
CFLAGS                   += -D_GNU_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE

LDFLAGS                  +=
