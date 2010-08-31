^AC_INIT(ufoai, 2.4-dev, tlh2000@users.sourceforge.net)

AC_CONFIG_SRCDIR([src/server/sv_main.c])

# check build/host/target systems
# (build  = system we're building on,
#  host   = system we're building for,
#  target = system the program we're building will build for)
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_MSG_CHECKING(system)
case "$host_os" in
	linux*)
		AC_MSG_RESULT([UNIX (GNU/Linux)])
		;;
	mingw32*)
		AC_MSG_RESULT([Win32 (mingw32)])
		;;
	cygwin*)
		AC_MSG_RESULT([Win32 (cygwin)])
		;;
	freebsd*)
		AC_MSG_RESULT([UNIX (FreeBSD)])
		;;
	openbsd*)
		AC_MSG_RESULT([UNIX (OpenBSD)])
		;;
	solaris*)
		AC_MSG_RESULT([UNIX (Solaris)])
		;;
	darwin*)
		AC_MSG_RESULT([Mac OS X])
		;;
	*)
		AC_MSG_RESULT([not sure... guessing a unix variant])
		;;
esac

AC_ARG_WITH(sdl_config,
	[AS_HELP_STRING([--with-sdl-config=FILE],[using the given sdl-config @<:@default=sdl-config@:>@])],
	[], [with_sdl_config=sdl-config])

AC_ARG_WITH(curl_config,
	[AS_HELP_STRING([--with-curl-config=FILE],[using the given curl-config @<:@default=curl-config@:>@])],
	[], [with_curl_config=curl-config])

AC_ARG_WITH(xml2_config,
	[AS_HELP_STRING([--with-xml2-config=FILE],[using the given xml2-config @<:@default=xml2-config@:>@])],
	[], [with_xml2_config=xml2-config])

AC_ARG_WITH(msys,
	[AS_HELP_STRING([--with-msys], [compile with msys support @<:@default=check@:>@])],
	[], [with_msys=check])

AC_ARG_ENABLE(dedicated,
	[AS_HELP_STRING([--disable-dedicated], [build dedicated server @<:@default=enabled@:>@])],
	[], [enable_dedicated=yes])

AC_ARG_ENABLE(client,
	[AS_HELP_STRING([--disable-client], [build client @<:@default=enabled@:>@])],
	[], [enable_client=yes])

AC_ARG_ENABLE(ufomodel,
	[AS_HELP_STRING([--disable-ufomodel], [build ufomodel @<:@default=enabled@:>@])],
	[], [enable_ufomodel=yes])

AC_ARG_ENABLE(ufo2map,
	[AS_HELP_STRING([--disable-ufo2map], [build ufo2map @<:@default=enabled@:>@])],
	[], [enable_ufo2map=yes])

AC_ARG_ENABLE(uforadiant,
	[AS_HELP_STRING([--disable-uforadiant], [build uforadiant @<:@default=enabled@:>@])],
	[], [enable_uforadiant=yes])

AC_ARG_ENABLE(release,
	[AS_HELP_STRING([--enable-release], [build release, if disabled then build debug @<:@default=disabled@:>@])],
	[], [enable_release=no])

AC_ARG_ENABLE(signals,
	[AS_HELP_STRING([--disable-signals], [build with signal handling @<:@default=enabled@:>@])],
	[], [enable_signals=yes])

AC_ARG_ENABLE(paranoid,
	[AS_HELP_STRING([--enable-paranoid], [enable PARANOID checks @<:@default=disabled@:>@])],
	[], [enable_paranoid=no])

AC_ARG_ENABLE(profiling,
	[AS_HELP_STRING([--enable-profiling], [build with profiling for gprof @<:@default=disabled@:>@])],
	[], [enable_profiling=no])

AC_ARG_ENABLE(hardlinkedgame,
	[AS_HELP_STRING([--enable-hardlinkedgame], [build with hard linked game @<:@default=disabled@:>@])],
	[], [enable_hardlinkedgame=no])

AC_ARG_ENABLE(tests,
	[AS_HELP_STRING([--enable-tests], [build with tests @<:@default=check@:>@])],
	[], [enable_tests=check])

AC_ARG_ENABLE(universal,
	[AS_HELP_STRING([--enable-universal], [build universal binaries on Mac OS X @<:@default=disabled@:>@])],
	[], [enable_universal=no])

#---------------------------------------------------------------------------------------------
# defined some defaults for users (mac-users..) who might not have pkg-config.
#---------------------------------------------------------------------------------------------
m4_define_default([PKG_CHECK_MODULES],
	[AC_MSG_ERROR([For current build options, pkg-config is required. Did you maybe forget to run aclocal with the current include path?])])

#---------------------------------------------------------------------------------------------
# CHECK SED
#---------------------------------------------------------------------------------------------

AC_CHECK_PROG(HAVE_SED,sed,yes)
if test $HAVE_SED != "yes"
then
	AC_MSG_ERROR([You must have sed installed])
fi

