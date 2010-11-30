TARGET_CPU?=universal
MAC_INST_DIR=contrib/installer/mac

UFOAI_MAC_PACKAGE_NAME=ufoai-$(UFOAI_VERSION)-macosx-$(TARGET_CPU)
UFORADIANT_MAC_PACKAGE_NAME=uforadiant-$(UFORADIANT_VERSION)-macosx-$(TARGET_CPU)

BUNDLE_PK3 = $(addprefix $(MAC_INST_DIR)/UFOAI.app/base/, $(PAK_FILES))

macinstaller: installer-pre create-dmg-ufoai create-dmg-uforadiant

# =======================

bundle-dirs-ufoai:
	@mkdir -p $(MAC_INST_DIR)/UFOAI.app/base
	@mkdir -p $(MAC_INST_DIR)/UFOAI.app/Contents/MacOS
	@mkdir -p $(MAC_INST_DIR)/UFOAI.app/Contents/Libraries
	@mkdir -p $(MAC_INST_DIR)/UFOAI.app/Contents/Frameworks

bundle-dirs-uforadiant:
	@mkdir -p $(MAC_INST_DIR)/UFORadiant.app/Contents/MacOS
	@mkdir -p $(MAC_INST_DIR)/UFORadiant.app/Contents/Libraries
	@mkdir -p $(MAC_INST_DIR)/UFORadiant.app/Contents/Frameworks

# =======================

package-dir-ufoai:
	@rm -rf $(MAC_INST_DIR)/$(UFOAI_MAC_PACKAGE_NAME)
	@mkdir -p $(MAC_INST_DIR)/$(UFOAI_MAC_PACKAGE_NAME)

package-dir-uforadiant:
	@rm -rf $(MAC_INST_DIR)/$(UFORADIANT_MAC_PACKAGE_NAME)
	@mkdir -p $(MAC_INST_DIR)/$(UFORADIANT_MAC_PACKAGE_NAME)

# =======================

updateversion-ufoai:
	@sed 's/@UFOAI_VERSION@/$(UFOAI_VERSION)/g' $(MAC_INST_DIR)/UFOAI.app/Contents/Info.plist.in > $(MAC_INST_DIR)/UFOAI.app/Contents/Info.plist

updateversion-uforadiant:
	@sed 's/@UFORADIANT_VERSION@/$(UFORADIANT_VERSION)/g' $(MAC_INST_DIR)/UFORadiant.app/Contents/Info.plist.in > $(MAC_INST_DIR)/UFORadiant.app/Contents/Info.plist

# =======================

copybinaries-ufoai: bundle-dirs-ufoai
	@cp $(BINARIES) $(MAC_INST_DIR)/UFOAI.app/Contents/MacOS
	@cp $(BINARIES_BASE) $(MAC_INST_DIR)/UFOAI.app/base

copybinaries-uforadiant: bundle-dirs-uforadiant
	@cp $(BINARIES_RADIANT) $(MAC_INST_DIR)/UFORadiant.app/Contents/MacOS

# =======================

copydata-ufoai: bundle-dirs-ufoai
	@mkdir -p $(MAC_INST_DIR)/UFOAI.app/base/i18n
	@cp -r base/i18n/[^.]* $(MAC_INST_DIR)/UFOAI.app/base/i18n
	@cp base/*.pk3 $(MAC_INST_DIR)/UFOAI.app/base

copydata-uforadiant:
	@git archive HEAD:radiant/ | tar -x -C $(MAC_INST_DIR)/UFORadiant.app
	@cp -r radiant/i18n/[^.]* $(MAC_INST_DIR)/UFORadiant.app/i18n
	@cp -rf radiant/plugins/[^.]*.dylib $(MAC_INST_DIR)/UFORadiant.app/plugins

# =======================

copynotes-ufoai: package-dir-ufoai
	@cp README $(MAC_INST_DIR)/$(UFOAI_MAC_PACKAGE_NAME)
	@cp COPYING $(MAC_INST_DIR)/$(UFOAI_MAC_PACKAGE_NAME)

copynotes-uforadiant: package-dir-uforadiant
	@cp src/tools/radiant/LICENSE $(MAC_INST_DIR)/$(UFORADIANT_MAC_PACKAGE_NAME)

# =======================

copylibs-ufoai:
	@rm -rf $(MAC_INST_DIR)/UFOAI.app/Contents/Frameworks/*.framework
	@perl $(MAC_INST_DIR)/macfixlibs.pl $(MAC_INST_DIR)/UFOAI.app ufo ufoded ufo2map ufomodel

copylibs-uforadiant:
	@rm -rf $(MAC_INST_DIR)/UFORadiant.app/Contents/Frameworks/*.framework
	@perl $(MAC_INST_DIR)/macfixlibs.pl $(MAC_INST_DIR)/UFORadiant.app uforadiant

# =======================

copy-package-bundle-ufoai: package-dir-ufoai bundle-ufoai
	@cp -r $(MAC_INST_DIR)/UFOAI.app $(MAC_INST_DIR)/$(UFOAI_MAC_PACKAGE_NAME)

copy-package-bundle-uforadiant: package-dir-uforadiant bundle-uforadiant
	@cp -r $(MAC_INST_DIR)/UFORadiant.app $(MAC_INST_DIR)/$(UFORADIANT_MAC_PACKAGE_NAME)

# =======================

strip-dev-files-ufoai: copy-package-bundle-ufoai
	@find $(MAC_INST_DIR)/$(UFOAI_MAC_PACKAGE_NAME)/UFOAI.app -name ".gitignore" | xargs rm -f
	@rm $(MAC_INST_DIR)/$(UFOAI_MAC_PACKAGE_NAME)/UFOAI.app/Contents/Info.plist.in

strip-dev-files-uforadiant: copy-package-bundle-uforadiant
	@find $(MAC_INST_DIR)/$(UFORADIANT_MAC_PACKAGE_NAME)/UFORadiant.app -name ".gitignore" | xargs rm -f
	@rm $(MAC_INST_DIR)/$(UFORADIANT_MAC_PACKAGE_NAME)/UFORadiant.app/Contents/Info.plist.in

# =======================

bundle-ufoai: copybinaries-ufoai copydata-ufoai copylibs-ufoai copynotes-ufoai

bundle-uforadiant: copybinaries-uforadiant copydata-uforadiant copylibs-uforadiant copynotes-uforadiant

# for testing:
bundle-nodata-ufoai: copybinaries-ufoai copylibs-ufoai copynotes-ufoai

# =======================

create-dmg-ufoai: bundle-ufoai updateversion-ufoai copy-package-bundle-ufoai strip-dev-files-ufoai
	@rm -f $(UFOAI_MAC_PACKAGE_NAME).dmg
	@hdiutil create -volname "UFO: Alien Invasion $(UFOAI_VERSION)" -srcfolder $(MAC_INST_DIR)/$(UFOAI_MAC_PACKAGE_NAME) $(UFOAI_MAC_PACKAGE_NAME).dmg

create-dmg-uforadiant: bundle-uforadiant updateversion-uforadiant copy-package-bundle-uforadiant strip-dev-files-uforadiant
	@rm -f $(UFORADIANT_MAC_PACKAGE_NAME).dmg
	@hdiutil create -volname "UFORadiant $(UFORADIANT_VERSION)" -srcfolder $(MAC_INST_DIR)/$(UFORADIANT_MAC_PACKAGE_NAME) $(UFORADIANT_MAC_PACKAGE_NAME).dmg

create-dmg: create-dmg-ufoai create-dmg-uforadiant
