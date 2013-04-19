SO_EXT                    = so
SO_LDFLAGS                = -shared
SO_CFLAGS                 = -fpic
SO_LIBS                  := -ldl

CFLAGS                   += -D_GNU_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE --jcache -Wno-c++11-extensions

EMSCRIPTEN               := 1
LDFLAGS                  +=
EXE_EXT                  := .html