#---------------------------------------------------------------------------------------------
# CHECK ECHO
#---------------------------------------------------------------------------------------------

AC_CHECK_PROG(HAVE_ECHO,echo,yes)
if test $HAVE_ECHO != "yes"
then
	AC_MSG_ERROR([You must have a functioning echo installed])
fi

#---------------------------------------------------------------------------------------------
# CHECK TARGET_OS
#---------------------------------------------------------------------------------------------
AC_MSG_CHECKING([target OS])
if test `echo $target_os | sed "s/\(linux\).*/\1/"` = "linux"
then
	AC_SUBST([TARGET_OS],[linux-gnu])
elif test `echo $target_os | sed "s/\(freebsd\).*/\1/"` = "freebsd"
then
	AC_SUBST([TARGET_OS],[freebsd])
elif test `echo $target_os | sed "s/\(netbsd\).*/\1/"` = "netbsd"
then
	AC_SUBST([TARGET_OS],[netbsd])
elif test `echo $target_os | sed "s/\(mingw32\).*/\1/"` = "mingw32"
then
	AC_SUBST([TARGET_OS],[mingw32])
elif test `echo $target_os | sed "s/\(solaris\).*/\1/"` = "solaris"
then
	AC_SUBST([TARGET_OS],[solaris])
elif test `echo $target_os | sed "s/\(darwin\).*/\1/"` = "darwin"
then
	AC_SUBST([TARGET_OS],[darwin])
	### most mac users will have their additional libs and headers under /opt/local,
	### check for that, and if present, add to CFLAGS/LDFLAGS (really convenient!)
	if test -d /opt/local
	then
		CFLAGS="$CFLAGS -I/opt/local/include -F/opt/local/Library/Frameworks"
		LDFLAGS="$LDFLAGS -L/opt/local/lib -F/opt/local/Library/Frameworks"
	fi

	### Check for universal binaries
	if test "$enable_universal" = "yes"
	then
		RADIANT_CFLAGS="$RADIANT_CFLAGS -arch i386 -arch ppc"
		RADIANT_LIBS="$RADIANT_LIBS -arch i386 -arch ppc"
		CFLAGS="$CFLAGS -arch i386 -arch ppc"
		LDFLAGS="$LDFLAGS -arch i386 -arch ppc"
	fi
else
	AC_MSG_ERROR([The target operating system '$target_os' is unknown, please contact the developers])
fi
AC_MSG_RESULT($TARGET_OS)

#---------------------------------------------------------------------------------------------
# CHECK TARGET_CPU
#---------------------------------------------------------------------------------------------

AC_MSG_CHECKING([target CPU])
if test "$target_cpu" = "x86_64" || test "$target_cpu" = "amd64" || test "$target_cpu" = "ia64"
then
	AC_SUBST([TARGET_CPU],[x86_64])
elif test "$target_cpu" = "i386" || test "$target_cpu" = "i486" || test "$target_cpu" = "i586" || test "$target_cpu" = "i686"
then
	AC_SUBST([TARGET_CPU],[i386])
elif test "$target_cpu" = "powerpc"
then
	AC_SUBST([TARGET_CPU],[powerpc])
elif test "$target_cpu" = "powerpc64"
then
	AC_SUBST([TARGET_CPU],[powerpc64])
else
	AC_MSG_WARN([The target CPU '$target_cpu' is unknown, default options will be applied])
	AC_MSG_WARN([Please contact the developers for assistance])
	AC_SUBST([TARGET_CPU],[unknown])
fi

if test $TARGET_OS = "darwin"
then
	if test "$enable_universal" = yes
	then
		AC_SUBST([TARGET_CPU],[universal])
	fi
fi
AC_MSG_RESULT($TARGET_CPU)

#---------------------------------------------------------------------------------------------
# CHECK CC
#---------------------------------------------------------------------------------------------

AC_PROG_CC

#---------------------------------------------------------------------------------------------
# CHECK CPP
#---------------------------------------------------------------------------------------------

AC_PROG_CXX

#---------------------------------------------------------------------------------------------
# CHECK LIBTOOL
#---------------------------------------------------------------------------------------------

#AC_PROG_LIBTOOL

# ---------------------------------------------------------------------
# CHECK SIZES OF INTEGER TYPES
# ---------------------------------------------------------------------
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)
AC_CHECK_SIZEOF(int*)

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
# CHECK windres
#---------------------------------------------------------------------------------------------

if test $TARGET_OS = "mingw32"
then
	AC_ARG_VAR([WINDRES], [path to windres utility])
	if test "x$ac_cv_env_WINDRES_set" != "xset"
	then
		AC_PATH_TOOL([WINDRES], [windres])
	fi
	if test -z "$WINDRES"
	then
		AC_MSG_ERROR([You must have the windres program installed])
	fi
fi

#---------------------------------------------------------------------------------------------
# CHECK winsock2
#---------------------------------------------------------------------------------------------

