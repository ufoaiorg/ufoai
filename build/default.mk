PKG_CONFIG               ?= PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(CROSS)pkg-config
CURL_LIBS                ?= $(call PKG_LIBS,libcurl)
CURL_CFLAGS              ?= $(call PKG_CFLAGS,libcurl)
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
else
JPEG_CFLAGS              ?= -Isrc/libs/jpeg -DAVOID_TABLES
JPEG_SRCS                 = \
    libs/jpeg/jaricom.c \
    libs/jpeg/jcapimin.c \
    libs/jpeg/jcapistd.c \
    libs/jpeg/jcarith.c \
    libs/jpeg/jccoefct.c \
    libs/jpeg/jccolor.c \
    libs/jpeg/jcdctmgr.c \
    libs/jpeg/jchuff.c \
    libs/jpeg/jcinit.c \
    libs/jpeg/jcmainct.c \
    libs/jpeg/jcmarker.c \
    libs/jpeg/jcmaster.c \
    libs/jpeg/jcomapi.c \
    libs/jpeg/jcparam.c \
    libs/jpeg/jcprepct.c \
    libs/jpeg/jcsample.c \
    libs/jpeg/jctrans.c \
    libs/jpeg/jdapimin.c \
    libs/jpeg/jdapistd.c \
    libs/jpeg/jdarith.c \
    libs/jpeg/jdatadst.c \
    libs/jpeg/jdatasrc.c \
    libs/jpeg/jdcoefct.c \
    libs/jpeg/jdcolor.c \
    libs/jpeg/jddctmgr.c \
    libs/jpeg/jdhuff.c \
    libs/jpeg/jdinput.c \
    libs/jpeg/jdmainct.c \
    libs/jpeg/jdmarker.c \
    libs/jpeg/jdmaster.c \
    libs/jpeg/jdmerge.c \
    libs/jpeg/jdpostct.c \
    libs/jpeg/jdsample.c \
    libs/jpeg/jdtrans.c \
    libs/jpeg/jerror.c \
    libs/jpeg/jfdctflt.c \
    libs/jpeg/jfdctfst.c \
    libs/jpeg/jfdctint.c \
    libs/jpeg/jidctflt.c \
    libs/jpeg/jidctint.c \
    libs/jpeg/jquant1.c \
    libs/jpeg/jquant2.c \
    libs/jpeg/jutils.c \
    libs/jpeg/jmemmgr.c

ifeq ($(TARGET_OS),android)
JPEG_SRCS += \
	libs/jpeg/jmem-android.c \
	libs/jpeg/jidctfst.S
else
JPEG_SRCS += \
	libs/jpeg/jmemansi.c \
	libs/jpeg/jidctfst.c
