SO_EXT                    = so
SO_LDFLAGS                = -shared
SO_CFLAGS                 = -fpic
SO_LIBS                  := -ldl

CFLAGS                   += -D_GNU_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE -DNO_I18N -DNO_HTTP
CFLAGS                   += -Wno-c++11-extensions -Wno-shift-op-parentheses
CFLAGS                   += --jcache
LDFLAGS                  += --jcache
EMSCRIPTEN               := 1
EXE_EXT                  ?= .html

#ufo_LDFLAGS      += -s DEAD_FUNCTIONS="['_glPushClientAttrib','_glPopClientAttrib','_glPointSize']"
ufo_LDFLAGS      += --shell-file contrib/installer/html5/shell.html
ufo_LDFLAGS      += --memory-init-file 1 -s VERBOSE=1 -s WARN_ON_UNDEFINED_SYMBOLS=1
ufo_LDFLAGS      += -s TOTAL_MEMORY=33554432
ufo_LDFLAGS      += -O2 -s ASM_JS=1 --minify 1
ifeq ($(EXE_EXT),.js)
ufo_LDFLAGS      += $(foreach file,$(shell find base -type f), --embed-file $(file))
LDFLAGS          += -g
else
# browser environments can use preload-file - but this wouldn't work with e.g. node
ufo_LDFLAGS      += --preload-file base
endif