if test $TARGET_OS = "mingw32"
then
	AC_MSG_RESULT([Mingw32 - Trying to use Winsock2 (WS2_32)])
	AC_CHECK_HEADERS([winsock2.h])
	AC_CHECK_LIB(ws2_32, main, [], AC_MSG_ERROR([Winsock2 is needed for a Windows mingw build.]))
fi

AC_CHECK_HEADERS([execinfo.h])

#---------------------------------------------------------------------------------------------
# CHECK NETWORKING
#---------------------------------------------------------------------------------------------

#---------------------------------------------------------------------------------------------
# CHECK IPV6 FUNCTION
#---------------------------------------------------------------------------------------------

AC_CHECK_FUNCS(getaddrinfo)
AC_CHECK_FUNCS(freeaddrinfo)

#---------------------------------------------------------------------------------------------
# CHECK AI_NUMERICSERV
#---------------------------------------------------------------------------------------------

AC_CHECK_DECL(AI_NUMERICSERV,
	[AC_DEFINE(HAVE_AI_NUMERICSERV,1,[AI_NUMERICSERV found in netdb.h])],
	[AC_DEFINE(HAVE_AI_NUMERICSERV,0,[AI_NUMERICSERV not found in netdb.h])],
	[#include <netdb.h>])

#---------------------------------------------------------------------------------------------
# CHECK SETPRIORITY
#---------------------------------------------------------------------------------------------

AC_CHECK_FUNCS(setpriority)

#---------------------------------------------------------------------------------------------
# CHECK STRNCASECMP
#---------------------------------------------------------------------------------------------

AC_CHECK_FUNCS(strncasecmp)

#---------------------------------------------------------------------------------------------
# Check for stdint.h
#---------------------------------------------------------------------------------------------

AC_CHECK_HEADERS([stdint.h])
if test "$ac_cv_header_stdint_h" != "yes"
then
	AC_MSG_ERROR([Could not find stdint.h!])
fi

#---------------------------------------------------------------------------------------------
# Check some headers
#---------------------------------------------------------------------------------------------

AC_CHECK_HEADERS([sys/time.h])
AC_CHECK_HEADERS([sys/resource.h])

#---------------------------------------------------------------------------------------------
# Check how to open a dynamic library ...
#---------------------------------------------------------------------------------------------

LIBS=
if test "$TARGET_OS" != "mingw32"
then
	AC_SEARCH_LIBS([dlopen],[dl])
	if test "$ac_cv_search_dlopen" = "no"
	then
		AC_MSG_ERROR([could not decide how to open a dynamic link library!])
	fi
fi
CLIENT_LIBS="$CLIENT_LIBS $LIBS"
SERVER_LIBS="$SERVER_LIBS $LIBS"
RADIANT_LIBS="$RADIANT_LIBS $LIBS"

#---------------------------------------------------------------------------------------------
# Check how to do maths
#---------------------------------------------------------------------------------------------

LIBS=
AC_SEARCH_LIBS([cos],[m])
if test "$ac_cv_search_cos" = "no"
then
	AC_MSG_ERROR([could not determine how to link to maths library])
fi
CLIENT_LIBS="$CLIENT_LIBS $LIBS"
SERVER_LIBS="$SERVER_LIBS $LIBS"
GAME_LIBS="$GAME_LIBS $LIBS"
TOOLS_LIBS="$TOOLS_LIBS $LIBS"

#---------------------------------------------------------------------------------------------
# Check how to do stricmp
#---------------------------------------------------------------------------------------------

LIBS=
AC_SEARCH_LIBS([stricmp],[])
if test "$ac_cv_search_stricmp" = "no"
then
	AC_SEARCH_LIBS([strcasecmp], [])
	if test "$ac_cv_search_strcasecmp" = "no"
	then
		AC_MSG_ERROR([could find neither stricmp nor strcasecmp!])
	else
		AC_DEFINE([stricmp],[strcasecmp],[stricmp function to use])
	fi
fi
CLIENT_LIBS="$CLIENT_LIBS $LIBS"
SERVER_LIBS="$SERVER_LIBS $LIBS"
GAME_LIBS="$GAME_LIBS $LIBS"
TOOLS_LIBS="$TOOLS_LIBS $LIBS"

#---------------------------------------------------------------------------------------------
# Check for zlib
#---------------------------------------------------------------------------------------------

LIBS=
AC_CHECK_HEADERS([zlib.h])
if test "$ac_cv_header_zlib_h" != "yes"
then
	AC_MSG_ERROR([Could not find zlib.h!])
fi
AC_CHECK_LIB(z, compress, [], AC_MSG_ERROR([You must have libz!!!]))
CLIENT_LIBS="$CLIENT_LIBS $LIBS"
SERVER_LIBS="$SERVER_LIBS $LIBS"
TOOLS_LIBS="$TOOLS_LIBS $LIBS"

#---------------------------------------------------------------------------------------------
# Check for curl
#---------------------------------------------------------------------------------------------
LIBS=
AC_CHECK_HEADER(curl/curl.h, LIBS=`$with_curl_config --libs`, AC_MSG_ERROR([You must have libcurl!!!]))
CLIENT_LIBS="$CLIENT_LIBS $LIBS"
SERVER_LIBS="$SERVER_LIBS $LIBS"

#---------------------------------------------------------------------------------------------
# Check JPEG
#---------------------------------------------------------------------------------------------
AC_CHECK_HEADERS([jpeglib.h])
if test "$ac_cv_header_jpeglib_h" = "no"
then
	AC_MSG_ERROR([You must have the JPEG development headers])
fi
LIBS=
AC_CHECK_LIB(jpeg, jpeg_CreateDecompress, [], AC_MSG_ERROR([You must have libjpeg!!!]))
CLIENT_LIBS="$CLIENT_LIBS $LIBS"
TOOLS_LIBS="$TOOLS_LIBS $LIBS"

#---------------------------------------------------------------------------------------------
# Check PNG
#---------------------------------------------------------------------------------------------
PKG_CHECK_MODULES(libpng, libpng, HAVE_libpng="yes", HAVE_libpng="no")
if test "x$HAVE_libpng" = "xno" ; then
	LIBS=
	AC_CHECK_HEADERS([png.h])
	if test "$ac_cv_header_png_h" = "no"
	then
		AC_MSG_ERROR([You must have the PNG development headers])
	fi
	AC_CHECK_LIB(png, png_create_info_struct, [], AC_MSG_ERROR([You must have libpng!!!]))
	libpng_LIBS=$LIBS
fi
CLIENT_LIBS="$CLIENT_LIBS $libpng_LIBS"
CLIENT_CFLAGS="$CLIENT_CFLAGS $libpng_CFLAGS"
TOOLS_LIBS="$TOOLS_LIBS $libpng_LIBS"
TOOLS_CFLAGS="$TOOLS_CFLAGS $libpng_CFLAGS"

#---------------------------------------------------------------------------------------------
# Check SDL
#---------------------------------------------------------------------------------------------
if test $TARGET_OS != "darwin" 
then
	PKG_CHECK_MODULES(SDL, sdl, HAVE_SDL="yes", HAVE_SDL="no")
	if test "x$HAVE_SDL" = "xno" ; then
		sdl_config=$with_sdl_config
		AC_CHECK_PROG(HAVE_SDL_CONFIG,$sdl_config,yes)
		if test "$ac_cv_prog_HAVE_SDL_CONFIG" = "yes" || test -x $sdl_config
		then
			SDL_CFLAGS=`$sdl_config --cflags`
			SDL_LIBS=`$sdl_config --libs`
		else
			AC_MSG_WARN([cannot find $sdl_config might fail to find SDL!])
		fi
	fi
else
	SDL_CFLAGS="$SDL_CFLAGS -I/opt/local/Library/Frameworks/SDL.framework/Headers"
	SDL_LIBS="$SDL_LIBS -framework SDL"
	HAVE_SDL=yes
fi


#---------------------------------------------------------------------------------------------
# Check SDL_image
#---------------------------------------------------------------------------------------------
if test $TARGET_OS != "darwin" 
then
	PKG_CHECK_MODULES(SDL_image, SDL_image, HAVE_SDL_image="yes", HAVE_SDL_image="no")
	if test "x$HAVE_SDL_image" = "xno" ; then
		LIBS=
		AC_SEARCH_LIBS([IMG_LoadTyped_RW],[SDL_image])
		if test "$ac_cv_search_IMG_LoadTyped_RW" = "no"
		then
			AC_MSG_ERROR([You must have the SDL_image development libraries])
		fi
		SDL_image_LIBS=$LIBS
	fi
else
	SDL_image_CFLAGS="$SDL_image_CFLAGS -I/opt/local/Library/Frameworks/SDL_image.framework/Headers"
	SDL_image_LIBS="$SDL_image_LIBS -framework SDL_image"
	HAVE_SDL_image=yes
fi

AC_MSG_NOTICE([SDL cflags: $SDL_CFLAGS])
AC_MSG_NOTICE([SDL ldflags: $SDL_LIBS])
AC_SUBST(SDL_CFLAGS)
AC_SUBST(SDL_LIBS)

AC_MSG_NOTICE([SDL_image cflags: $SDL_image_CFLAGS])
AC_MSG_NOTICE([SDL_image ldflags: $SDL_image_LIBS])
AC_SUBST([SDL_IMAGE_CFLAGS], "$SDL_image_CFLAGS")
AC_SUBST([SDL_IMAGE_LIBS], "$SDL_image_LIBS")

CLIENT_LIBS="$CLIENT_LIBS $SDL_LIBS $SDL_image_LIBS"
CLIENT_CFLAGS="$CLIENT_CFLAGS $SDL_CFLAGS $SDL_image_CFLAGS"
TOOLS_LIBS="$TOOLS_LIBS $SDL_LIBS $SDL_image_LIBS"
TOOLS_CFLAGS="$TOOLS_CFLAGS $SDL_CFLAGS $SDL_image_CFLAGS"

if test "$enable_client" = "yes"
then
	#---------------------------------------------------------------------------------------------
	# Check opengl
	#---------------------------------------------------------------------------------------------
	if test $TARGET_OS = "darwin" 
	then
		CLIENT_CFLAGS="$CLIENT_CFLAGS"
		CLIENT_LIBS="$CLIENT_LIBS -framework OpenGL"
	fi

	#---------------------------------------------------------------------------------------------
	# Check SDL_mixer and SDL_ttf
	#---------------------------------------------------------------------------------------------
	if test $TARGET_OS = "darwin" 
	then
		SDL_mixer_CFLAGS="$SDL_mixer_CFLAGS -I/opt/local/Library/Frameworks/SDL_mixer.framework/Headers"
		SDL_mixer_LIBS="$SDL_mixer_LIBS -framework SDL_mixer"
		SDL_ttf_CFLAGS="$SDL_ttf_CFLAGS -I/opt/local/Library/Frameworks/SDL_ttf.framework/Headers"
		SDL_ttf_LIBS="$SDL_ttf_LIBS -framework SDL_ttf"
	else
		OLD_CFLAGS=$CPPFLAGS
		OLD_LDFLAGS=$LDFLAGS
		CPPFLAGS=$SDL_CFLAGS
		LDFLAGS=$SDL_LIBS

		PKG_CHECK_MODULES(SDL_mixer, SDL_mixer, HAVE_SDL_mixer="yes", HAVE_SDL_mixer="no")
		if test "x$HAVE_SDL_mixer" = "xno" ; then
			LIBS=
			AC_SEARCH_LIBS([Mix_OpenAudio],[SDL_mixer])
			if test "$ac_cv_search_Mix_OpenAudio" = "no"
			then
				AC_MSG_ERROR([You must have the SDL_mixer development libraries])
			fi
			SDL_mixer_LIBS=$LIBS
		fi

		LIBS=
		AC_SEARCH_LIBS([TTF_Init],[SDL_ttf],[],[],[-lfreetype -lz])
		if test "$ac_cv_search_TTF_Init" = "no"
		then
			AC_MSG_ERROR([You must have the SDL_ttf development libraries])
		fi
		SDL_ttf_LIBS=$LIBS

		CPPFLAGS=$OLD_CFLAGS
		LDFLAGS=$OLD_LDFLAGS
	fi

	AC_MSG_NOTICE([SDL_mixer cflags: $SDL_mixer_CFLAGS])
	AC_MSG_NOTICE([SDL_mixer ldflags: $SDL_mixer_LIBS])
	AC_SUBST([SDL_MIXER_CFLAGS], "$SDL_mixer_CFLAGS")
	AC_SUBST([SDL_MIXER_LIBS], "$SDL_mixer_LIBS")
	
	AC_MSG_NOTICE([SDL_ttf cflags: $SDL_ttf_CFLAGS])
	AC_MSG_NOTICE([SDL_ttf ldflags: $SDL_ttf_LIBS])
	AC_SUBST([SDL_TTF_CFLAGS], "$SDL_ttf_CFLAGS")
	AC_SUBST([SDL_TTF_LIBS], "$SDL_ttf_LIBS")

	CLIENT_LIBS="$CLIENT_LIBS $SDL_mixer_LIBS $SDL_ttf_LIBS"
	CLIENT_CFLAGS="$CLIENT_CFLAGS $SDL_mixer_CFLAGS $SDL_ttf_CFLAGS"

	#---------------------------------------------------------------------------------------------
	# Check OGG
	#---------------------------------------------------------------------------------------------
	AC_CHECK_HEADERS([ogg/ogg.h])
	if test "$ac_cv_header_ogg_ogg_h" = "no"
	then
		AC_MSG_ERROR([You must have the OGG development headers])
	fi
	LIBS=
	AC_CHECK_LIB(ogg, ogg_sync_init, [], AC_MSG_ERROR([You must have libogg!!!]))
	CLIENT_LIBS="$CLIENT_LIBS $LIBS"

	#---------------------------------------------------------------------------------------------
	# Check Vorbis
	#---------------------------------------------------------------------------------------------
	PKG_CHECK_MODULES(VORBIS, vorbis, HAVE_VORBIS="yes", HAVE_VORBIS="no")
	if test "x$HAVE_VORBIS" = "xno" ; then
		AC_CHECK_HEADERS([vorbis/codec.h])
		if test "$ac_cv_header_vorbis_codec_h" = "no"
		then
			AC_MSG_ERROR([You must have the Vorbis development headers])
		fi
		LIBS=
		AC_CHECK_LIB(vorbis, vorbis_synthesis_init, [], AC_MSG_ERROR([You must have libvorbis!!!]), [-lm -logg])
		CLIENT_LIBS="$LIBS $CLIENT_LIBS"
	else
		AC_SUBST([HAVE_VORBIS_CODEC_H],1)
		AC_DEFINE([HAVE_VORBIS_CODEC_H],[1],[Have vorbis library])
		CLIENT_CFLAGS="$CLIENT_CFLAGS $VORBIS_CFLAGS"
		CLIENT_LIBS="$VORBIS_LIBS $CLIENT_LIBS"
	fi

	#---------------------------------------------------------------------------------------------
	# Check XVID
	#---------------------------------------------------------------------------------------------
	AC_CHECK_HEADERS([xvid.h])
	if test "$ac_cv_header_xvid_h" = "no"
	then
		AC_MSG_WARN([Could not find the Xvid development headers])
	else
		LIBS=
		AC_CHECK_LIB(xvidcore, xvid_decore, [], AC_MSG_ERROR([You must have libxvidcore!!!]))
		CLIENT_LIBS="$CLIENT_LIBS $LIBS"
	fi

	#---------------------------------------------------------------------------------------------
	# Check Theora
	#---------------------------------------------------------------------------------------------
	PKG_CHECK_MODULES(THEORA, theora, HAVE_THEORA="yes", HAVE_THEORA="no")
	if test "x$HAVE_THEORA" = "xno" ; then
		AC_CHECK_HEADERS([theora/theora.h])
		if test "$ac_cv_header_theora_theora_h" = "no"
		then
			if test "$ac_cv_header_xvid_h" = "no"
			then
				AC_MSG_ERROR([Could not find the Theora development headers nor the Xvid developement headers])
			else
				AC_MSG_WARN([Could not find the Theora development headers])
			fi
		else
			LIBS=
			AC_CHECK_LIB(theora, theora_decode_packetin, [], AC_MSG_ERROR([You must have libtheora!!!]))
			CLIENT_LIBS="$CLIENT_LIBS $LIBS"
		fi
	else
		AC_SUBST([HAVE_THEORA_THEORA_H],1)
		AC_DEFINE([HAVE_THEORA_THEORA_H],[1],[Have theora library])
		CLIENT_CFLAGS="$CLIENT_CFLAGS $THEORA_CFLAGS"
		CLIENT_LIBS="$CLIENT_LIBS $THEORA_LIBS"
	fi


	#---------------------------------------------------------------------------------------------
	# Check for gettext
	#---------------------------------------------------------------------------------------------
	AC_CHECK_HEADERS([libintl.h])
	if test "$ac_cv_header_libintl_h" != "yes"
	then
		AC_MSG_ERROR([Could not find libintl.h!])
	fi

	AC_SEARCH_LIBS([gettext],[intl],[],[],[-liconv])
	if test "$ac_cv_search_gettext" = "no"
	then
		AC_MSG_ERROR([Could not find internationalization library with gettext in it])
	fi
	#AC_CHECK_LIB(intl, gettext, [], AC_MSG_ERROR([You must have libintl!!!]))

	#---------------------------------------------------------------------------------------------
	# Check for opengl headers
	#---------------------------------------------------------------------------------------------
	AC_CHECK_HEADERS([GL/gl.h GL/glu.h])
	if test "$ac_cv_header_GL_gl_h" != "yes" || test "$ac_cv_header_GL_glu_h" != "yes"
	then
		AC_CHECK_HEADERS([OpenGL/gl.h OpenGL/glu.h])
		if test "$ac_cv_header_OpenGL_gl_h" != "yes" || test "$ac_cv_header_OpenGL_glu_h" != "yes"
		then
			AC_MSG_ERROR([Could not find OpenGL development headers!])
		else
			AC_DEFINE([GL_H_WITHOUT_GL_DIR],[1],[OpenGL header path])
		fi
	fi

	#---------------------------------------------------------------------------------------------
	# Set the client clags and libs - the rest are shared libs
	#---------------------------------------------------------------------------------------------
	CLIENT_LIBS="$CLIENT_LIBS $LIBS"
# end of client only checks
fi

#---------------------------------------------------------------------------------------------
# Check for msys
#---------------------------------------------------------------------------------------------

LIBS=
if test "$with_msys" = "yes"
then
	AC_SUBST([MSYS],1)
	AC_DEFINE([MSYS],[1],[Use msys])
	AC_MSG_NOTICE([With msys])
fi
if test "$with_msys" = "check"
then
	case "$MACHTYPE" in
		*msys)
			AC_SUBST([MSYS],1)
			AC_DEFINE([MSYS],[1],[Use msys])
			AC_MSG_NOTICE([With msys])
			;;
		*)
			AC_MSG_NOTICE([Without msys])
			;;
	esac
