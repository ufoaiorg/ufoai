all: create-dmg

TARGET_CPU?=universal
EXT=pk3
ROOTDIR=../../../..
#relative from root dir
INSTDIR=src/ports/macosx/installer/
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

BUNDLE_PK3 = $(addprefix UFOAI.app/base/, $(PAK_FILES))

# =======================

bundle-dirs-ufoai:
	@mkdir -p UFOAI.app/base

bundle-dirs-uforadiant:

# =======================

$(BASE_PK3) :
	$(MAKE) -C $(ROOTDIR) $(addprefix base/,$(notdir $@))

$(BUNDLE_PK3) : UFOAI.app/base/%.pk3 : $(ROOTDIR)/base/%.pk3
	cp $< $@

# =======================

package-dir-ufoai:
	@rm -rf $(UFOAI_PACKAGE_NAME)
	@mkdir -p $(UFOAI_PACKAGE_NAME)

package-dir-uforadiant:
	@rm -rf $(UFORADIANT_PACKAGE_NAME)
	@mkdir -p $(UFORADIANT_PACKAGE_NAME)

# =======================

updateversion-ufoai:
	@sed 's/@UFOAI_VERSION@/$(UFOAI_VERSION)/g' UFOAI.app/Contents/Info.plist.in > UFOAI.app/Contents/Info.plist

updateversion-uforadiant:
	@sed 's/@UFORADIANT_VERSION@/$(UFORADIANT_VERSION)/g' UFORadiant.app/Contents/Info.plist.in > UFORadiant.app/Contents/Info.plist

# =======================

copybinaries-ufoai: bundle-dirs-ufoai
	@cp $(BINARIES) UFOAI.app/Contents/MacOS
	@cp $(BINARIES_BASE) UFOAI.app/base

copybinaries-uforadiant: bundle-dirs-uforadiant
	@cp $(BINARIES_RADIANT) UFORadiant.app/Contents/MacOS

# =======================

copydata-ufoai: bundle-dirs-ufoai $(BUNDLE_PK3)
	@mkdir -p UFOAI.app/base/i18n
	@cp -r $(ROOTDIR)/base/i18n/[^.]* UFOAI.app/base/i18n

copydata-uforadiant:
	@svn export --force $(ROOTDIR)/radiant UFORadiant.app
	@cp -r $(ROOTDIR)/radiant/i18n/[^.]* UFORadiant.app/i18n
	@cp -rf $(ROOTDIR)/radiant/modules/[^.]*.dylib UFORadiant.app/modules
	@cp -rf $(ROOTDIR)/radiant/plugins/[^.]*.dylib UFORadiant.app/plugins

# =======================

copynotes-ufoai: package-dir-ufoai
	@cp $(ROOTDIR)/README $(UFOAI_PACKAGE_NAME)
	@cp $(ROOTDIR)/CONTRIBUTORS $(UFOAI_PACKAGE_NAME)
	@cp $(ROOTDIR)/COPYING $(UFOAI_PACKAGE_NAME)

copynotes-uforadiant: package-dir-uforadiant
	@cp $(ROOTDIR)/src/tools/radiant/LICENSE $(UFORADIANT_PACKAGE_NAME)

# =======================

copylibs-ufoai:
	@rm -rf UFOAI.app/Contents/Frameworks/*.framework
	@perl macfixlibs.pl UFOAI.app ufo ufoded ufo2map

copylibs-uforadiant:
	@rm -rf UFORadiant.app/Contents/Frameworks/*.framework
	@perl macfixlibs.pl UFORadiant.app uforadiant

# =======================

copy-package-bundle-ufoai: package-dir-ufoai bundle-ufoai
	@cp -r UFOAI.app $(UFOAI_PACKAGE_NAME)
	@rm $(UFOAI_PACKAGE_NAME)/UFOAI.app/Contents/Info.plist.in

copy-package-bundle-uforadiant: package-dir-uforadiant bundle-uforadiant
	@cp -r UFORadiant.app $(UFORADIANT_PACKAGE_NAME)
	@rm $(UFORADIANT_PACKAGE_NAME)/UFORadiant.app/Contents/Info.plist.in

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
	@rm -f $(UFOAI_PACKAGE_NAME).dmg
	@hdiutil create -volname "UFO: Alien Invasion $(UFOAI_VERSION)" -srcfolder $(UFOAI_PACKAGE_NAME) $(UFOAI_PACKAGE_NAME).dmg

create-dmg-uforadiant: bundle-uforadiant updateversion-uforadiant copy-package-bundle-uforadiant strip-dev-files-uforadiant
	@rm -f $(UFORADIANT_PACKAGE_NAME).dmg
	@hdiutil create -volname "UFORadiant $(UFORADIANT_VERSION)" -srcfolder $(UFORADIANT_PACKAGE_NAME) $(UFORADIANT_PACKAGE_NAME).dmg

create-dmg: create-dmg-ufoai create-dmg-uforadiant

