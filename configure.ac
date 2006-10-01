AC_INIT(src/server/sv_main.c)
AC_CANONICAL_TARGET
AC_ARG_WITH(sdl,
	[AS_HELP_STRING([--with-sdl], [with SDL support @<:@default=yes@:>@])],
	[], [with_sdl=yes])

AC_ARG_WITH(vid_glx,
	[AS_HELP_STRING([--with-glx], [with GLX support @<:@default=check@:>@])],
	[], [with_vid_glx=check])

AC_ARG_WITH(shaders,
	[AS_HELP_STRING([--with-shaders], [with OpenGL shader support @<:@default=check@:>@])],
	[], [with_shaders=check])

AC_ARG_WITH(snd_sdl,
    [AS_HELP_STRING([--with-snd-sdl], [with SDL sound support @<:@default=check@:>@])],
	[], [with_snd_sdl=check])

AC_ARG_WITH(snd_alsa,
	[AS_HELP_STRING([--with-snd-alsa], [with ALSA sound support @<:@default=check@:>@])],
	[], [with_snd_alsa=check])

AC_ARG_WITH(snd_oss,
	[AS_HELP_STRING([--with-snd-oss], [with OSS sound support @<:@default=check@:>@])],
	[], [with_snd_oss=check])

AC_ARG_WITH(snd_arts,
    [AS_HELP_STRING([--with-snd-arts], [with arts sound support @<:@default=check@:>@])],
	[], [with_snd_arts=check])

AC_ARG_WITH(vid_dga,
	[AS_HELP_STRING([--with-dga], [compile with DGA support  @<:@default=check@:>@])],
	[], [with_vid_dga=check])

AC_ARG_WITH(vid_vidmode,
	[AS_HELP_STRING([--with-vidmode], [compile with vidmode support @<:@default=check@:>@])],
	[], [with_vid_vidmode=check])

AC_ARG_ENABLE(dedicated,
	[AS_HELP_STRING([--disable-dedicated], [build dedicated server @<:@default=enabled@:>@])],
	[], [enable_dedicated=yes])

AC_ARG_ENABLE(ufo2map,
	[AS_HELP_STRING([--disable-ufo2map], [build ufo2map @<:@default=enabled@:>@])],
	[], [enable_ufo2map=yes])

AC_ARG_ENABLE(qdata,
	[AS_HELP_STRING([--disable-qdata], [build qdata @<:@default=enabled@:>@])],
	[], [enable_qdata=yes])

AC_ARG_ENABLE(gettext,
	[AS_HELP_STRING([--disable-gettext], [build with multilanguage support @<:@default=enabled@:>@])],
	[], [enable_gettext=yes])

AC_ARG_ENABLE(release,
	[AS_HELP_STRING([--enable-release], [build release, if disabled then build debug @<:@default=disabled@:>@])],
	[], [enable_release=no])

AC_ARG_ENABLE(paranoid,
	[AS_HELP_STRING([--enable-paranoid], [enable PARANOID checks @<:@default=disabled@:>@])],
	[], [enable_paranoid=no])

AC_ARG_ENABLE(profiling,
	[AS_HELP_STRING([--enable-profiling], [build with profiling for gprof @<:@default=disabled@:>@])],
	[], [enable_profiling=no])

#---------------------------------------------------------------------------------------------
# CHECK TARGET_OS
#---------------------------------------------------------------------------------------------

if test "$target_os" == "linux-gnu"
then
	AC_SUBST([TARGET_OS],[linux-gnu])
else
	AC_MSG_ERROR([The target operating system '$target_os' is unknown, please contact the developers])
fi

#---------------------------------------------------------------------------------------------
# CHECK TARGET_CPU
#---------------------------------------------------------------------------------------------

if test "$target_cpu" == "x86_64"
then
	AC_SUBST([TARGET_CPU],[x86_64])
elif test "$target_cpu" == "i386" ||
	 test "$target_cpu" == "i486" ||
	 test "$target_cpu" == "i586" ||
	 test "$target_cpu" == "i686"
then
	AC_SUBST([TARGET_CPU],[i386])
else
	AC_MSG_WARN([The target CPU '$target_cpu' is unknown, default options will be applied])
	AC_MSG_WARN([Please contact the developers for assistance])
	AC_SUBST([TARGET_CPU],[unknown])
fi

#---------------------------------------------------------------------------------------------
# CHECK CC
#---------------------------------------------------------------------------------------------

AC_PROG_CC

#---------------------------------------------------------------------------------------------
# CHECK RM
#---------------------------------------------------------------------------------------------

AC_CHECK_PROG(HAVE_RM,rm,yes)
if test $HAVE_RM != "yes"
then
	AC_MSG_ERROR([You must have the rm program installed])
fi