fi

#---------------------------------------------------------------------------------------------
# Check for radiant
#---------------------------------------------------------------------------------------------

if test "x$enable_uforadiant" = "xyes"
then
	LIBS=
	CPPFLAGS=
	RADIANT_LIBS=

	#---------------------------------------------------------------------------------------------
	# Check for gtk2, gtkglext, glib2 and libxml2
	#---------------------------------------------------------------------------------------------

	AC_MSG_NOTICE([UFORadiant dependencies: gtk2, gtkglext, gtksourceview-2.0, glib2 and libxml2])
	PKG_CHECK_MODULES([RADIANT], [gtk+-2.0 gtkglext-1.0 glib-2.0 libxml-2.0 gtksourceview-2.0])
	RADIANT_CFLAGS="$RADIANT_CFLAGS $CPPFLAGS"

	#---------------------------------------------------------------------------------------------
	# Check for openal
	#---------------------------------------------------------------------------------------------

	if test $TARGET_OS = "darwin"
	then
		OPENAL_CFLAGS="$OPENAL_CFLAGS"
		OPENAL_LIBS="$OPENAL_LIBS -framework OpenAL"
	else
		PKG_CHECK_MODULES(OPENAL, openal, HAVE_OPENAL="yes", HAVE_OPENAL="no")
		if test "x$HAVE_OPENAL" = "xno" ; then
			AC_CHECK_HEADERS([AL/al.h])
			if test "$ac_cv_header_AL_al_h" = "no"
			then
				AC_MSG_ERROR([You must have openal installed - or deactivate the uforadiant build])
			fi
			LIBS=
			AC_SEARCH_LIBS([alEnable], [openal32 openal], , AC_MSG_ERROR([You must have libopenal!!!]))
			OPENAL_LIBS="$LIBS $OPENAL_LIBS"
		fi
	fi
