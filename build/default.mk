PKG_CONFIG               ?= PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(CROSS)pkg-config

ifdef HAVE_LIBCURL_CURL_H
CURL_LIBS                ?= $(call PKG_LIBS,libcurl)
CURL_CFLAGS              ?= $(call PKG_CFLAGS,libcurl)
endif

ifdef HAVE_LUA5_4_LUA_H
LUA_LIBS                 ?= $(call PKG_LIBS,"lua5.4 lua-5.4 lua54 lua")
LUA_CFLAGS               ?= $(call PKG_CFLAGS,"lua5.4 lua-5.4 lua54 lua")
LUA_SRCS                  =
endif

ifdef HAVE_LIBPNG_PNG_H
PNG_LIBS                 ?= -lpng
PNG_CFLAGS               ?=
else
PNG_CFLAGS               ?= -Isrc/libs/png -DPNG_NO_CONFIG_H
PNG_SRCS                  = \
	libs/png/png.c \
	libs/png/pngerror.c \
	libs/png/pngget.c \
	libs/png/pngmem.c \
	libs/png/pngpread.c \
	libs/png/pngread.c \
	libs/png/pngrio.c \
	libs/png/pngrtran.c \
	libs/png/pngrutil.c \
	libs/png/pngset.c \
	libs/png/pngtrans.c \
	libs/png/pngwio.c \
	libs/png/pngwrite.c \
	libs/png/pngwtran.c \
	libs/png/pngwutil.c
endif

ifdef HAVE_JPEGLIB_H
JPEG_LIBS                ?= -ljpeg
JPEG_CFLAGS              ?=
endif

ifdef HAVE_SDL2_SDL_H
SDL_LIBS                 ?= $(call PKG_LIBS,sdl2)
SDL_CFLAGS               ?= $(call PKG_CFLAGS,sdl2)
endif

ifdef HAVE_SDL2_TTF_SDL_TTF_H
SDL_TTF_LIBS             ?= $(call PKG_LIBS,SDL2_ttf)
SDL_TTF_CFLAGS           ?= $(call PKG_CFLAGS,SDL2_ttf)
endif

ifdef HAVE_SDL2_MIXER_SDL_MIXER_H
SDL_MIXER_LIBS           ?= $(call PKG_LIBS,SDL2_mixer)
SDL_MIXER_CFLAGS         ?= $(call PKG_CFLAGS,SDL2_mixer)
endif

OPENGL_CFLAGS            ?= $(call PKG_CFLAGS,gl,GL)
OPENGL_LIBS              ?= $(call PKG_LIBS,gl,GL)
OPENAL_CFLAGS            ?= $(call PKG_CFLAGS,openal)
OPENAL_LIBS              ?= $(call PKG_LIBS,openal)

ifdef HAVE_THEORA_THEORA_H
THEORA_CFLAGS            ?= $(call PKG_CFLAGS,theora)
THEORA_LIBS              ?= $(call PKG_LIBS,theora)
else

endif
GLIB_CFLAGS              ?= $(call PKG_CFLAGS,glib-2.0)
GLIB_LIBS                ?= $(call PKG_LIBS,glib-2.0)
GDK_PIXBUF_CFLAGS        ?= $(call PKG_CFLAGS,gdk-pixbuf-2.0)
GDK_PIXBUF_LIBS          ?= $(call PKG_LIBS,gdk-pixbuf-2.0)
GTK_CFLAGS               ?= $(call PKG_CFLAGS,gtk+-2.0)
GTK_LIBS                 ?= $(call PKG_LIBS,gtk+-2.0)

