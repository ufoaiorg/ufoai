#pragma once
#include <SDL_platform.h>
#include <cstdlib>

#if !defined(__LINUX__) && !defined(__MACOSX__) && !defined(__WIN64__) \
	&& !defined(__WIN32__) && !defined(__ANDROID__) && !defined(EMSCRIPTEN) \
	&& !defined(__IPHONEOS__) && !defined(__FREEBSD__) && !defined(__OPENBSD__) \
	&& !defined(PANDORA)
#error The target platform was not found.  Please add to config.h.
#endif

#if defined(__LINUX__)
#  include "linux-config.h"
#elif defined(__MACOSX__)
#  include "darwin-config.h"
#elif defined(__WIN64__)
#  include "mingw64_64-config.h"
#elif defined(__WIN32__)
#  ifdef __MINGW64_VERSION_MAJOR
#    include "mingw64-config.h"
#  else
#    include "mingw32-config.h"
#  endif
#elif defined(EMSCRIPTEN)
#  include "html5-config.h"
#elif defined(__ANDROID__)
#  include "android-config.h"
#elif defined(__IPHONEOS__)
#  include "ios-config.h"
#elif defined(__FREEBSD__)
#  include "freebsd-config.h"
#elif defined(__OPENBSD__)
#  include "openbsd-config.h"
#elif defined(PANDORA)
#  include "openpandora-config.h"
#endif
