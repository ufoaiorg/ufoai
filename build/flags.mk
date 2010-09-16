#TODO the manually added linker flag is more a hack than a solution
define PKG_LIBS
	`$(PKG_CONFIG) --libs $(1) 2> /dev/null || echo "-l$(1)"`
endef

define PKG_CFLAGS
	`$(PKG_CONFIG) --cflags $(1) 2> /dev/null`
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
CCFLAGS += -std=c99
#CCFLAGS += -Werror-implicit-function-declaration
#CCFLAGS += -Wimplicit-int
#CCFLAGS += -Wmissing-prototypes
#CCFLAGS += -Wdeclaration-after-statement
#CCFLAGS += -Wc++-compat

CXXFLAGS += $(CFLAGS)
