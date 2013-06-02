CONFIG ?= Makefile.local
-include $(CONFIG)

HOST_OS     ?= $(shell uname | sed -e s/_.*// | tr '[:upper:]' '[:lower:]')
TARGET_OS   ?= $(HOST_OS)
TARGET_ARCH ?= $(shell uname -m | sed -e s/i.86/i386/)

ifneq ($(findstring $(HOST_OS),sunos darwin),)
  TARGET_ARCH ?= $(shell uname -p | sed -e s/i.86/i386/)
endif

MODE        ?= debug
PREFIX      ?= /usr/local
WINDRES     ?= windres
CC          ?= gcc
CXX         ?= g++
Q           ?= @
BUILDDIR    ?= $(MODE)-$(TARGET_OS)-$(TARGET_ARCH)
SRCDIR      := src
TARGETS_TMP := $(sort $(patsubst build/modules/%.mk,%,$(wildcard build/modules/*.mk)))
TARGETS     := $(filter-out $(foreach TARGET,$(TARGETS_TMP),$(if $($(TARGET)_DISABLE),$(TARGET),)),$(TARGETS_TMP))
DISABLED    := $(filter-out $(TARGETS),$(TARGETS_TMP))

ifeq ($(DISABLE_DEPENDENCY_TRACKING),)
  DEP_FLAGS := -MP -MD -MT $$@
endif
CONFIGURE_PREFIX ?=

INSTALL         ?= install
INSTALL_PROGRAM ?= $(INSTALL) -m 755 -s
INSTALL_SCRIPT  ?= $(INSTALL) -m 755
INSTALL_DIR     ?= $(INSTALL) -d
INSTALL_MAN     ?= $(INSTALL) -m 444
INSTALL_DATA    ?= $(INSTALL) -m 444

.PHONY: all
ifeq ($(TARGET_OS),html5)
ifeq ($(EMSCRIPTEN_CALLED),1)
all: $(TARGETS)
else
all: emscripten
endif
else
all: $(TARGETS)
endif

include build/flags.mk
include build/platforms/$(TARGET_OS).mk
include build/modes/$(MODE).mk
include build/default.mk

CXXFLAGS := $(CFLAGS) $(CXXFLAGS)

ASSEMBLE_OBJECTS = $(patsubst %, $(BUILDDIR)/$(1)/%.o, $(filter %.c %.cpp %.rc, $($(1)_SRCS)))
FILTER_OUT = $(foreach v,$(2),$(if $(findstring $(1),$(v)),,$(v)))

define INCLUDE_RULE
include build/modules/$(1).mk
endef
$(foreach TARGET,$(TARGETS_TMP),$(eval $(call INCLUDE_RULE,$(TARGET))))

.PHONY: clean
clean: $(addprefix clean-,$(TARGETS)) clean-docs

.PHONY: distclean
distclean: clean
	$(Q)rm -f config.h Makefile.local
	$(Q)rm -rf $(BUILDDIR)

.PHONY: install
install: install-pre $(addprefix install-,$(TARGETS))

.PHONY: strip
strip: $(addprefix strip-,$(TARGETS))

config.h: configure
	$(Q)$(CONFIGURE_PREFIX) ./configure $(CONFIGURE_OPTIONS)

define BUILD_RULE
ifndef $(1)_DISABLE
ifndef $(1)_IGNORE

-include $($(1)_OBJS:.o=.d)

# if the target filename differs:
ifneq ($(1),$($(1)_FILE))
.PHONY: $(1)
$(1): $($(1)_FILE)
endif

$($(1)_FILE): $(BUILDDIR)/$(1)/.dirs build/modules/$(1).mk $(foreach DEP,$($(1)_DEPS),$($(DEP)_FILE)) $($(1)_OBJS)
	@echo '===> LD [$$@]'
	$(Q)mkdir -p $(dir $($(1)_FILE))
	$(Q)$(CROSS)$($(1)_LINKER) $($(1)_OBJS) $($(1)_LDFLAGS) $(LDFLAGS) -o $($(1)_FILE)

$(BUILDDIR)/$(1)/%.c.o: $(SRCDIR)/%.c $(BUILDDIR)/$(1)/.dirs
	@echo '===> CC [$(1)] $$<'
	$(Q)$(CROSS)$(CC) $(CCFLAGS) $($(1)_CCFLAGS) -c -o $$@ $$< $(DEP_FLAGS)

$(BUILDDIR)/$(1)/%.rc.o: $(SRCDIR)/%.rc $(BUILDDIR)/$(1)/.dirs
	@echo '===> WINDRES [$(1)] $$<'
	$(Q)$(CROSS)$(WINDRES) -v $(subst \,\\\,$(subst -DUFO_REVISION,-D UFO_REVISION,$(filter -DUFO_REVISION=%,$(CXXFLAGS)))) $(subst -DDEBUG, -D DEBUG,$(filter -DDEBUG,$(CXXFLAGS))) -D FULL_PATH_RC_FILE -i $$< -o $$@

$(BUILDDIR)/$(1)/%.cpp.o: $(SRCDIR)/%.cpp $(BUILDDIR)/$(1)/.dirs
	@echo '===> CXX [$(1)] $$<'
	$(Q)$(CROSS)$(CXX) $(CXXFLAGS) $($(1)_CXXFLAGS) -c -o $$@ $$< $(DEP_FLAGS)

$(BUILDDIR)/$(1)/.dirs: config.h
	$(Q)mkdir -p $(foreach i,$($(1)_OBJS),$(dir $(i)))
	$(Q)touch $$@

.PHONY: clean-$(1)
clean-$(1):
	@echo 'Cleaning up $(1)'
	$(Q)rm -rf $(BUILDDIR)/$(1) $($(1)_FILE)

.PHONY: strip-$(1)
strip-$(1): $($(1)_FILE)
	@echo 'Stripping $$<'
	$(Q)strip $$<

install-$(1): $($(1)_FILE)
	@echo 'Install $$<'
	$(Q)$(INSTALL_DIR) $(DESTDIR)$(PKGDATADIR)/$(dir $($(1)_FILE))
	$(Q)$(INSTALL_PROGRAM) $$< $(DESTDIR)$(PKGDATADIR)/$$<

else
# if this target is ignored, just do nothing
$(1):
clean-$(1):
strip-$(1):
install-$(1):
endif
endif
endef
$(foreach TARGET,$(TARGETS),$(eval $(call BUILD_RULE,$(TARGET))))

.PHONY: run-configure
run-configure:
	$(Q)./configure $(CONFIGURE_OPTIONS)

include build/data.mk
include build/install.mk
include build/lang.mk
include build/maps.mk
include build/models.mk
include build/various.mk

.PHONY: emscripten
emscripten:
	$(Q)$(EMSCRIPTEN_ROOT)/emmake $(MAKE) EMSCRIPTEN_CALLED=1
