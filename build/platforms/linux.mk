SO_EXT                    = so
SO_LDFLAGS                = -shared
SO_CFLAGS                 = -fpic
SO_LIBS                  := -ldl

CFLAGS                   += -DSHARED_EXT=\"$(SO_EXT)\"
CFLAGS                   += -D_GNU_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE

LDFLAGS                  +=

CURL_CONFIG              ?= curl-config
CURL_LIBS                ?= $(shell $(CURL_CONFIG) --libs)
CURL_CFLAGS              ?= $(shell $(CURL_CONFIG) --cflags)
SDL_CONFIG               ?= sdl-config
SDL_LIBS                 ?= $(shell $(SDL_CONFIG) --libs)
SDL_CFLAGS               ?= $(shell $(SDL_CONFIG) --cflags)
PKG_CONFIG               ?= pkg-config
SDL_TTF_LIBS             ?= $(call PKG_LIBS,SDL_ttf)
SDL_TTF_CFLAGS           ?= $(call PKG_CFLAGS,SDL_ttf)
SDL_IMAGE_LIBS           ?= $(call PKG_LIBS,SDL_image)
SDL_IMAGE_CFLAGS         ?= $(call PKG_CFLAGS,SDL_image)
SDL_MIXER_LIBS           ?= $(call PKG_LIBS,SDL_mixer)
SDL_MIXER_CFLAGS         ?= $(call PKG_CFLAGS,SDL_mixer)
OPENGL_CFLAGS            ?= $(call PKG_CFLAGS,gl,GL)
OPENGL_LIBS              ?= $(call PKG_LIBS,gl,GL)
OPENAL_CFLAGS            ?= $(call PKG_CFLAGS,openal)
OPENAL_LIBS              ?= $(call PKG_LIBS,openal)
ifdef HAVE_THEORA_THEORA_H
THEORA_CFLAGS            ?= $(call PKG_CFLAGS,theora)
THEORA_LIBS              ?= $(call PKG_LIBS,theora)
endif
GLIB_CFLAGS              ?= $(call PKG_CFLAGS,glib-2.0)
GLIB_LIBS                ?= $(call PKG_LIBS,glib-2.0)
GTK_CFLAGS               ?= $(call PKG_CFLAGS,gtk+-2.0)
GTK_LIBS                 ?= $(call PKG_LIBS,gtk+-2.0)
GTK_SOURCEVIEW_CFLAGS    ?= $(call PKG_CFLAGS,gtksourceview-2.0)
GTK_SOURCEVIEW_LIBS      ?= $(call PKG_LIBS,gtksourceview-2.0)
GTK_GLEXT_CFLAGS         ?= $(call PKG_CFLAGS,gtkglext-1.0)
GTK_GLEXT_LIBS           ?= $(call PKG_LIBS,gtkglext-1.0)
XML2_CFLAGS              ?= $(call PKG_CFLAGS,libxml-2.0)
XML2_LIBS                ?= $(call PKG_LIBS,libxml-2.0)
VORBIS_CFLAGS            ?= $(call PKG_CFLAGS,vorbis)
VORBIS_LIBS              ?= $(call PKG_LIBS,vorbis)
INTL_LIBS                ?=
ifdef HAVE_XVID_H
XVID_CFLAGS              ?=
XVID_LIBS                ?= -lxvidcore
endif
