TARGET_CPU?=universal
ROOTDIR=.
DIR=$(ROOTDIR)/contrib/installer/mac

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

BUNDLE_PK3 = $(addprefix $(DIR)/UFOAI.app/base/, $(PAK_FILES))

macinstaller: lang maps-sync pk3 create-dmg-ufoai create-dmg-uforadiant

# =======================

bundle-dirs-ufoai:
	@mkdir -p $(DIR)/UFOAI.app/base

bundle-dirs-uforadiant:

# =======================

$(BUNDLE_PK3) : $(DIR)/UFOAI.app/base/%.pk3 : $(ROOTDIR)/base/%.pk3
	cp $< $@

# =======================

package-dir-ufoai:
	@rm -rf $(DIR)/$(UFOAI_PACKAGE_NAME)
	@mkdir -p $(DIR)/$(UFOAI_PACKAGE_NAME)

package-dir-uforadiant:
	@rm -rf $(DIR)/$(UFORADIANT_PACKAGE_NAME)
	@mkdir -p $(DIR)/$(UFORADIANT_PACKAGE_NAME)

# =======================

updateversion-ufoai:
	@sed 's/@UFOAI_VERSION@/$(UFOAI_VERSION)/g' $(DIR)/UFOAI.app/Contents/Info.plist.in > $(DIR)/UFOAI.app/Contents/Info.plist

updateversion-uforadiant:
	@sed 's/@UFORADIANT_VERSION@/$(UFORADIANT_VERSION)/g' $(DIR)/UFORadiant.app/Contents/Info.plist.in > $(DIR)/UFORadiant.app/Contents/Info.plist

# =======================

copybinaries-ufoai: bundle-dirs-ufoai
	@cp $(BINARIES) $(DIR)/UFOAI.app/Contents/MacOS
	@cp $(BINARIES_BASE) $(DIR)/UFOAI.app/base

copybinaries-uforadiant: bundle-dirs-uforadiant
	@cp $(BINARIES_RADIANT) $(DIR)/UFORadiant.app/Contents/MacOS

# =======================

copydata-ufoai: bundle-dirs-ufoai $(BUNDLE_PK3)
	@mkdir -p $(DIR)/UFOAI.app/base/i18n
	@cp -r $(ROOTDIR)/base/i18n/[^.]* $(DIR)/UFOAI.app/base/i18n

copydata-uforadiant:
	@svn export --force $(ROOTDIR)/radiant $(DIR)/UFORadiant.app
	@cp -r $(ROOTDIR)/radiant/i18n/[^.]* $(DIR)/UFORadiant.app/i18n
	@cp -rf $(ROOTDIR)/radiant/plugins/[^.]*.dylib $(DIR)/UFORadiant.app/plugins

# =======================

copynotes-ufoai: package-dir-ufoai
	@cp $(ROOTDIR)/README $(DIR)/$(UFOAI_PACKAGE_NAME)
	@cp $(ROOTDIR)/CONTRIBUTORS $(DIR)/$(UFOAI_PACKAGE_NAME)
	@cp $(ROOTDIR)/COPYING $(DIR)/$(UFOAI_PACKAGE_NAME)

copynotes-uforadiant: package-dir-uforadiant
	@cp $(ROOTDIR)/src/tools/radiant/LICENSE $(DIR)/$(UFORADIANT_PACKAGE_NAME)

# =======================

copylibs-ufoai:
	@rm -rf UFOAI.app/Contents/Frameworks/*.framework
	@perl $(DIR)/macfixlibs.pl $(DIR)/UFOAI.app ufo ufoded ufo2map

copylibs-uforadiant:
	@rm -rf UFORadiant.app/Contents/Frameworks/*.framework
	@perl $(DIR)/macfixlibs.pl $(DIR)/UFORadiant.app uforadiant

# =======================

copy-package-bundle-ufoai: package-dir-ufoai bundle-ufoai
	@cp -r $(DIR)/UFOAI.app $(DIR)/$(UFOAI_PACKAGE_NAME)
	@rm $(DIR)/$(UFOAI_PACKAGE_NAME)/UFOAI.app/Contents/Info.plist.in

copy-package-bundle-uforadiant: package-dir-uforadiant bundle-uforadiant
	@cp -r $(DIR)/UFORadiant.app $(DIR)/$(UFORADIANT_PACKAGE_NAME)
	@rm $(DIR)/$(UFORADIANT_PACKAGE_NAME)/UFORadiant.app/Contents/Info.plist.in

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
	@rm -f $(DIR)/$(UFOAI_PACKAGE_NAME).dmg
	@hdiutil create -volname "UFO: Alien Invasion $(UFOAI_VERSION)" -srcfolder $(DIR)/$(UFOAI_PACKAGE_NAME) $(UFOAI_PACKAGE_NAME).dmg

create-dmg-uforadiant: bundle-uforadiant updateversion-uforadiant copy-package-bundle-uforadiant strip-dev-files-uforadiant
	@rm -f $(DIR)/$(UFORADIANT_PACKAGE_NAME).dmg
	@hdiutil create -volname "UFORadiant $(UFORADIANT_VERSION)" -srcfolder $(DIR)/$(UFORADIANT_PACKAGE_NAME) $(UFORADIANT_PACKAGE_NAME).dmg

create-dmg: create-dmg-ufoai create-dmg-uforadiant

	