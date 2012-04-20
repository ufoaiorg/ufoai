SO_EXT                    = dylib
SO_CFLAGS                 = -fPIC -fno-common
SO_LDFLAGS                = -dynamiclib
SO_LIBS                  := -ldl

CFLAGS                   += -D_BSD_SOURCE -D_XOPEN_SOURCE
LDFLAGS                  += -framework IOKit -framework Foundation -framework Cocoa
LDFLAGS                  += -rdynamic

### most mac users will have their additional libs and headers under /opt/local,
### check for that, and if present, add to CFLAGS/LDFLAGS (really convenient!)
CFLAGS                   += -I/opt/local/include -F/opt/local/Library/Frameworks
LDFLAGS                  += -L/opt/local/lib -F/opt/local/Library/Frameworks

ifdef UNIVERSAL
	CFLAGS += -arch i386 -arch ppc
	LDFLAGS += -arch i386 -arch ppc
endif

FRAMEWORK_DIR            ?= /opt/local/Library/Frameworks
CURL_CONFIG              ?= curl-config
CURL_LIBS                ?= $(shell $(CURL_CONFIG) --libs)
CURL_CFLAGS              ?= $(shell $(CURL_CONFIG) --cflags)
SDL_CFLAGS               ?= -I$(FRAMEWORK_DIR)/SDL.framework/Headers
SDL_LIBS                 ?= -framework SDL
SDL_MIXER_CFLAGS         ?= -I$(FRAMEWORK_DIR)/SDL_mixer.framework/Headers
SDL_MIXER_LIBS           ?= -framework SDL_mixer
SDL_TTF_CFLAGS           ?= -I$(FRAMEWORK_DIR)/SDL_ttf.framework/Headers
SDL_TTF_LIBS             ?= -framework SDL_ttf
OPENGL_CFLAGS            ?=
OPENGL_LIBS              ?= -framework OpenGL
OPENAL_CFLAGS            ?=
OPENAL_LIBS              ?= -framework OpenAL
PKG_CONFIG               ?= pkg-config
ifdef HAVE_THEORA_THEORA_H
THEORA_CFLAGS            ?= $(call PKG_CFLAGS,theora) $(call PKG_CFLAGS,vorbis)
THEORA_LIBS              ?= $(call PKG_LIBS,theora) $(call PKG_LIBS,vorbis)
endif
GLIB_CFLAGS              ?= $(call PKG_CFLAGS,glib-2.0)
GLIB_LIBS                ?= $(call PKG_LIBS,glib-2.0)
GDK_PIXBUF_CFLAGS        ?= $(call PKG_CFLAGS,gdk-pixbuf-2.0)
GDK_PIXBUF_LIBS          ?= $(call PKG_LIBS,gdk-pixbuf-2.0)
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
OGG_CFLAGS               ?= $(call PKG_CFLAGS,ogg)
OGG_LIBS                 ?= $(call PKG_LIBS,ogg)
INTL_LIBS                ?= -lintl
ifdef HAVE_XVID_H
XVID_CFLAGS              ?=
XVID_LIBS                ?= -lxvidcore
endif

uforadiant_LDFLAGS       += -headerpad_max_install_names
ufoded_LDFLAGS           += -headerpad_max_install_names
