#pragma once

#ifndef __GNUC__
# define __attribute__(x)
#endif

#ifdef _MSC_VER
# define __func__   __FUNCTION__
# define __PRETTY_FUNCTION__   __FUNCSIG__
#endif
