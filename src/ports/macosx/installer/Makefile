#all: update_installer_data build_dmg
all: create-dmg

EXT=pk3
ROOTDIR=../../../..
#relative from root dir
INSTDIR=src/ports/macosx/installer/
#get the version of ufo
VERSION_UC=$(shell grep UFO_VERSION $(ROOTDIR)/src/common/common.h | sed -e 's/.*UFO_VERSION\s*\(.*\)/\1/' | sed -e 's/\"//g')
VERSION_LC=$(shell echo $(VERSION_UC) | sed -e 'y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/;')

PACKAGE_NAME=ufoai-$(VERSION_LC)-macosx-$(TARGET_CPU)

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

bundle_dirs:
	@mkdir -p UFOAI.app/base
	@mkdir -p UFOAI.app/radiant

$(BASE_PK3) :
	$(MAKE) -C $(ROOTDIR) $(addprefix base/,$(notdir $@))

$(BUNDLE_PK3) : UFOAI.app/base/%.pk3 : $(ROOTDIR)/base/%.pk3
	cp -v $< $@

package_dir:
	@rm -rf $(PACKAGE_NAME)
	@mkdir -p $(PACKAGE_NAME)

updateversion:
	@sed 's/@VERSION@/$(VERSION_UC)/g' UFOAI.app/Contents/Info.plist.in > UFOAI.app/Contents/Info.plist

copyradiant:
	@cp -r $(ROOTDIR)/radiant UFOAI.app

copybinaries: bundle_dirs
	@cp $(BINARIES) UFOAI.app/Contents/MacOS
	@cp $(BINARIES_RADIANT) UFOAI.app/Contents/MacOS
	@cp $(BINARIES_BASE) UFOAI.app/base

copydata: bundle_dirs $(BUNDLE_PK3)
	@mkdir -p UFOAI.app/base/i18n
	@cp -r $(ROOTDIR)/base/i18n/[^.]* UFOAI.app/base/i18n

copynotes: package_dir
	@cp $(ROOTDIR)/README $(PACKAGE_NAME)
	@cp $(ROOTDIR)/CONTRIBUTORS $(PACKAGE_NAME)
	@cp $(ROOTDIR)/COPYING $(PACKAGE_NAME)

copylibs:
	@rm -rf UFOAI.app/Contents/Frameworks/*.framework
	@perl macfixlibs.pl

copy_package_bundle: package_dir bundle
	@cp -r UFOAI.app $(PACKAGE_NAME)

strip_dev_files: copy_package_bundle
	@find -d $(PACKAGE_NAME) -name .svn -exec rm -rf {} \;

bundle: copybinaries copydata copyradiant copylibs copynotes

# for testing:
bundle-nodata: copybinaries copylibs copynotes

create-dmg: bundle updateversion copy_package_bundle strip_dev_files
	@rm -f $(PACKAGE_NAME).dmg
	@hdiutil create -srcfolder $(PACKAGE_NAME) $(PACKAGE_NAME).dmg

build_dmg: copybinaries copylibs $(BUNDLE_PK3)
#	for dir in $(ROOTDIR)/base/i18n/[^.]*; do \
#		mkdir -p UFOAI.app/base/i18n/$$dir/LC_MESSAGES; \
#		cp $(ROOTDIR)/base/$$dir/LC_MESSAGES/ufoai.mo UFOAI.app/base/$$dir/LC_MESSAGES/ufoai.mo; \
#	done
	@cp -r $(ROOTDIR)/base/i18n/[^.]* UFOAI.app/base/i18n
	@rm -f $(PACKAGE_NAME).dmg
	@cp $(ROOTDIR)/README UFOAI.app
	@cp $(ROOTDIR)/CONTRIBUTORS UFOAI.app
	@cp $(ROOTDIR)/COPYING UFOAI.app
	@hdiutil create -srcfolder UFOAI.app $(PACKAGE_NAME).dmg

update_installer_data:
	@sed 's/@VERSION@/$(VERSION_UC)/g' UFOAI.app/Contents/Info.plist.in > UFOAI.app/Contents/Info.plist