endif
endif
ifdef HAVE_SDL2_SDL_H
SDL_LIBS                 ?= $(call PKG_LIBS,sdl2)
SDL_CFLAGS               ?= $(call PKG_CFLAGS,sdl2)
else
SDL_LIBS                 ?= $(call PKG_LIBS,sdl)
SDL_CFLAGS               ?= $(call PKG_CFLAGS,sdl)
endif
ifdef HAVE_SDL2_TTF_SDL_TTF_H
SDL_TTF_LIBS             ?= $(call PKG_LIBS,SDL2_ttf)
SDL_TTF_CFLAGS           ?= $(call PKG_CFLAGS,SDL2_ttf)
else
SDL_TTF_LIBS             ?= $(call PKG_LIBS,SDL_ttf)
SDL_TTF_CFLAGS           ?= $(call PKG_CFLAGS,SDL_ttf)
endif
ifdef HAVE_SDL2_MIXER_SDL_MIXER_H
SDL_MIXER_LIBS           ?= $(call PKG_LIBS,SDL2_mixer)
SDL_MIXER_CFLAGS         ?= $(call PKG_CFLAGS,SDL2_mixer)
else
ifdef HAVE_SDL_MIXER_SDL_MIXER_H
SDL_MIXER_LIBS           ?= $(call PKG_LIBS,SDL_mixer)
SDL_MIXER_CFLAGS         ?= $(call PKG_CFLAGS,SDL_mixer)
else
ifdef HAVE_SDL2_SDL_H
SDL_MIXER_LIBS           ?=
SDL_MIXER_CFLAGS         ?= -Isrc/libs/vorbis -Isrc/libs/vorbis/src -Isrc/libs/vorbis/include -Isrc/libs/ogg/include -Isrc/libs/SDL_mixer -DOGG_MUSIC -DWAV_MUSIC -DHAVE_SDL_MIXER_H
SDL_MIXER_SRCS            = \
	libs/vorbis/src/analysis.c \
	libs/vorbis/src/bitrate.c \
	libs/vorbis/src/block.c \
	libs/vorbis/src/codebook.c \
	libs/vorbis/src/envelope.c \
	libs/vorbis/src/floor0.c \
	libs/vorbis/src/floor1.c \
	libs/vorbis/src/info.c \
	libs/vorbis/src/lookup.c \
	libs/vorbis/src/lpc.c \
	libs/vorbis/src/lsp.c \
	libs/vorbis/src/mapping0.c \
	libs/vorbis/src/mdct.c \
	libs/vorbis/src/psy.c \
	libs/vorbis/src/registry.c \
	libs/vorbis/src/res0.c \
	libs/vorbis/src/sharedbook.c \
	libs/vorbis/src/smallft.c \
	libs/vorbis/src/synthesis.c \
	libs/vorbis/src/vorbisenc.c \
	libs/vorbis/src/vorbisfile.c \
	libs/vorbis/src/window.c \
	libs/ogg/src/framing.c \
	libs/ogg/src/bitwise.c \
	libs/SDL_mixer/dynamic_flac.c \
	libs/SDL_mixer/dynamic_fluidsynth.c \
	libs/SDL_mixer/dynamic_mod.c \
	libs/SDL_mixer/dynamic_mp3.c \
	libs/SDL_mixer/dynamic_ogg.c \
	libs/SDL_mixer/effect_position.c \
	libs/SDL_mixer/effects_internal.c \
	libs/SDL_mixer/effect_stereoreverse.c \
	libs/SDL_mixer/fluidsynth.c \
	libs/SDL_mixer/load_aiff.c \
	libs/SDL_mixer/load_flac.c \
	libs/SDL_mixer/load_ogg.c \
	libs/SDL_mixer/load_voc.c \
	libs/SDL_mixer/mixer.c \
	libs/SDL_mixer/music.c \
	libs/SDL_mixer/music_cmd.c \
	libs/SDL_mixer/music_flac.c \
	libs/SDL_mixer/music_mad.c \
	libs/SDL_mixer/music_mod.c \
	libs/SDL_mixer/music_modplug.c \
	libs/SDL_mixer/music_ogg.c \
	libs/SDL_mixer/wavestream.c
else
SDL_MIXER_LIBS           ?= $(call PKG_LIBS,SDL_mixer)
SDL_MIXER_CFLAGS         ?= $(call PKG_CFLAGS,SDL_mixer)
endif
endif
endif
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
MXML_CFLAGS              ?= $(call PKG_CFLAGS,mxml)
MXML_LIBS                ?= $(call PKG_LIBS,mxml)
PICOMODEL_CFLAGS         ?= $(call PKG_CFLAGS,picomodel)
PICOMODEL_LIBS           ?= $(call PKG_LIBS,picomodel)
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
ifndef HAVE_MXML_MXML_H
MXML_SRCS                 = libs/mxml/mxml-attr.c \
                            libs/mxml/mxml-entity.c \
                            libs/mxml/mxml-file.c \
                            libs/mxml/mxml-index.c \
                            libs/mxml/mxml-node.c \
                            libs/mxml/mxml-private.c \
                            libs/mxml/mxml-search.c \
                            libs/mxml/mxml-set.c \
                            libs/mxml/mxml-string.c
MXML_CFLAGS               = -Isrc/libs/mxml
ifeq ($(findstring $(TARGET_OS), mingw32 mingw64),)
MXML_LIBS                 = -lpthread
endif
else
MXML_SRCS                 =
endif
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
