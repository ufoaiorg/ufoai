TARGET_CPU?=universal
MAC_INST_DIR=$(INSTALLER_DIR)/mac

UFOAI_MAC_PACKAGE_NAME=ufoai-$(UFOAI_VERSION)-macosx-$(TARGET_CPU)
UFORADIANT_MAC_PACKAGE_NAME=uforadiant-$(UFORADIANT_VERSION)-macosx-$(TARGET_CPU)

BUNDLE_PK3 = $(addprefix $(MAC_INST_DIR)/UFOAI.app/base/, $(PAK_FILES))

macinstaller: installer-pre create-dmg-ufoai create-dmg-uforadiant

# =======================

bundle-dirs-ufoai:
	$(Q)mkdir -p $(MAC_INST_DIR)/UFOAI.app/base
	$(Q)mkdir -p $(MAC_INST_DIR)/UFOAI.app/Contents/MacOS
	$(Q)mkdir -p $(MAC_INST_DIR)/UFOAI.app/Contents/Libraries
	$(Q)mkdir -p $(MAC_INST_DIR)/UFOAI.app/Contents/Frameworks

bundle-dirs-uforadiant:
	$(Q)mkdir -p $(MAC_INST_DIR)/UFORadiant.app/Contents/MacOS
	$(Q)mkdir -p $(MAC_INST_DIR)/UFORadiant.app/Contents/Libraries
	$(Q)mkdir -p $(MAC_INST_DIR)/UFORadiant.app/Contents/Frameworks

# =======================

package-dir-ufoai:
	$(Q)rm -rf $(MAC_INST_DIR)/$(UFOAI_MAC_PACKAGE_NAME)
	$(Q)mkdir -p $(MAC_INST_DIR)/$(UFOAI_MAC_PACKAGE_NAME)

package-dir-uforadiant:
	$(Q)rm -rf $(MAC_INST_DIR)/$(UFORADIANT_MAC_PACKAGE_NAME)
	$(Q)mkdir -p $(MAC_INST_DIR)/$(UFORADIANT_MAC_PACKAGE_NAME)

# =======================

updateversion-ufoai:
	$(Q)sed 's/@UFOAI_VERSION@/$(UFOAI_VERSION)/g' $(MAC_INST_DIR)/UFOAI.app/Contents/Info.plist.in > $(MAC_INST_DIR)/UFOAI.app/Contents/Info.plist

updateversion-uforadiant:
	$(Q)sed 's/@UFORADIANT_VERSION@/$(UFORADIANT_VERSION)/g' $(MAC_INST_DIR)/UFORadiant.app/Contents/Info.plist.in > $(MAC_INST_DIR)/UFORadiant.app/Contents/Info.plist

# =======================

copybinaries-ufoai: bundle-dirs-ufoai
	$(Q)cp $(BINARIES) $(MAC_INST_DIR)/UFOAI.app/Contents/MacOS
	$(Q)cp $(BINARIES_BASE) $(MAC_INST_DIR)/UFOAI.app/base

copybinaries-uforadiant: bundle-dirs-uforadiant
	$(Q)cp $(BINARIES_RADIANT) $(MAC_INST_DIR)/UFORadiant.app/Contents/MacOS

# =======================

