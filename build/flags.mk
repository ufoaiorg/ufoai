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
  LDFLAGS += -pg
endif

CCFLAGS += $(CFLAGS)

ifeq ($(W2K),1)
  CCFLAGS += -std=gnu89
else
  CCFLAGS += -std=c99
endif

#CCFLAGS += -Werror-implicit-function-declaration
#CCFLAGS += -Wimplicit-int
#CCFLAGS += -Wmissing-prototypes
#CCFLAGS += -Wdeclaration-after-statement
#CCFLAGS += -Wc++-compat

CXXFLAGS += $(CFLAGS)
