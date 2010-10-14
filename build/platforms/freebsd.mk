SO_EXT                    = so
SO_LDFLAGS                = -shared
SO_CFLAGS                 = -fpic
SO_LIBS                  :=

CFLAGS                   += -DSHARED_EXT=\"$(SO_EXT)\"

LDFLAGS                  +=

INTL_LIBS                ?= -lintl
