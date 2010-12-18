ifeq ($(STATIC),1)
PKG_CONFIG_LIBS_FLAGS := --static
endif

#TODO the manually added linker flag is more a hack than a solution
define PKG_LIBS
$(shell $(PKG_CONFIG) $(PKG_CONFIG_LIBS_FLAGS) --libs $(1) 2> /dev/null || ( if [ -z "$(2)" ]; then echo "-l$(1)"; else echo "-l$(2)"; fi ))
endef

define PKG_CFLAGS
$(shell $(PKG_CONFIG) --cflags $(1) 2> /dev/null)
endef


CFLAGS += -DHAVE_CONFIG_H
CFLAGS += -ggdb
#CFLAGS += -pipe
CFLAGS += -Winline
CFLAGS += -Wcast-qual
CFLAGS += -Wcast-align
CFLAGS += -Wmissing-declarations
CFLAGS += -Wpointer-arith
CFLAGS += -Wno-long-long
CFLAGS += -pedantic
CFLAGS += -Wall
CFLAGS += -Wextra
CFLAGS += -Wno-sign-compare
CFLAGS += -Wno-unused-parameter
CFLAGS += -Wreturn-type
CFLAGS += -Wwrite-strings

ifeq ($(PROFILING),1)
  CFLAGS  += -pg
  CCFLAGS  += -pg
  LDFLAGS += -pg
endif

ifeq ($(DEBUG),1)
  CFLAGS  += -DDEBUG
  CCFLAGS  += -DDEBUG
else
  CFLAGS  += -DNDEBUG
  CCFLAGS  += -DNDEBUG
endif

CCFLAGS += $(CFLAGS)
CCFLAGS += -std=c99
ifeq ($(W2K),1) # Wspiapi.h make use of inline function, but std=c99 will prevent this. -fgnu89-inline tells GCC to use the traditional GNU semantics for inline functions when in C99 mode
  CCFLAGS += -fgnu89-inline
endif

#CCFLAGS += -Werror-implicit-function-declaration
#CCFLAGS += -Wimplicit-int
#CCFLAGS += -Wmissing-prototypes
#CCFLAGS += -Wdeclaration-after-statement
#CCFLAGS += -Wc++-compat

CXXFLAGS += $(CFLAGS)
CXXFLAGS += -Wnon-virtual-dtor
