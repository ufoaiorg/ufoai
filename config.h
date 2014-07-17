#pragma once
#include <SDL_platform.h>
#include <stdlib.h>

#ifndef __LINUX__
#ifndef __MACOSX__
#ifndef __WIN64__
#ifndef __WIN32__
#ifndef __ANDROID__
#ifndef EMSCRIPTEN
#ifndef __IPHONEOS__
#ifndef __FREEBSD__
#ifndef __OPENBSD__
#error The target platform was not found.  Please add to config.h.
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif

#ifdef __LINUX__
#include "linux-config.h"
#endif

#ifdef __MACOSX__
#include "darwin-config.h"
#endif

#ifdef __WIN64__
#include "mingw64_64.h"
#elif defined __WIN32__
#ifdef __MINGW64_VERSION_MAJOR
#include "mingw64-config.h"
#else
#include "mingw32-config.h"
#endif
#endif

#ifdef EMSCRIPTEN
#include "html5-config.h"
#endif

#ifdef __ANDROID__
#include "android-config.h"
#endif

#ifdef __IPHONEOS__
#include "ios-config.h"
#endif

#ifdef __FREEBSD__
#include "freebsd-config.h"
#endif

#ifdef __OPENBSD__
#include "openbsd-config.h"
#endif
