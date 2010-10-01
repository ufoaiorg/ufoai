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

INSTALL         ?= install
INSTALL_PROGRAM ?= $(INSTALL) -m 755 -s
INSTALL_SCRIPT  ?= $(INSTALL) -m 755
INSTALL_DIR     ?= $(INSTALL) -d
INSTALL_MAN     ?= $(INSTALL) -m 444
INSTALL_DATA    ?= $(INSTALL) -m 444

.PHONY: all
all: $(TARGETS)

include build/flags.mk
include build/platforms/$(TARGET_OS).mk
include build/modes/$(MODE).mk

ASSEMBLE_OBJECTS = \
	$(addprefix $(BUILDDIR)/$(1)/,$(addsuffix .o,$(filter %.c,$($(1)_SRCS)))) \
	$(addprefix $(BUILDDIR)/$(1)/,$(addsuffix .o,$(filter %.rc,$($(1)_SRCS)))) \
	$(addprefix $(BUILDDIR)/$(1)/,$(addsuffix .o,$(filter %.m,$($(1)_SRCS)))) \
	$(addprefix $(BUILDDIR)/$(1)/,$(addsuffix .o,$(filter %.cpp,$($(1)_SRCS)))) \
	$(addprefix $(BUILDDIR)/$(1)/,$(addsuffix .o,$(filter %.cc,$($(1)_SRCS))))

define INCLUDE_RULE
include build/modules/$(1).mk
endef
$(foreach TARGET,$(TARGETS),$(eval $(call INCLUDE_RULE,$(TARGET))))

.PHONY: clean
clean: $(addprefix clean-,$(TARGETS))

.PHONY: distclean
distclean: clean
	$(Q)rm -f config.h Makefile.local

.PHONY: install
install: install-pre $(addprefix install-,$(TARGETS))

.PHONY: strip
strip: $(addprefix strip-,$(TARGETS))

config.h: configure
	$(Q)./configure $(CONFIGURE_OPTIONS)

define BUILD_RULE
ifndef $(1)_DISABLE
ifndef $(1)_IGNORE

DEPENDENCIES = $($(1)_OBJS:.o=.d)
-include $(DEPENDENCIES)

# if the target filename differs:
ifneq ($(1),$($(1)_FILE))
.PHONY: $(1)
$(1): $($(1)_FILE)
endif

$($(1)_FILE): $(BUILDDIR)/$(1)/.dirs $(foreach DEP,$($(1)_DEPS),$($(DEP)_FILE)) $($(1)_OBJS)
	@echo '===> LD [$$@]'
	$(Q)$(CROSS)$(CC) $(CFLAGS) $($(1)_OBJS) $(LDFLAGS) $($(1)_LDFLAGS) -o $($(1)_FILE)

$(BUILDDIR)/$(1)/%.c.o: $(SRCDIR)/%.c $(BUILDDIR)/$(1)/.dirs
	@echo '===> CC [$(1)] $$<'
	$(Q)$(CROSS)$(CC) $(CCFLAGS) $($(1)_CCFLAGS) -c -o $$@ $$< -MD -MT $$@ -MP

$(BUILDDIR)/$(1)/%.m.o: $(SRCDIR)/%.m $(BUILDDIR)/$(1)/.dirs
	@echo '===> CC [$(1)] $$<'
	$(Q)$(CROSS)$(CC) $(CCFLAGS) $($(1)_CCFLAGS) -c -o $$@ $$< -MD -MT $$@ -MP

$(BUILDDIR)/$(1)/%.rc.o: $(SRCDIR)/%.rc $(BUILDDIR)/$(1)/.dirs
	@echo '===> WINDRES [$(1)] $$<'
	$(Q)$(CROSS)$(WINDRES) -i $$< -o $$@

$(BUILDDIR)/$(1)/%.cc.o: $(SRCDIR)/%.cc $(BUILDDIR)/$(1)/.dirs
	@echo '===> CXX [$(1)] $$<'
	$(Q)$(CROSS)$(CXX) $(CXXFLAGS) $($(1)_CXXFLAGS) -c -o $$@ $$< -MD -MT $$@ -MP

$(BUILDDIR)/$(1)/%.cpp.o: $(SRCDIR)/%.cpp $(BUILDDIR)/$(1)/.dirs
	@echo '===> CXX [$(1)] $$<'
	$(Q)$(CROSS)$(CXX) $(CXXFLAGS) $($(1)_CXXFLAGS) -c -o $$@ $$< -MD -MT $$@ -MP

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
	$(Q)$(INSTALL_DIR) $(PKGDATADIR)/$(shell dirname $$<)
	$(Q)$(INSTALL_PROGRAM) $$< $(PKGDATADIR)/$$<

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
