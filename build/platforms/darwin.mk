SO_EXT                    = dylib
SO_CFLAGS                 = -fPIC -fno-common
SO_LDFLAGS                = -dynamiclib
SO_LIBS                  := -ldl

CFLAGS                   += -D_BSD_SOURCE -D_XOPEN_SOURCE
CXXFLAGS                 += -std=c++11
LDFLAGS                  += -framework IOKit -framework Foundation -framework Cocoa -headerpad_max_install_names

### most mac users will have their additional libs and headers under /opt/local
### if they are using macports, or in /sw if they are using fink,
### check for that, and if present, add to CFLAGS/LDFLAGS (really convenient!)
ifneq ($(wildcard /opt/local),)
	LIB_BASE_DIR     ?= /opt/local
	FRAMEWORK_DIR    ?= $(LIB_BASE_DIR)/Library/Frameworks
else
ifneq ($(wildcard /sw),)
	LIB_BASE_DIR     ?= /sw
	FRAMEWORK_DIR    ?=
endif
endif

ifneq ($(LIB_BASE_DIR),)
	CFLAGS           += -I$(LIB_BASE_DIR)/include
	LDFLAGS          += -L$(LIB_BASE_DIR)/lib
endif

ifneq ($(FRAMEWORK_DIR),)
	CFLAGS           += -F$(FRAMEWORK_DIR)
	LDFLAGS          += -F$(FRAMEWORK_DIR)

ifndef HAVE_SDL2_SDL_H
	SDL_CFLAGS       ?= -I$(FRAMEWORK_DIR)/SDL.framework/Headers
	SDL_LIBS         ?= -framework SDL -lSDLmain
	SDL_MIXER_CFLAGS ?= -I$(FRAMEWORK_DIR)/SDL_mixer.framework/Headers
	SDL_MIXER_LIBS   ?= -framework SDL_mixer
	SDL_TTF_CFLAGS   ?= -I$(FRAMEWORK_DIR)/SDL_ttf.framework/Headers
	SDL_TTF_LIBS     ?= -framework SDL_ttf
endif
endif

ifdef UNIVERSAL
	CFLAGS           += -arch i386 -arch ppc
	LDFLAGS          += -arch i386 -arch ppc
endif

OPENGL_CFLAGS            ?=
OPENGL_LIBS              ?= -framework OpenGL
OPENAL_CFLAGS            ?=
OPENAL_LIBS              ?= -framework OpenAL
MUMBLE_LIBS              ?=
INTL_LIBS                ?= -lintl