else
	AC_MSG_NOTICE([Without UFORadiant])
fi

#---------------------------------------------------------------------------------------------
# Check for tests
#---------------------------------------------------------------------------------------------

if test "$enable_tests" = "check" || test "$enable_tests" = "yes"
then
	AC_SEARCH_LIBS([CU_initialize_registry],[cunit])
	if test "$ac_cv_search_CU_initialize_registry" = "no"
	then
		if test "$enable_tests" = "yes"
		then
			AC_MSG_ERROR([--enable-tests specified but could not find cunit lib])
		else
			AC_SUBST([BUILD_TESTS],0)
			AC_MSG_NOTICE([Without tests - cunit library not found])
		fi
	else
		AC_SUBST([BUILD_TESTS],1)
		AC_DEFINE([BUILD_TESTS],[1],[Have cunit library])
		AC_MSG_NOTICE([With tests])
	fi
else
	AC_MSG_NOTICE([Without tests])
	AC_SUBST([BUILD_TESTS],0)
fi

#---------------------------------------------------------------------------------------------
# Wipe the LIBS string
#---------------------------------------------------------------------------------------------

LIBS=

#---------------------------------------------------------------------------------------------
# Check for client
#---------------------------------------------------------------------------------------------

AC_SUBST([BUILD_CLIENT],0)
if test "$enable_client" = "yes"
then
	AC_MSG_NOTICE([Enabling client])
	AC_SUBST([BUILD_CLIENT],1)
