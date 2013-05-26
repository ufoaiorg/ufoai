SO_EXT                    = dll
SO_LDFLAGS                = -shared
SO_CFLAGS                 = -fpic
SO_LIBS                  :=
EXE_EXT                  := .exe

CFLAGS                   += -DWINVER=0x501
LDFLAGS                  +=

OPENGL_LIBS              ?= -lopengl64
caveexpress_LDFLAGS      += -lws2_64 -lwinmm -lgdi64 -liphlpapi -static-libgcc -static-libstdc++