#---------------------------------------------------------------------------------------------
# CHECK MKDIR
#---------------------------------------------------------------------------------------------

AC_CHECK_PROG(HAVE_MKDIR,mkdir,yes)
if test $HAVE_MKDIR != "yes"
then
	AC_MSG_ERROR([You must have the mkdir program installed])
fi

#---------------------------------------------------------------------------------------------
# CHECK LIBTOOL
#---------------------------------------------------------------------------------------------

AC_CHECK_PROG(HAVE_LIBTOOL,libtool,yes)
if test "$HAVE_LIBTOOL" != "yes"
then
	AC_MSG_ERROR([You must have libtool installed])
fi

#---------------------------------------------------------------------------------------------
# Check for X11/Xlib.h
#---------------------------------------------------------------------------------------------

AC_CHECK_HEADERS([X11/Xlib.h])
if test "$ac_cv_header_X11_Xlib_h" == "yes"
then
	AC_SEARCH_LIBS([XCreateWindow],[X11])
	if test "$ac_cv_search_XCreateWindow" == "no"
	then
		AC_MSG_WARN([found X11 header but couldn't find library!])
	fi
fi

#---------------------------------------------------------------------------------------------
# Check how to open a dynamic library ...
#---------------------------------------------------------------------------------------------

AC_SEARCH_LIBS([dlopen],[dl])
if test "$ac_cv_search_dlopen" == "no"
then
	AC_MSG_ERROR([could not decide how to open a dynamic link library!])
fi

#---------------------------------------------------------------------------------------------
# Check how to do maths
#---------------------------------------------------------------------------------------------

AC_SEARCH_LIBS([cos],[m])
if test "$ac_cv_search_cos" == "no"
then
	AC_MSG_ERROR([could not determine how to link to maths library])
fi

#---------------------------------------------------------------------------------------------
# Check how to do stricmp
#---------------------------------------------------------------------------------------------

AC_SEARCH_LIBS([stricmp],[])
if test "$ac_cv_search_stricmp" == "no"
then
	AC_SEARCH_LIBS([strcasecmp], [])
	if test "$ac_cv_search_strcasecmp" == "no"
	then
	   AC_MSG_ERROR([could find neither stricmp nor strcasecmp!])
	else
	   AC_DEFINE([stricmp],[strcasecmp])
	fi
fi

#---------------------------------------------------------------------------------------------
# Check for libvorbis file
#---------------------------------------------------------------------------------------------

AC_SEARCH_LIBS([ov_open],[vorbisfile])
if test "$ac_cv_search_ov_open" == "no"
then
	AC_MSG_ERROR([couldn't find libvorbisfile!])
fi

#---------------------------------------------------------------------------------------------
# Check SDL
#---------------------------------------------------------------------------------------------

AC_CHECK_PROG(HAVE_SDL_CONFIG,sdl-config,yes)
if test "$ac_cv_prog_HAVE_SDL_CONFIG" == "yes"
then
	AC_SUBST([SDL_CFLAGS],`sdl-config --cflags`)
	AC_SUBST([SDL_LIBS],`sdl-config --libs`)
else
	AC_MSG_ERROR([cannot find sdl-config, make sure the SDL development libraries are installed!])
fi

if test "$with_sdl" == "check" || test "$with_sdl" == "yes" || \
   test "$with_snd_sdl" == "check" || test "$with_snd_sdl" == "yes"
then
	AC_SUBST([HAVE_SDL],1)
else
	AC_SUBST([HAVE_SDL],0)
fi

if test "$with_snd_sdl" == "check" || test "$with_snd_sdl" == "yes"
then
	AC_SUBST([HAVE_SND_SDL],1)
else
	AC_SUBST([HAVE_SND_SDL],0)
fi

#---------------------------------------------------------------------------------------------
# Check for SDL_ttf
#---------------------------------------------------------------------------------------------

AC_SEARCH_LIBS([TTF_Init],[SDL_ttf])
if test "$ac_cv_search_TTF_Init" == "no"
then
	AC_MSG_ERROR([You must have the SDL_ttf development libraries])
fi

#---------------------------------------------------------------------------------------------
# Check JPEG
#---------------------------------------------------------------------------------------------

AC_CHECK_HEADERS([jpeglib.h])
if test "$ac_cv_header_jpeglib_h" == "no"
then
	AC_MSG_ERROR([You must have the JPEG development headers])
fi

AC_SEARCH_LIBS([jpeg_CreateDecompress],[jpeg])
if test "$ac_cv_search_jpeg_create_decompress" == "no"
then
	AC_MSG_ERROR([You must have the JPEG development libraries])
fi

#---------------------------------------------------------------------------------------------
# Check GLX
#---------------------------------------------------------------------------------------------

AC_SUBST([HAVE_VID_GLX],0)
if test "$with_vid_glx" == "check" || test "$with_vid_glx" == "yes"
then
	if test "$ac_cv_header_X11_Xlib_h" != "no"
	then
		AC_CHECK_HEADERS([GL/glx.h], [], [], [
				#if HAVE_X11_XLIB_H
				#include <X11/Xlib.h>
				#endif
			])
	fi
	if test "$ac_cv_header_GL_glx_h" == "yes"
	then
		AC_SEARCH_LIBS([glXCreateContext],[GL MesaGL])
		if test "$ac_cv_search_glXCreateContext" == "no"
		then
		   AC_MSG_WARN([found GLX headers but couldn't find GLX libraries, GLX will be disabled!])
		else
		   AC_SUBST([HAVE_VID_GLX],1)
		fi
	elif test "$with_vid_glx" == "yes"
	then
		AC_MSG_ERROR([--with-glx specified but failed find GLX header])
	fi
fi

#---------------------------------------------------------------------------------------------
# Check for opengl headers
#---------------------------------------------------------------------------------------------

AC_CHECK_HEADERS([GL/gl.h GL/glu.h])

if test "$ac_cv_header_GL_gl_h" != "yes" || \
   test "$ac_cv_header_GL_glu_h" != "yes"
then
	AC_MSG_ERROR([Couldn't find OpenGL development headers!])
fi

#---------------------------------------------------------------------------------------------
# Check for shaders
#---------------------------------------------------------------------------------------------

if test "$with_shaders" == "check" || test "$with_shaders" == "yes"
then
	shaders_enabled=no
	AC_CHECK_HEADERS([GL/glext.h],[],[],[
		#include <GL/gl.h>
		#include <GL/glu.h>
	])

	if test "$ac_cv_header_GL_glext_h" == "yes"
	then
		AC_MSG_CHECKING([OpenGL shader support])
		AC_COMPILE_IFELSE([
				AC_LANG_PROGRAM([[
					#include <GL/gl.h>
					#include <GL/glu.h>
					#include <GL/glext.h>
					]]
					,[[glEnable(GL_FRAGMENT_PROGRAM_ARB);]]
				)],
				[ac_cv_shaders=yes],
				[ac_cv_shaders=]
			)
		if test "$ac_cv_shaders" == "yes"
		then
			AC_MSG_RESULT([yes])
			AC_SUBST([HAVE_SHADERS],1)
			AC_DEFINE([SHADERS],1)
			shaders_enabled=yes
		else
			AC_MSG_RESULT([no])
		fi
	fi
	if test "$with_shaders" == "yes" && test "shaders_enabled" != "yes"
	then
		AC_MSG_ERROR([--with-shaders specified but couldn't use OpenGL shaders!])
	fi
fi

#---------------------------------------------------------------------------------------------
# Check ALSA
#---------------------------------------------------------------------------------------------

if test "$with_snd_alsa" == "check" || test "$with_snd_alsa" == "yes"
then
	AC_CHECK_HEADERS([alsa/asoundlib.h])
	if test "$ac_cv_header_alsa_asoundlib_h" == "yes"
	then
		AC_SEARCH_LIBS([snd_pcm_open], [asound])
		if test "$ac_cv_search_snd_pcm_open" != "no"
		then
			AC_SUBST([HAVE_SND_ALSA],1)
			AC_DEFINE([USE_ALSA],1)
		else
			AC_MSG_ERROR([found alsa header but couldn't locate alsa library!])
		fi
	elif test "$with_snd_alsa" == "yes"
	then
		AC_MSG_ERROR([specified --with-snd-alsa specified but cannot find alsa/asoundlib.h!])
	fi
fi

#---------------------------------------------------------------------------------------------
# Check OSS
#---------------------------------------------------------------------------------------------

if test "$with_snd_alsa" == "check" || test "$with_snd_alsa" == "yes"
then
   AC_CHECK_HEADERS([linux/soundcard.h])
   if test "$ac_cv_header_linux_soundcard_h" == "yes"
   then
		AC_DEFINE([OSS_HEADER],[linux])
   else
     AC_CHECK_HEADERS([sys/soundcard.h])
     if test "$ac_cv_header_sys_soundcard_h" == "yes"
	 then
	 	AC_DEFINE([OSS_HEADER],[sys])
	 fi
   fi
   if test "$ac_cv_header_linux_soundcard_h" == "yes" || \
	  test "$ac_cv_header_sys_soundcard_h" == "yes"
   then
     AC_SUBST([HAVE_SND_OSS],1)
   fi
fi

#---------------------------------------------------------------------------------------------
# Check ARTS
#---------------------------------------------------------------------------------------------

if test "$with_snd_arts" == check || test "$with_snd_arts" == yes
then
	AC_CHECK_PROG(HAVE_ARTSC_CONFIG,artsc-config,yes)
	if test "$ac_cv_prog_HAVE_ARTSC_CONFIG" == "yes"
	then
		AC_SUBST([HAVE_SND_ARTS],1)
		AC_SUBST([ARTS_CFLAGS], `artsc-config --cflags`)
		AC_SUBST([ARTS_LIBS], `artsc-config --libs`)
	fi
fi

#---------------------------------------------------------------------------------------------
# Check for DGA
#---------------------------------------------------------------------------------------------

if test "$with_vid_dga" == "check" || test "$with_vid_dga" == "yes"
then
	if test "$ac_cv_header_X11_Xlib_h" == "yes"
	then
		AC_CHECK_HEADERS([X11/extensions/xf86dga.h], [], [], [
			#if HAVE_X11_XLIB_H
			#include <X11/Xlib.h>
			#endif
			])
	fi

	if test "$ac_cv_header_X11_extensions_xf86dga_h" == "yes"
	then
		AC_SUBST([HAVE_VID_DGA], 1)
		AC_DEFINE([HAVE_DGA],1)
	elif test "$with_vid_dga" == "yes"
	then
		AC_MSG_ERROR([--with-dga specified but couldn't find X11/extensions/xf86dga.h])
	fi
fi

#---------------------------------------------------------------------------------------------
# Check for vidmode
#---------------------------------------------------------------------------------------------

if test "$with_vid_vidmode" == "check" || test "$with_vid_vidmode" == "yes"
then
	if test "$ac_cv_header_X11_Xlib_h" == "yes"
	then
		AC_CHECK_HEADERS([X11/extensions/xf86vmode.h],[],[],[
			#if HAVE_X11_XLIB_H
			#include <X11/Xlib.h>
			#endif
			])
	fi

	if test "$ac_cv_header_X11_extensions_xf86vmode_h" == "yes"
	then
		AC_SUBST([HAVE_VID_VIDMODE], 1)
		AC_DEFINE([HAVE_VIDMODE],1)
	elif test "$with_vid_vidmode" == "yes"
	then
		AC_MSG_ERROR([--with-vidmode specified but couldn't find X11/extensions/xf86vmode.h])
	fi
fi


#---------------------------------------------------------------------------------------------
# Check for dedicated
#---------------------------------------------------------------------------------------------

AC_SUBST([BUILD_DEDICATED],0)
if test "$enable_dedicated" == "yes"
then
	AC_MSG_NOTICE([Enabling dedicated server])
	AC_SUBST([BUILD_DEDICATED],1)
fi

#---------------------------------------------------------------------------------------------
# Check for ufo2map
#---------------------------------------------------------------------------------------------

AC_SUBST([BUILD_UFO2MAP],0)
if test "$enable_ufo2map" == "yes"
then
	AC_MSG_NOTICE([Enabling ufo2map])
	AC_SUBST([BUILD_UFO2MAP],1)
fi

#---------------------------------------------------------------------------------------------
# Check for qdata
#---------------------------------------------------------------------------------------------

AC_SUBST([BUILD_QDATA],0)
if test "$enable_qdata" == "yes"
then
	AC_MSG_NOTICE([Enabling qdata])
	AC_SUBST([BUILD_QDATA],1)
fi

#---------------------------------------------------------------------------------------------
# Check for gettext
#---------------------------------------------------------------------------------------------

if test "$enable_gettext" == "yes"
then
	AC_MSG_NOTICE([Enabling multilanguage support])
	AC_DEFINE([HAVE_GETTEXT],1)
fi

#---------------------------------------------------------------------------------------------
# Check for debug
#---------------------------------------------------------------------------------------------

if test "$enable_release" == "no"
then
	AC_MSG_NOTICE([Enabling debug build])
	AC_SUBST([BUILD_DEBUG],1)
else
	AC_MSG_NOTICE([Enabling release build])
	AC_SUBST([BUILD_DEBUG],0)
fi

#---------------------------------------------------------------------------------------------
# Check for PARANOID
#---------------------------------------------------------------------------------------------

if test "$enable_paranoid" == "yes"
then
	AC_SUBST([PARANOID],1)
	AC_MSG_NOTICE([Enabling PARANOID])
fi

#---------------------------------------------------------------------------------------------
# Check for PROFILING
#---------------------------------------------------------------------------------------------

if test "$enable_profiling" == "yes"
then
	AC_SUBST([PROFILING],1)
	AC_MSG_NOTICE([Enabling profiling])
else
	AC_SUBST([PROFILING],0)
fi

#---------------------------------------------------------------------------------------------

AC_SUBST([EXE_EXT],"$ac_exe_ext")

AC_CONFIG_HEADERS([config.h:config.h.in])
AC_OUTPUT(Makefile)
