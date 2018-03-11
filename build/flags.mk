ifeq ($(STATIC),1)
PKG_CONFIG_LIBS_FLAGS := --static
CONFIG_LIBS_FLAGS = --static-libs
else
CONFIG_LIBS_FLAGS = --libs
endif

#TODO the manually added linker flag is more a hack than a solution
define PKG_LIBS
$(shell set -- $(shell echo $(1) | cut -d' ' -f 1-); for i in "$${@}"; do $(PKG_CONFIG) $(PKG_CONFIG_LIBS_FLAGS) --libs $${i} 2> /dev/null && break; done \
	|| ( if [ -z "$(2)" ]; then echo "-l$(shell echo $(1) | cut -d' ' -f1)"; else echo "-l$(2)"; fi ))
endef

define PKG_CFLAGS
$(shell set -- $(shell echo $(1) | cut -d' ' -f 1-); for i in "$${@}"; do $(PKG_CONFIG) --cflags $${i} 2> /dev/null && break; done)
endef


CFLAGS += -DHAVE_CONFIG_H
CFLAGS += -g
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
CFLAGS += -Wno-variadic-macros
CFLAGS += -Wno-format-zero-length
CFLAGS += -Wno-expansion-to-defined

# clang stuff
ifneq (,$(findstring clang,$(CXX)))
  CFLAGS += -Wno-extended-offsetof
  CFLAGS += -Wno-c++11-extensions
  CFLAGS += -Wno-cast-align
endif

ifeq ($(PROFILING),1)
  CFLAGS  += -pg
  LDFLAGS += -pg
endif

ifeq ($(DEBUG),1)
  CFLAGS  += -DDEBUG
else
  CFLAGS  += -DNDEBUG
endif

CCFLAGS += $(CFLAGS)
CCFLAGS += -std=c99

#CCFLAGS += -Werror-implicit-function-declaration
#CCFLAGS += -Wimplicit-int
#CCFLAGS += -Wmissing-prototypes
#CCFLAGS += -Wdeclaration-after-statement
#CCFLAGS += -Wc++-compat

CXXFLAGS += -Wnon-virtual-dtor
