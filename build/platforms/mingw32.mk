SO_EXT                    = dll
SO_LDFLAGS                = -shared
SO_CFLAGS                 = -shared
SO_LIBS                  :=

EXE_EXT                   = .exe

# TODO: config.h
CFLAGS                   += -DSHARED_EXT=\"$(SO_EXT)\"
CFLAGS                   += -DGETTEXT_STATIC
ifeq ($(W2K),1)
  CFLAGS                 += -DWINVER=0x500
else
  CFLAGS                 += -DWINVER=0x501
endif

PKG_CONFIG               ?= $(CROSS)pkg-config
OPENGL_LIBS              ?= -lopengl32
OPENAL_LIBS              ?= $(call PKG_LIBS,openal,OpenAL32)
SDL_TTF_LIBS             ?= $(call PKG_LIBS,SDL_ttf) $(call PKG_LIBS,freetype2)
SDL_TTF_CFLAGS           ?= $(call PKG_CFLAGS,SDL_ttf) $(call PKG_CFLAGS,freetype2)
INTL_LIBS                ?= -lintl

ufo_LDFLAGS              += -lws2_32 -lwinmm -lgdi32 -lfreetype
ufoded_LDFLAGS           += -lws2_32 -lwinmm -lgdi32
testall_LDFLAGS          += -lws2_32 -lwinmm -lgdi32 -lfreetype
ifdef HAVE_BFD_H
    BFD_LIBS             := -lbfd -liberty -limagehlp
    ufo_LDFLAGS          += $(BFD_LIBS)
    ufoded_LDFLAGS       += $(BFD_LIBS)
    testall_LDFLAGS      += $(BFD_LIBS)
endif
ufo2map_LDFLAGS          += -lwinmm
ufomodel_LDFLAGS         += -lwinmm
uforadiant_LDFLAGS       += -lglib-2.0 -lgtk-win32-2.0 -lgobject-2.0
