# Some variables needed by the modules
SRCDIR=src

GENERIC_FLAGS+=-Wall
CPPFLAGS+=-DHAVE_CONFIG_H $(GENERIC_FLAGS) -DUSE_SIGNALS=$(USE_SIGNALS)
CFLAGS+=-DHAVE_CONFIG_H -DUSE_SIGNALS=$(USE_SIGNALS) $(GENERIC_FLAGS) -pipe -Winline \
	-Wcast-qual -Wcast-align -Wdeclaration-after-statement -Wmissing-prototypes \
	-Wmissing-declarations -std=c99 -Wpointer-arith
#	-Wunreachable-code -Wlogical-op -Wc++-compat
#	-Wunsafe-loop-optimizations \
#	-Wimplicit-int -Wpacked -Wparentheses -Wpadded \
#	-Wunused -Wunused-function -Wunused-label -Wunused-value -Wunused-variable \
#	-fstack-protector-all -Wstack-protector \
#	-Wfloat-equal -Wconversion -Wbad-function-cast \
#	-Wshadow -pedantic -combine -fwhole-program -fexcess-precision=fast

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

ifeq ($(HARD_LINKED_GAME),1)
	CFLAGS+=-DHARD_LINKED_GAME
endif

ifeq ($(PARANOID),1)
	CFLAGS+=-DPARANOID
	CPPFLAGS+=-DPARANOID
endif
