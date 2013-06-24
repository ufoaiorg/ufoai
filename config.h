#pragma once
#include <SDL_platform.h>

#ifdef __LINUX__
#include "linux-config.h"
#endif

#ifdef __MACOSX__
#include "darwin-config.h"
#endif

#ifdef __WIN64__
#include "mingw64_64.h"
#elif defined __WIN32__
#include "mingw64-config.h"
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
