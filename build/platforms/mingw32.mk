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
ifdef HAVE_SDL2_TTF_SDL_TTF_H
SDL_TTF_LIBS             ?= $(call PKG_LIBS,SDL2_ttf) $(call PKG_LIBS,freetype2)
SDL_TTF_CFLAGS           ?= $(call PKG_CFLAGS,SDL2_ttf) $(call PKG_CFLAGS,freetype2)
else
SDL_TTF_LIBS             ?= $(call PKG_LIBS,SDL_ttf) $(call PKG_LIBS,freetype2)
SDL_TTF_CFLAGS           ?= $(call PKG_CFLAGS,SDL_ttf) $(call PKG_CFLAGS,freetype2)
endif
INTL_LIBS                ?= -lintl

ufo_LDFLAGS              += -lws2_32 -lwinmm -lgdi32 -lfreetype -mwindows
ufoded_LDFLAGS           += -lws2_32 -lwinmm -lgdi32 -mwindows
testall_LDFLAGS          += -lws2_32 -lwinmm -lgdi32 -lfreetype -mconsole
ifdef HAVE_BFD_H
    BFD_LIBS             := -lbfd -liberty -limagehlp
endif
ufo2map_LDFLAGS          += -lwinmm -mconsole
ufomodel_LDFLAGS         += -lwinmm -mconsole
uforadiant_LDFLAGS       += -lglib-2.0 -lgtk-win32-2.0 -lgobject-2.0 -mwindows -static-libgcc -static-libstdc++
ufoslicer_LDFLAGS        += -lwinmm -mconsole
memory_LDFLAGS           += -mconsole $(INTL_LIBS)
