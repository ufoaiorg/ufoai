#ifndef CXX_H
#define CXX_H

#define GCC_ATLEAST(major, minor) (defined __GNUC__ && (__GNUC__ > (major) || (__GNUC__ == (major) && __GNUC_MINOR__ >= (minor))))

#define CXX11(gcc_major, gcc_minor, msc_ver) ( \
	__cplusplus >= 201103L || \
	(defined __GXX_EXPERIMENTAL_CXX0X__ && GCC_ATLEAST((gcc_major), (gcc_minor))) || \
	(defined _MSC_VER && (msc_ver) != 0 && _MSC_VER >= (msc_ver)) \
)

#if CXX11(4, 4, 0)
#	define DEFAULT = default
#	define DELETED = delete
#else
#	define DEFAULT {}
#	define DELETED
#endif

#if CXX11(4, 7, 1400)
#	define OVERRIDE override
#else
#	define OVERRIDE
#endif

#endif