fi

#---------------------------------------------------------------------------------------------
# Check for dedicated
#---------------------------------------------------------------------------------------------

AC_SUBST([BUILD_DEDICATED],0)
if test "$enable_dedicated" = "yes"
then
	AC_MSG_NOTICE([Enabling dedicated server])
	AC_SUBST([BUILD_DEDICATED],1)
fi

#---------------------------------------------------------------------------------------------
# Check for ufomodel
#---------------------------------------------------------------------------------------------

AC_SUBST([BUILD_UFOMODEL],0)
if test "$enable_ufomodel" = "yes"
then
	AC_MSG_NOTICE([Enabling ufomodel])
	AC_SUBST([BUILD_UFOMODEL],1)
fi

#---------------------------------------------------------------------------------------------
# Check for ufo2map
#---------------------------------------------------------------------------------------------

AC_SUBST([BUILD_UFO2MAP],0)
if test "$enable_ufo2map" = "yes"
then
	AC_MSG_NOTICE([Enabling ufo2map])
	AC_SUBST([BUILD_UFO2MAP],1)
fi

#---------------------------------------------------------------------------------------------
# Check for uforadiant
#---------------------------------------------------------------------------------------------

if test "$enable_uforadiant" = "yes"
then
	AC_MSG_NOTICE([Enabling UFORadiant])
	AC_SUBST([BUILD_UFORADIANT],1)
