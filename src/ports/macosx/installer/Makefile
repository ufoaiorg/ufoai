all: create-dmg

TARGET_CPU?=universal
EXT=pk3
ROOTDIR=../../../..
#relative from root dir
INSTDIR=src/ports/macosx/installer/
#get the version of ufo
VERSION_UC=$(shell grep UFO_VERSION $(ROOTDIR)/src/common/common.h | sed -e 's/.*UFO_VERSION\s*\(.*\)/\1/' | sed -e 's/\"//g')
VERSION_LC=$(shell echo $(VERSION_UC) | sed -e 'y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/;')
RADIANT_VERSION_UC=1.5
RADIANT_VERSION_LC=$(shell echo $(RADIANT_VERSION_UC) | sed -e 'y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/;')

UFOAI_PACKAGE_NAME=ufoai-$(VERSION_LC)-macosx-$(TARGET_CPU)
UFORADIANT_PACKAGE_NAME=uforadiant-$(RADIANT_VERSION_LC)-macosx-$(TARGET_CPU)

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
	cp -v $< $@

# =======================

package-dir-ufoai:
	@rm -rf $(UFOAI_PACKAGE_NAME)
	@mkdir -p $(UFOAI_PACKAGE_NAME)

package-dir-uforadiant:
	@rm -rf $(UFORADIANT_PACKAGE_NAME)
	@mkdir -p $(UFORADIANT_PACKAGE_NAME)

# =======================

updateversion-ufoai:
	@sed 's/@VERSION@/$(VERSION_UC)/g' UFOAI.app/Contents/Info.plist.in > UFOAI.app/Contents/Info.plist

updateversion-uforadiant:
	@sed 's/@RADIANT_VERSION@/$(RADIANT_VERSION_UC)/g' UFORadiant.app/Contents/Info.plist.in > UFORadiant.app/Contents/Info.plist

# =======================

copybinaries-ufoai: bundle-dirs-ufoai
	@cp $(BINARIES) UFOAI.app/Contents/MacOS
	@cp $(BINARIES_BASE) UFOAI.app/base

copybinaries-uforadiant: bundle-dirs-uforadiant
	@cp $(BINARIES_RADIANT) UFORadiant.app/Contents/MacOS

# =======================

copydata-ufoai: bundle-dirs-ufoai $(BUNDLE_PK3)
	@mkdir -p UFOAI.app/base/i18n

copydata-uforadiant:
	@cp -r $(ROOTDIR)/radiant/* UFORadiant.app
	@cp -r $(ROOTDIR)/base/i18n/[^.]* UFOAI.app/base/i18n

# =======================

copynotes-ufoai: package-dir-ufoai
	@cp $(ROOTDIR)/README $(UFOAI_PACKAGE_NAME)
	@cp $(ROOTDIR)/CONTRIBUTORS $(UFOAI_PACKAGE_NAME)
	@cp $(ROOTDIR)/COPYING $(UFOAI_PACKAGE_NAME)

copynotes-uforadiant: package-dir-uforadiant

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

copy-package-bundle-uforadiant: package-dir-uforadiant bundle-uforadiant
	@cp -r UFORadiant.app $(UFORADIANT_PACKAGE_NAME)

# =======================

strip-dev-files-ufoai: copy-package-bundle-ufoai
	@find -d $(UFOAI_PACKAGE_NAME) -name .svn -exec rm -rf {} \;

strip-dev-files-uforadiant: copy-package-bundle-uforadiant
	@find -d $(UFORADIANT_PACKAGE_NAME) -name .svn -exec rm -rf {} \;

# =======================

bundle-ufoai: copybinaries-ufoai copydata-ufoai copylibs-ufoai copynotes-ufoai

bundle-uforadiant: copybinaries-uforadiant copydata-uforadiant copylibs-uforadiant copynotes-uforadiant

# for testing:
bundle-nodata-ufoai: copybinaries-ufoai copylibs-ufoai copynotes-ufoai

# =======================

create-dmg-ufoai: bundle-ufoai updateversion-ufoai copy-package-bundle-ufoai strip-dev-files-ufoai
	@rm -f $(UFOAI_PACKAGE_NAME).dmg
	@hdiutil create -srcfolder $(UFOAI_PACKAGE_NAME) $(UFOAI_PACKAGE_NAME).dmg

create-dmg-uforadiant: bundle-uforadiant updateversion-uforadiant copy-package-bundle-uforadiant strip-dev-files-uforadiant
	@rm -f $(UFORADIANT_PACKAGE_NAME).dmg
	@hdiutil create -srcfolder $(UFORADIANT_PACKAGE_NAME) $(UFORADIANT_PACKAGE_NAME).dmg

create-dmg: create-dmg-ufoai create-dmg-uforadiant

