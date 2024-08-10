SO_EXT                    = dll
SO_LDFLAGS                = -shared
SO_CFLAGS                 = -fpic
SO_LIBS                  :=
EXE_EXT                  := .exe

CFLAGS                   += -DWINVER=0x501
LDFLAGS                  +=

INTL_LIBS                ?= -lintl
OPENGL_LIBS              ?= -lopengl32

ufo_LDFLAGS              += -lws2_32 -lwinmm
ufo2map_LDFLAGS          += -lwinmm
ufoded_LDFLAGS           += -lws2_32 -lwinmm
ufomodel_LDFLAGS         += -lwinmm
ufoslicer_LDFLAGS        += -lwinmm
