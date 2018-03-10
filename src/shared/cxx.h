#pragma once

#define GCC_ATLEAST(major, minor) (defined __GNUC__ && (__GNUC__ > (major) || (__GNUC__ == (major) && __GNUC_MINOR__ >= (minor))))

#ifndef __has_extension // clang feature detection
#	ifdef __has_feature
#		define __has_extension __has_feature
#	else
#		define __has_extension(x) 0
#	endif
#endif

#define CXX11(gcc_major, gcc_minor, msc_ver, clang_feature) ( \
	__cplusplus >= 201103L || \
	(defined __GXX_EXPERIMENTAL_CXX0X__ && GCC_ATLEAST((gcc_major), (gcc_minor))) || \
	(defined _MSC_VER && (msc_ver) != 0 && _MSC_VER >= (msc_ver)) || \
	__has_extension(clang_feature) \
)

#if CXX11(4, 4, 0, cxx_defaulted_functions)
#	define DEFAULT = default
#else
#	define DEFAULT {}
#endif

#if CXX11(4, 4, 0, cxx_deleted_functions)
#	define DELETED = delete
#else
#	define DELETED
#endif

#if not CXX11(4, 7, 1400, cxx_override_control)
#	define override
#endif

#ifndef __GNUC__
# define __attribute__(x)
#endif

#ifdef _MSC_VER
# define snprintf _snprintf
# define __func__   __FUNCTION__
# define __PRETTY_FUNCTION__   __FUNCSIG__
#else
# if __cplusplus < 201103L
#  ifndef nullptr
/* not typesafe as the real nullptr from c++11 */
#   define nullptr 0
#  endif
# endif
#endif

#if __cplusplus >= 201103L
# define register
#endif
