all: create-dmg

TARGET_CPU?=universal
EXT=pk3
ROOTDIR=../../..
#relative from root dir
INSTDIR=contrib/installer/mac
UFOAI_VERSION ?= 0.0
UFORADIANT_VERSION ?= 0.0

UFOAI_PACKAGE_NAME=ufoai-$(UFOAI_VERSION)-macosx-$(TARGET_CPU)
UFORADIANT_PACKAGE_NAME=uforadiant-$(UFORADIANT_VERSION)-macosx-$(TARGET_CPU)

BINARIES = \
	$(ROOTDIR)/ufo \
	$(ROOTDIR)/ufoded \
	$(ROOTDIR)/ufo2map
BINARIES_BASE = \
	$(ROOTDIR)/base/game.dylib
BINARIES_RADIANT = \
	$(ROOTDIR)/radiant/uforadiant

include $(ROOTDIR)/build/pk3_def.mk

BASE_PK3 = $(addprefix $(ROOTDIR)/base/, $(PAK_FILES))

BUNDLE_PK3 = $(addprefix $(ROOTDIR)/contrib/installer/mac/UFOAI.app/base/, $(PAK_FILES))

# =======================

bundle-dirs-ufoai:
	@mkdir -p $(ROOTDIR)/contrib/installer/mac/UFOAI.app/base

bundle-dirs-uforadiant:

# =======================

$(BASE_PK3) :
	$(MAKE) -C $(ROOTDIR) $(addprefix base/,$(notdir $@))

$(BUNDLE_PK3) : $(ROOTDIR)/contrib/installer/mac/UFOAI.app/base/%.pk3 : $(ROOTDIR)/base/%.pk3
	cp $< $@

# =======================

package-dir-ufoai:
	@rm -rf $(ROOTDIR)/contrib/installer/mac/$(UFOAI_PACKAGE_NAME)
	@mkdir -p $(ROOTDIR)/contrib/installer/mac/$(UFOAI_PACKAGE_NAME)

package-dir-uforadiant:
	@rm -rf $(ROOTDIR)/contrib/installer/mac/$(UFORADIANT_PACKAGE_NAME)
	@mkdir -p $(ROOTDIR)/contrib/installer/mac/$(UFORADIANT_PACKAGE_NAME)

# =======================

updateversion-ufoai:
	@sed 's/@UFOAI_VERSION@/$(UFOAI_VERSION)/g' $(ROOTDIR)/contrib/installer/mac/UFOAI.app/Contents/Info.plist.in > $(ROOTDIR)/contrib/installer/mac/UFOAI.app/Contents/Info.plist

updateversion-uforadiant:
	@sed 's/@UFORADIANT_VERSION@/$(UFORADIANT_VERSION)/g' $(ROOTDIR)/contrib/installer/mac/UFORadiant.app/Contents/Info.plist.in > $(ROOTDIR)/contrib/installer/mac/UFORadiant.app/Contents/Info.plist

# =======================

copybinaries-ufoai: bundle-dirs-ufoai
	@cp $(BINARIES) $(ROOTDIR)/contrib/installer/mac/UFOAI.app/Contents/MacOS
	@cp $(BINARIES_BASE) $(ROOTDIR)/contrib/installer/mac/UFOAI.app/base

copybinaries-uforadiant: bundle-dirs-uforadiant
	@cp $(BINARIES_RADIANT) $(ROOTDIR)/contrib/installer/mac/UFORadiant.app/Contents/MacOS

# =======================

copydata-ufoai: bundle-dirs-ufoai $(BUNDLE_PK3)
	@mkdir -p $(ROOTDIR)/contrib/installer/mac/UFOAI.app/base/i18n
	@cp -r $(ROOTDIR)/base/i18n/[^.]* $(ROOTDIR)/contrib/installer/mac/UFOAI.app/base/i18n

copydata-uforadiant:
	@svn export --force $(ROOTDIR)/radiant $(ROOTDIR)/contrib/installer/mac/UFORadiant.app
	@cp -r $(ROOTDIR)/radiant/i18n/[^.]* $(ROOTDIR)/contrib/installer/mac/UFORadiant.app/i18n
	@cp -rf $(ROOTDIR)/radiant/plugins/[^.]*.dylib $(ROOTDIR)/contrib/installer/mac/UFORadiant.app/plugins

