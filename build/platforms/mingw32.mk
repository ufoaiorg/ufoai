SO_EXT                    = dll
SO_LDFLAGS                = -shared
SO_CFLAGS                 = -shared

EXE_EXT                   = .exe

# TODO: config.h
CFLAGS                   += -DSHARED_EXT=\"$(SO_EXT)\"
CFLAGS                   += -DGETTEXT_STATIC
# Windows XP is the minimum we need
CFLAGS                   += -DWINVER=0x501

CURL_CONFIG              ?= curl-config
CURL_LIBS                ?= $(shell $(CURL_CONFIG) --libs)
CURL_CFLAGS              ?= $(shell $(CURL_CONFIG) --cflags)
SDL_CONFIG               ?= sdl-config
SDL_LIBS                 ?= $(shell $(SDL_CONFIG) --libs)
SDL_CFLAGS               ?= $(shell $(SDL_CONFIG) --cflags)
OPENGL_CFLAGS            ?=
OPENGL_LIBS              ?= -lopengl32
PKG_CONFIG               ?= $(CROSS)pkg-config
SDL_TTF_LIBS             ?= $(call PKG_LIBS,SDL_ttf) $(call PKG_LIBS,freetype2)
SDL_TTF_CFLAGS           ?= $(call PKG_CFLAGS,SDL_ttf) $(call PKG_CFLAGS,freetype2)
SDL_IMAGE_LIBS           ?= $(call PKG_LIBS,SDL_image)
SDL_IMAGE_CFLAGS         ?= $(call PKG_CFLAGS,SDL_image)
SDL_MIXER_LIBS           ?= $(call PKG_LIBS,SDL_mixer)
SDL_MIXER_CFLAGS         ?= $(call PKG_CFLAGS,SDL_mixer)
OPENAL_CFLAGS            ?= $(call PKG_CFLAGS,openal)
OPENAL_LIBS              ?= $(call PKG_LIBS,openal)
THEORA_CFLAGS            ?= $(call PKG_CFLAGS,theora)
THEORA_LIBS              ?= $(call PKG_LIBS,theora)
GLIB_CFLAGS              ?= $(call PKG_CFLAGS,glib-2.0)
GLIB_LIBS                ?= $(call PKG_LIBS,glib-2.0)
GTK_CFLAGS               ?= $(call PKG_CFLAGS,gtk+-2.0) $(call PKG_CFLAGS,libxml-2.0)
GTK_LIBS                 ?= $(call PKG_LIBS,gtk+-2.0) $(call PKG_LIBS,libxml-2.0)
GTK_SOURCEVIEW_CFLAGS    ?= $(call PKG_CFLAGS,gtksourceview-2.0)
GTK_SOURCEVIEW_LIBS      ?= $(call PKG_LIBS,gtksourceview-2.0)
GTK_GLEXT_CFLAGS         ?= $(call PKG_CFLAGS,gtkglext-1.0)
GTK_GLEXT_LIBS           ?= $(call PKG_LIBS,gtkglext-1.0)
VORBIS_CFLAGS            ?= $(call PKG_CFLAGS,vorbis)
VORBIS_LIBS              ?= $(call PKG_LIBS,vorbis)
INTL_LIBS                ?= -lintl
XVID_CFLAGS              ?=
XVID_LIBS                ?= -lxvidcore

ufo_LDFLAGS              += -lws2_32 -lwinmm -lgdi32 -lfreetype
ufoded_LDFLAGS           += -lws2_32 -lwinmm -lgdi32
testall_LDFLAGS          += -lws2_32 -lwinmm -lgdi32 -lfreetype
ufo2map_LDFLAGS          += -lwinmm
ufomodel_LDFLAGS         += -lwinmm
uforadiant_LDFLAGS       += -lglib-2.0 -lgtk-win32-2.0 -lgobject-2.0