ifdef HAVE_GTKSOURCEVIEW_2_0_GTKSOURCEVIEW_H
GTK_SOURCEVIEW_CFLAGS    ?= $(call PKG_CFLAGS,gtksourceview-2.0)
GTK_SOURCEVIEW_LIBS      ?= $(call PKG_LIBS,gtksourceview-2.0)
GTK_SOURCEVIEW_SRCS      =
else
GTK_SOURCEVIEW_CFLAGS    ?= -Isrc/libs -Isrc/libs/gtksourceview -DDATADIR="\"dummy\""
GTK_SOURCEVIEW_LIBS      =
GTK_SOURCEVIEW_SRCS      ?= \
	libs/gtksourceview/gtksourcebuffer.c \
	libs/gtksourceview/gtksourcecompletion.c \
	libs/gtksourceview/gtksourcecompletioncontext.c \
	libs/gtksourceview/gtksourcecompletioninfo.c \
	libs/gtksourceview/gtksourcecompletionitem.c \
	libs/gtksourceview/gtksourcecompletionmodel.c \
	libs/gtksourceview/gtksourcecompletionproposal.c \
	libs/gtksourceview/gtksourcecompletionprovider.c \
	libs/gtksourceview/gtksourcecompletionutils.c \
	libs/gtksourceview/gtksourcecontextengine.c \
	libs/gtksourceview/gtksourceengine.c \
	libs/gtksourceview/gtksourcegutter.c \
	libs/gtksourceview/gtksourceiter.c \
	libs/gtksourceview/gtksourcelanguage.c \
	libs/gtksourceview/gtksourcelanguagemanager.c \
	libs/gtksourceview/gtksourcelanguage-parser-1.c \
	libs/gtksourceview/gtksourcelanguage-parser-2.c \
	libs/gtksourceview/gtksourcemark.c \
	libs/gtksourceview/gtksourceprintcompositor.c \
	libs/gtksourceview/gtksourcestyle.c \
	libs/gtksourceview/gtksourcestylescheme.c \
	libs/gtksourceview/gtksourcestyleschememanager.c \
	libs/gtksourceview/gtksourceundomanager.c \
	libs/gtksourceview/gtksourceundomanagerdefault.c \
	libs/gtksourceview/gtksourceview.c \
	libs/gtksourceview/gtksourceview-marshal.c \
	libs/gtksourceview/gtksourceview-typebuiltins.c \
	libs/gtksourceview/gtksourceview-utils.c \
	libs/gtksourceview/gtktextregion.c \
	libs/gtksourceview/gtksourceview-i18n.c
endif

GTK_GLEXT_CFLAGS         ?= $(call PKG_CFLAGS,gtkglext-1.0)
GTK_GLEXT_LIBS           ?= $(call PKG_LIBS,gtkglext-1.0)

XML2_CFLAGS              ?= $(call PKG_CFLAGS,libxml-2.0)
XML2_LIBS                ?= $(call PKG_LIBS,libxml-2.0)

VORBIS_CFLAGS            ?= $(call PKG_CFLAGS,vorbis)
VORBIS_LIBS              ?= $(call PKG_LIBS,vorbis)

OGG_CFLAGS               ?= $(call PKG_CFLAGS,ogg)
OGG_LIBS                 ?= $(call PKG_LIBS,ogg)

INTL_LIBS                ?=

ifdef HAVE_XVID_H
XVID_CFLAGS              ?=
XVID_LIBS                ?= -lxvidcore
endif

ifdef HAVE_BFD_H
BFD_CFLAGS               ?=
BFD_LIBS                 ?= -lbfd -liberty
endif

MUMBLE_LIBS              ?=
MUMBLE_SRCS               = libs/mumble/libmumblelink.c
MUMBLE_CFLAGS             = -Isrc/libs/mumble

MXML_CFLAGS              ?= $(call PKG_CFLAGS,mxml)
MXML_LIBS                ?= $(call PKG_LIBS,mxml)
ifndef HAVE_MXML_MXML_H
MXML_SRCS                 = libs/mxml-3.3.1/mxml-attr.c \
                            libs/mxml-3.3.1/mxml-entity.c \
                            libs/mxml-3.3.1/mxml-file.c \
                            libs/mxml-3.3.1/mxml-get.c \
                            libs/mxml-3.3.1/mxml-index.c \
                            libs/mxml-3.3.1/mxml-node.c \
                            libs/mxml-3.3.1/mxml-private.c \
                            libs/mxml-3.3.1/mxml-search.c \
                            libs/mxml-3.3.1/mxml-set.c \
                            libs/mxml-3.3.1/mxml-string.c
MXML_CFLAGS               = -Isrc/libs/mxml-3.3.1
MXML_LIBS                 =
ifeq ($(findstring $(TARGET_OS), mingw32 mingw64),)
MXML_LIBS                 = -lpthread
endif
else
MXML_SRCS                 =
endif

PICOMODEL_CFLAGS         ?= $(call PKG_CFLAGS,picomodel)
PICOMODEL_LIBS           ?= $(call PKG_LIBS,picomodel)
ifndef HAVE_PICOMODEL_PICOMODEL_H
PICOMODEL_SRCS            = libs/picomodel/picointernal.c \
                            libs/picomodel/picomodel.c \
                            libs/picomodel/picomodules.c \
                            libs/picomodel/pm_ase.c \
                            libs/picomodel/pm_md3.c \
                            libs/picomodel/pm_obj.c \
                            libs/picomodel/pm_md2.c
PICOMODEL_CFLAGS          = -Isrc/libs/picomodel
PICOMODEL_LIBS            =
else
PICOMODEL_SRCS            =
endif

GTEST_CFLAGS             ?= -Isrc/libs/gtest/include -Isrc/libs/gtest
GTEST_LIBS               += -lpthread
GTEST_SRCS                = libs/gtest/src/gtest-all.cc