fi

#---------------------------------------------------------------------------------------------
# Check for debug
#---------------------------------------------------------------------------------------------

if test "$enable_release" = "no"
then
	AC_MSG_NOTICE([Enabling debug build])
	AC_SUBST([BUILD_DEBUG],1)
else
	AC_MSG_NOTICE([Enabling release build])
	AC_SUBST([BUILD_DEBUG],0)
fi

#---------------------------------------------------------------------------------------------
# Check for signal handling
#---------------------------------------------------------------------------------------------

if test $TARGET_OS != "darwin" 
then
	if test "$enable_signals" = "no"
	then
		AC_MSG_NOTICE([Disable signal handling])
		USE_SIGNALS=0
	else
		AC_MSG_NOTICE([Enabling signal handling])
		USE_SIGNALS=1
	fi
else
	AC_MSG_NOTICE([Always disable signal handling for $TARGET_OS])
	USE_SIGNALS=0
fi
AC_SUBST([USE_SIGNALS])

#---------------------------------------------------------------------------------------------
# Check for PARANOID
#---------------------------------------------------------------------------------------------

if test "$enable_paranoid" = "yes"
then
	AC_SUBST([PARANOID],1)
	AC_MSG_NOTICE([Enabling PARANOID])
fi

#---------------------------------------------------------------------------------------------
# Check for PROFILING
#---------------------------------------------------------------------------------------------

