SO_EXT                    = dll
SO_LDFLAGS                = -shared
SO_CFLAGS                 = -shared
SO_LIBS                  :=

EXE_EXT                   = .exe

# TODO: config.h
CCFLAGS                  += -DSHARED_EXT=\"$(SO_EXT)\"
CCFLAGS                  += -DGETTEXT_STATIC
CXXFLAGS                 += -DSHARED_EXT=\"$(SO_EXT)\"
CXXFLAGS                 += -DGETTEXT_STATIC
ifeq ($(W2K),1)
  CCFLAGS                += -DWINVER=0x500
  CXXFLAGS               += -DWINVER=0x500
else
  CCFLAGS                += -DWINVER=0x501
  CXXFLAGS               += -DWINVER=0x501
endif

PKG_CONFIG               ?= $(CROSS)pkg-config
OPENGL_LIBS              ?= -lopengl32
OPENAL_LIBS              ?= $(call PKG_LIBS,openal,OpenAL32)
SDL_TTF_LIBS             ?= $(call PKG_LIBS,SDL_ttf) $(call PKG_LIBS,freetype2)
SDL_TTF_CFLAGS           ?= $(call PKG_CFLAGS,SDL_ttf) $(call PKG_CFLAGS,freetype2)
INTL_LIBS                ?= -lintl

ufo_LDFLAGS              += -lws2_32 -lwinmm -lgdi32 -lfreetype -mwindows
ufoded_LDFLAGS           += -lws2_32 -lwinmm -lgdi32 -mwindows
testall_LDFLAGS          += -lws2_32 -lwinmm -lgdi32 -lfreetype
ifdef HAVE_BFD_H
    BFD_LIBS             := -lbfd -liberty -limagehlp
endif
ufo2map_LDFLAGS          += -lwinmm
ufomodel_LDFLAGS         += -lwinmm
uforadiant_LDFLAGS       += -lglib-2.0 -lgtk-win32-2.0 -lgobject-2.0 -mwindows -static-libgcc -static-libstdc++