# =======================

copynotes-ufoai: package-dir-ufoai
	@cp $(ROOTDIR)/README $(ROOTDIR)/contrib/installer/mac/$(UFOAI_PACKAGE_NAME)
	@cp $(ROOTDIR)/CONTRIBUTORS $(ROOTDIR)/contrib/installer/mac/$(UFOAI_PACKAGE_NAME)
	@cp $(ROOTDIR)/COPYING $(ROOTDIR)/contrib/installer/mac/$(UFOAI_PACKAGE_NAME)

copynotes-uforadiant: package-dir-uforadiant
	@cp $(ROOTDIR)/src/tools/radiant/LICENSE $(ROOTDIR)/contrib/installer/mac/$(UFORADIANT_PACKAGE_NAME)

# =======================

copylibs-ufoai:
	@rm -rf UFOAI.app/Contents/Frameworks/*.framework
	@perl $(ROOTDIR)/contrib/installer/mac/macfixlibs.pl $(ROOTDIR)/contrib/installer/mac/UFOAI.app ufo ufoded ufo2map

copylibs-uforadiant:
	@rm -rf UFORadiant.app/Contents/Frameworks/*.framework
	@perl $(ROOTDIR)/contrib/installer/mac/macfixlibs.pl $(ROOTDIR)/contrib/installer/mac/UFORadiant.app uforadiant

# =======================

copy-package-bundle-ufoai: package-dir-ufoai bundle-ufoai
	@cp -r $(ROOTDIR)/contrib/installer/mac/UFOAI.app $(ROOTDIR)/contrib/installer/mac/$(UFOAI_PACKAGE_NAME)
	@rm $(ROOTDIR)/contrib/installer/mac/$(UFOAI_PACKAGE_NAME)/UFOAI.app/Contents/Info.plist.in

copy-package-bundle-uforadiant: package-dir-uforadiant bundle-uforadiant
	@cp -r $(ROOTDIR)/contrib/installer/mac/UFORadiant.app $(ROOTDIR)/contrib/installer/mac/$(UFORADIANT_PACKAGE_NAME)
	@rm $(ROOTDIR)/contrib/installer/mac/$(UFORADIANT_PACKAGE_NAME)/UFORadiant.app/Contents/Info.plist.in

# =======================

strip-dev-files-ufoai: copy-package-bundle-ufoai
	@find $(UFOAI_PACKAGE_NAME) -type d -print | grep '.*svn$$' | xargs rm -rf

strip-dev-files-uforadiant: copy-package-bundle-uforadiant
	@find $(UFORADIANT_PACKAGE_NAME) -type d -print | grep '.*svn$$' | xargs rm -rf

# =======================

bundle-ufoai: copybinaries-ufoai copydata-ufoai copylibs-ufoai copynotes-ufoai

bundle-uforadiant: copybinaries-uforadiant copydata-uforadiant copylibs-uforadiant copynotes-uforadiant

# for testing:
bundle-nodata-ufoai: copybinaries-ufoai copylibs-ufoai copynotes-ufoai

# =======================

create-dmg-ufoai: bundle-ufoai updateversion-ufoai copy-package-bundle-ufoai strip-dev-files-ufoai
	@rm -f $(ROOTDIR)/contrib/installer/mac/$(UFOAI_PACKAGE_NAME).dmg
	@hdiutil create -volname "UFO: Alien Invasion $(UFOAI_VERSION)" -srcfolder $(ROOTDIR)/contrib/installer/mac/$(UFOAI_PACKAGE_NAME) $(UFOAI_PACKAGE_NAME).dmg

create-dmg-uforadiant: bundle-uforadiant updateversion-uforadiant copy-package-bundle-uforadiant strip-dev-files-uforadiant
	@rm -f $(ROOTDIR)/contrib/installer/mac/$(UFORADIANT_PACKAGE_NAME).dmg
	@hdiutil create -volname "UFORadiant $(UFORADIANT_VERSION)" -srcfolder $(ROOTDIR)/contrib/installer/mac/$(UFORADIANT_PACKAGE_NAME) $(UFORADIANT_PACKAGE_NAME).dmg

create-dmg: create-dmg-ufoai create-dmg-uforadiant