if test "$enable_profiling" = "yes"
then
	AC_SUBST([PROFILING],1)
	AC_MSG_NOTICE([Enabling profiling])
else
	AC_SUBST([PROFILING],0)
fi

#---------------------------------------------------------------------------------------------
# Check for HARD_LINKED_GAME
#---------------------------------------------------------------------------------------------

if test "$enable_hardlinkedgame" = "yes"
then
	AC_SUBST([HARD_LINKED_GAME],1)
	AC_MSG_NOTICE([Enabling hard linked game])
else
	AC_SUBST([HARD_LINKED_GAME],0)
fi


#---------------------------------------------------------------------------------------------
# Fill in path variables
#---------------------------------------------------------------------------------------------

test "$prefix" = "NONE" && prefix=/usr/local
test "$exec_prefix" = "NONE" && exec_prefix=$prefix
test "$localedir" = "NONE" && localedir=$prefix/share/locale
datarootdir=$prefix/share
test "$datadir" = "NONE" && datadir=$datarootdir

eval PKGLIBDIR="$libdir/$PACKAGE_NAME"
AC_DEFINE_UNQUOTED(PKGLIBDIR, "$PKGLIBDIR", [Define to path containing the shared modules.])

eval PKGDATADIR="$datadir/$PACKAGE_NAME"
AC_DEFINE_UNQUOTED(PKGDATADIR, "$PKGDATADIR", [Define to path containing the game data.])

eval BINDIR="$bindir"
eval LOCALEDIR="$localedir"
eval PKGLIBDIR="$PKGLIBDIR"
eval PKGDATADIR="$PKGDATADIR"

#---------------------------------------------------------------------------------------------

AC_SUBST([EXE_EXT],"$ac_exe_ext")

AC_SUBST([CLIENT_LIBS],"$CLIENT_LIBS")
AC_SUBST([CLIENT_CFLAGS],"$CLIENT_CFLAGS")
AC_SUBST([SERVER_LIBS],"$SERVER_LIBS")
AC_SUBST([SERVER_CFLAGS],"$SERVER_CFLAGS")
AC_SUBST([GAME_LIBS],"$GAME_LIBS")
AC_SUBST([GAME_CFLAGS],"$GAME_CFLAGS")
AC_SUBST([TOOLS_LIBS],"$TOOLS_LIBS")
AC_SUBST([TOOLS_CFLAGS],"$TOOLS_CFLAGS")
AC_SUBST([RADIANT_LIBS],"$RADIANT_LIBS")
AC_SUBST([RADIANT_CFLAGS], "$RADIANT_CFLAGS")
AC_SUBST([OPENAL_LIBS], "$OPENAL_LIBS")
AC_SUBST([OPENAL_CFLAGS], "$OPENAL_CFLAGS")

AC_SUBST(CFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(PKGDATADIR)
AC_SUBST(PKGLIBDIR)
AC_SUBST(BINDIR)
AC_SUBST(LOCALEDIR)
AC_SUBST(PACKAGE_NAME)
AC_SUBST(PACKAGE_VERSION)

AC_CONFIG_HEADERS([config.h:config.h.in])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

#---------------------------------------------------------------------------------------------

AC_MSG_RESULT([
${PACKAGE_NAME} ${PACKAGE_VERSION}: Automatic configuration OK.

  Configuration summary:
    Platform: .......... ${host}
    Compiler: .......... ${CC}

  Installation directories:

    Program: ........... ${BINDIR}
    Locales: ........... ${LOCALEDIR}
    Libraries: ......... ${PKGLIBDIR}
    Data: .............. ${PKGDATADIR}

  Compiler flags:

    CC: ................ ${CFLAGS}
    Client: ............ ${CLIENT_CFLAGS}
    Server: ............ ${SERVER_CFLAGS}
    Tools: ............. ${TOOLS_CFLAGS}
    UFORadiant: ........ ${RADIANT_CFLAGS}

  Linker flags:

    LD: ................ ${LDFLAGS}
    Client: ............ ${CLIENT_LIBS}
    Server: ............ ${SERVER_LIBS}
    Tools: ............. ${TOOLS_LIBS}
    UFORadiant: ........ ${RADIANT_LIBS}

Type 'make' to build $PROGRAM.  Type 'make install' to install it.
])

