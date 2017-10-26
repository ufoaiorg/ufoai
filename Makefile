CONFIG ?= Makefile.local
-include $(CONFIG)

ifneq ($(ADDITIONAL_PATH),)
PATH        := $(ADDITIONAL_PATH):$(PATH)
endif

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
PROJECTSDIR ?= build/projects
PROJECTS    := $(sort $(patsubst $(PROJECTSDIR)/%.mk,%,$(wildcard $(PROJECTSDIR)/*.mk)))
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
UNINSTALL_FILE  ?= rm --interactive=never
UNINSTALL_DIR   ?= rmdir -p --ignore-fail-on-non-empty

ifeq ($(USE_CCACHE),1)
CCACHE := ccache
else
CCACHE :=
endif

.PHONY: all
ifeq ($(TARGET_OS),android)
all: android
else
ifeq ($(TARGET_OS),html5)
ifeq ($(EMSCRIPTEN_CALLED),1)
all: $(TARGETS)
else
all: emscripten
endif
else
all: $(TARGETS)
endif
endif

include build/flags.mk
include build/platforms/$(TARGET_OS).mk
include build/modes/$(MODE).mk
include build/default.mk

CXXFLAGS := $(CFLAGS) $(CXXFLAGS)

ASSEMBLE_OBJECTS = $(patsubst %, $(BUILDDIR)/$(1)/%.o, $(filter %.c %.cpp %.rc, $($(1)_SRCS)))
FILTER_OUT = $(foreach v,$(2),$(if $(findstring $(1),$(v)),,$(v)))

define INCLUDE_PROJECT_RULE
include $(PROJECTSDIR)/$(1).mk
endef

define INCLUDE_RULE
include build/modules/$(1).mk
endef
$(foreach TARGET,$(TARGETS_TMP),$(eval $(call INCLUDE_RULE,$(TARGET))))

.PHONY: clean
clean: $(addprefix clean-,$(TARGETS)) clean-docs
	$(Q)rm -rf $(BUILDDIR)

.PHONY: distclean
distclean: clean
	$(Q)rm -f $(TARGET_OS)-config.h Makefile.local
	$(Q)rm -rf $(BUILDDIR)

.PHONY: install
install: install-pre $(addprefix install-,$(TARGETS))

.PHONY: uninstall
uninstall:  uninstall-pre $(addprefix uninstall-,$(TARGETS))

.PHONY: strip
strip: $(addprefix strip-,$(TARGETS))

$(TARGET_OS)-config.h Makefile.local: configure
	@echo "restarting configure for $(TARGET_OS)"
	$(Q)$(CONFIGURE_PREFIX) ./configure $(CONFIGURE_OPTIONS)
#	Makefile remaking should take care of this
#	$(Q)$(MAKE) $(MAKECMDGOALS)

define BUILD_RULE
ifndef $(1)_DISABLE
ifndef $(1)_IGNORE

-include $($(1)_OBJS:.o=.d)

# if the target filename differs:
ifneq ($(1),$($(1)_FILE))
.PHONY: $(1)
$(1): $(TARGET_OS)-config.h $($(1)_FILE)
endif

$($(1)_FILE): $(BUILDDIR)/$(1)/.dirs build/modules/$(1).mk $(foreach DEP,$($(1)_DEPS),$($(DEP)_FILE)) $($(1)_OBJS)
	@echo '===> LD [$$@]'
	$(Q)mkdir -p $(dir $($(1)_FILE))
	$(Q)$(CCACHE) $(CROSS)$($(1)_LINKER) $($(1)_OBJS) $($(1)_LDFLAGS) $(LDFLAGS) -o $($(1)_FILE)

$(BUILDDIR)/$(1)/%.c.o: $(SRCDIR)/%.c $(BUILDDIR)/$(1)/.dirs
	@echo '===> CC [$(1)] $$<'
	$(Q)$(CCACHE) $(CROSS)$(CC) $(CCFLAGS) $($(1)_CCFLAGS) -c -o $$@ $$< $(DEP_FLAGS)

$(BUILDDIR)/$(1)/%.m.o: $(SRCDIR)/%.m $(BUILDDIR)/$(1)/.dirs
	@echo '===> CC [$(1)] $$<'
	$(Q)$(CCACHE) $(CROSS)$(CC) $(CCFLAGS) $($(1)_CCFLAGS) -c -o $$@ $$< $(DEP_FLAGS)

$(BUILDDIR)/$(1)/%.mm.o: $(SRCDIR)/%.mm $(BUILDDIR)/$(1)/.dirs
	@echo '===> CC [$(1)] $$<'
	$(Q)$(CCACHE) $(CROSS)$(CC) $(CCFLAGS) $($(1)_CCFLAGS) -c -o $$@ $$< $(DEP_FLAGS)

$(BUILDDIR)/$(1)/%.rc.o: $(SRCDIR)/%.rc $(BUILDDIR)/$(1)/.dirs
	@echo '===> WINDRES [$(1)] $$<'
	$(Q)$(CROSS)$(WINDRES) -v $(subst \,\\\,$(subst -DUFO_REVISION,-D UFO_REVISION,$(filter -DUFO_REVISION=%,$(CXXFLAGS)))) $(subst -DDEBUG, -D DEBUG,$(filter -DDEBUG,$(CXXFLAGS))) -D FULL_PATH_RC_FILE -i $$< -o $$@

$(BUILDDIR)/$(1)/%.cc.o: $(SRCDIR)/%.cc $(BUILDDIR)/$(1)/.dirs
	@echo '===> CXX [$(1)] $$<'
	$(Q)$(CCACHE) $(CROSS)$(CXX) $(CXXFLAGS) $($(1)_CXXFLAGS) -c -o $$@ $$< $(DEP_FLAGS)

$(BUILDDIR)/$(1)/%.cpp.o: $(SRCDIR)/%.cpp $(BUILDDIR)/$(1)/.dirs
	@echo '===> CXX [$(1)] $$<'
	$(Q)$(CCACHE) $(CROSS)$(CXX) $(CXXFLAGS) $($(1)_CXXFLAGS) -c -o $$@ $$< $(DEP_FLAGS)

$(BUILDDIR)/$(1)/.dirs: $(TARGET_OS)-config.h
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

uninstall-$(1):
	@echo 'Uninstall $($(1)_FILE)'
	$(Q)$(UNINSTALL_FILE) $(DESTDIR)$(PKGDATADIR)/$($(1)_FILE)
	$(Q)$(UNINSTALL_DIR) $(dir $(DESTDIR)$(PKGDATADIR)/$($(1)_FILE))

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

$(foreach PROJECT,$(PROJECTS),$(eval $(call INCLUDE_PROJECT_RULE,$(PROJECT))))

.PHONY: run-configure
run-configure:
	$(Q)$(CONFIGURE_PREFIX) ./configure $(CONFIGURE_OPTIONS)

include build/data.mk
include build/install.mk
include build/lang.mk
include build/maps.mk
include build/models.mk
include build/various.mk

ifeq ($(TARGET_OS),android)
.PHONY: clean-android
clean-android:
	@echo "===> ANDROID [clean project]"
	$(Q)rm -rf android-project/assets
	$(Q)rm -rf android-project/bin
	$(Q)rm -f android-project/jni/SDL \
		android-project/jni/SDL_mixer \
		android-project/jni/SDL_ttf \
		android-project/jni/libpng \
		android-project/jni/jpeg \
		android-project/jni/ogg \
		android-project/jni/curl \
		android-project/jni/vorbis \
		android-project/jni/intl \
		android-project/jni/zlib \
		android-project/jni/theora
	$(Q)cd android-project && ant clean

.PHONY: android-update-project
android-update-project: clean-android
	@echo "===> ANDROID [update project]"
	$(Q)cd android-project && android update project -p . -t android-13

.PHONY: android-copy-assets
android-copy-assets: clean-android pk3
	@echo "===> CP [copy assets]"
	$(Q)mkdir -p android-project/assets/base
	$(Q)cp -l base/*.pk3 android-project/assets/base
	$(Q)for i in hdpi ldpi mdpi xhdpi; do mkdir -p android-project/res/drawable-$${i}; cp contrib/installer/ufoai.png android-project/res/drawable-$${i}/icon.png; done

.PHONY: android
android: android-update-project android-copy-assets
	$(Q)ln -sf ../../src/libs/SDL android-project/jni/SDL
	$(Q)ln -sf ../../src/libs/SDL_mixer android-project/jni/SDL_mixer
	$(Q)ln -sf ../../src/libs/SDL_ttf android-project/jni/SDL_ttf
	$(Q)ln -sf ../../src/libs/libpng-1.6.2 android-project/jni/libpng
	$(Q)ln -sf ../../src/libs/jpeg-9 android-project/jni/jpeg
	$(Q)ln -sf ../../src/libs/curl android-project/jni/curl
	$(Q)ln -sf ../../src/libs/libogg-1.3.1 android-project/jni/ogg
	$(Q)ln -sf ../../src/libs/vorbis android-project/jni/vorbis
	$(Q)ln -sf ../../src/libs/intl android-project/jni/intl
	$(Q)ln -sf ../../src/libs/theora android-project/jni/theora
	$(Q)ln -sf ../../src/libs/zlib-1.2.8 android-project/jni/zlib
	@echo "===> NDK [native build]"
	$(Q)cd android-project && ndk-build V=1 -j 5
	@echo "===> ANT [java build]"
	$(Q)cd android-project && ant debug
	$(Q)cd android-project && ant installd

android-uninstall:
	$(Q)cd android-project && ant uninstall org.ufoai
endif

ifeq ($(TARGET_OS),html5)
.PHONY: emscripten
emscripten:
	$(Q)$(EMSCRIPTEN_ROOT)/emmake $(MAKE) EMSCRIPTEN_CALLED=1
endif
