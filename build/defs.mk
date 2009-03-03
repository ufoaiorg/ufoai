# Some variables needed by the modules
SRCDIR=src

GENERIC_FLAGS+=-Wall
CPPFLAGS+=-DHAVE_CONFIG_H $(GENERIC_FLAGS)
CFLAGS+=-DHAVE_CONFIG_H $(GENERIC_FLAGS) -pipe -Winline -Wcast-qual -Wcast-align -std=c99\
	-Wdeclaration-after-statement -Wmissing-prototypes -Wmissing-declarations
#	-Wunreachable-code -Wpointer-arith -Wcast-align -Wunsafe-loop-optimizations \
#	-Wimplicit-int -Wpacked -Wparentheses \
#	-Wunused -Wunused-function -Wunused-label -Wunused-value -Wunused-variable -Wvariadic-macros
#	-fstack-protector-all -Wstack-protector -Wpadded
#	-Wfloat-equal -Wconversion -Wunreachable-code -Wbad-function-cast \
#	-Wshadow -Wpointer-arith
#	-Wbad-function-cast -pedantic -std=c99
#-combine -fwhole-program

# Common things
_BUILDDIR=$(strip $(BUILDDIR))
_MODULE=$(strip $(MODULE))
_SRCDIR=$(strip $(SRCDIR))

LIBTOOL_LD=$(LIBTOOL) --silent --mode=link $(CC) -module -rpath / $(LDFLAGS) $(KIBS)
LIBTOOL_CC=$(LIBTOOL) --silent --mode=compile $(CC) -prefer-pic $(CFLAGS)

# Target options

ifeq ($(BUILD_DEBUG),1)
	BUILDDIR=debug-$(TARGET_OS)-$(TARGET_CPU)
	CFLAGS+=-ggdb -O0 -DDEBUG -fno-inline
	CPPFLAGS+=-ggdb -O0 -DDEBUG -fno-inline
# -fvar-tracking
else
	BUILDDIR=release-$(TARGET_OS)-$(TARGET_CPU)
	CFLAGS+=-ggdb -DNDEBUG $(RELEASE_CFLAGS) -O2
	CPPFLAGS+=-ggdb -DNDEBUG -O2
endif

ifeq ($(PROFILING),1)
	CFLAGS+=-pg
	LNKFLAGS+=-pg
endif

LNKFLAGS+=-rdynamic

ifeq ($(PARANOID),1)
	CFLAGS+=-DPARANOID
	CPPFLAGS+=-DPARANOID
endif