copydata-ufoai: bundle-dirs-ufoai
	$(Q)mkdir -p $(MAC_INST_DIR)/UFOAI.app/base/i18n
	$(Q)cp -r base/i18n/[^.]* $(MAC_INST_DIR)/UFOAI.app/base/i18n
	$(Q)cp base/*.pk3 $(MAC_INST_DIR)/UFOAI.app/base

copydata-uforadiant:
	$(Q)git archive HEAD:radiant/ | tar -x -C $(MAC_INST_DIR)/UFORadiant.app
	$(Q)cp -r radiant/i18n/[^.]* $(MAC_INST_DIR)/UFORadiant.app/i18n

# =======================

copynotes-ufoai: package-dir-ufoai
	$(Q)cp README $(MAC_INST_DIR)/$(UFOAI_MAC_PACKAGE_NAME)
	$(Q)cp COPYING $(MAC_INST_DIR)/$(UFOAI_MAC_PACKAGE_NAME)

copynotes-uforadiant: package-dir-uforadiant
	$(Q)cp src/tools/radiant/LICENSE $(MAC_INST_DIR)/$(UFORADIANT_MAC_PACKAGE_NAME)

# =======================

copylibs-ufoai:
	$(Q)rm -rf $(MAC_INST_DIR)/UFOAI.app/Contents/Frameworks/*.framework
	$(Q)perl $(MAC_INST_DIR)/macfixlibs.pl $(MAC_INST_DIR)/UFOAI.app ufo ufoded ufo2map ufomodel

copylibs-uforadiant:
	$(Q)rm -rf $(MAC_INST_DIR)/UFORadiant.app/Contents/Frameworks/*.framework
	$(Q)perl $(MAC_INST_DIR)/macfixlibs.pl $(MAC_INST_DIR)/UFORadiant.app uforadiant

# =======================

copy-package-bundle-ufoai: package-dir-ufoai bundle-ufoai
	$(Q)cp -r $(MAC_INST_DIR)/UFOAI.app $(MAC_INST_DIR)/$(UFOAI_MAC_PACKAGE_NAME)

copy-package-bundle-uforadiant: package-dir-uforadiant bundle-uforadiant
	$(Q)cp -r $(MAC_INST_DIR)/UFORadiant.app $(MAC_INST_DIR)/$(UFORADIANT_MAC_PACKAGE_NAME)

# =======================

strip-dev-files-ufoai: copy-package-bundle-ufoai
	$(Q)find $(MAC_INST_DIR)/$(UFOAI_MAC_PACKAGE_NAME)/UFOAI.app -name ".gitignore" | xargs rm -f
	$(Q)rm $(MAC_INST_DIR)/$(UFOAI_MAC_PACKAGE_NAME)/UFOAI.app/Contents/Info.plist.in

strip-dev-files-uforadiant: copy-package-bundle-uforadiant
	$(Q)find $(MAC_INST_DIR)/$(UFORADIANT_MAC_PACKAGE_NAME)/UFORadiant.app -name ".gitignore" | xargs rm -f
	$(Q)rm $(MAC_INST_DIR)/$(UFORADIANT_MAC_PACKAGE_NAME)/UFORadiant.app/Contents/Info.plist.in

# =======================

bundle-ufoai: copybinaries-ufoai copydata-ufoai copylibs-ufoai copynotes-ufoai

bundle-uforadiant: copybinaries-uforadiant copydata-uforadiant copylibs-uforadiant copynotes-uforadiant

# for testing:
bundle-nodata-ufoai: copybinaries-ufoai copylibs-ufoai copynotes-ufoai

# =======================

create-dmg-ufoai: bundle-ufoai updateversion-ufoai copy-package-bundle-ufoai strip-dev-files-ufoai
	$(Q)rm -f $(UFOAI_MAC_PACKAGE_NAME).dmg
	$(Q)hdiutil create -volname "UFO: Alien Invasion $(UFOAI_VERSION)" -srcfolder $(MAC_INST_DIR)/$(UFOAI_MAC_PACKAGE_NAME) $(UFOAI_MAC_PACKAGE_NAME).dmg

create-dmg-uforadiant: bundle-uforadiant updateversion-uforadiant copy-package-bundle-uforadiant strip-dev-files-uforadiant
	$(Q)rm -f $(UFORADIANT_MAC_PACKAGE_NAME).dmg
	$(Q)hdiutil create -volname "UFORadiant $(UFORADIANT_VERSION)" -srcfolder $(MAC_INST_DIR)/$(UFORADIANT_MAC_PACKAGE_NAME) $(UFORADIANT_MAC_PACKAGE_NAME).dmg

create-dmg: create-dmg-ufoai create-dmg-uforadiant
