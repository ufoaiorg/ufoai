all: update_installer_data build_dmg

EXT=pk3
ROOTDIR=../../../../
#relative from root dir
INSTDIR=src/ports/macosx/installer/
#get the version of ufo
VERSION_UC=$(shell grep UFO_VERSION $(ROOTDIR)src/common/common.h | sed -e 's/.*UFO_VERSION\s*\(.*\)/\1/' | sed -e 's/\"//g')
VERSION_LC=$(shell echo $(VERSION_UC) | sed -e 'y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/;')

PACKAGE_NAME=ufoai-$(VERSION_LC)-macosx-$(TARGET_CPU)

BINARIES = \
	$(ROOTDIR)ufo \
	$(ROOTDIR)ufoded \
	$(ROOTDIR)ufo2map \
	$(ROOTDIR)base/game.dylib

build_dmg:
	@mkdir -p UFOAI.app/Contents/MacOS
	@mkdir -p UFOAI.app/Contents/Frameworks
	@mkdir -p UFOAI.app/Contents/Libraries
	@mkdir -p UFOAI.app/base
	@cp $(BINARIES) UFOAI.app
	@cp -v $(ROOTDIR)base/*.$(EXT) UFOAI.app/base/
#	for dir in $(ROOTDIR)base/i18n/*; do \
#		mkdir -p UFOAI.app/base/i18n/$$dir/LC_MESSAGES; \
#		cp $(ROOTDIR)base/$$dir/LC_MESSAGES/ufoai.mo UFOAI.app/base/$$dir/LC_MESSAGES/ufoai.mo; \
#	done
	@cp -r $(ROOTDIR)base/i18n UFOAI.app/base/i18n
	@perl macfixlibs.pl
	@rm -f $(PACKAGE_NAME).dmg
	@cp $(ROOTDIR)README UFOAI.app
	@cp $(ROOTDIR)CONTRIBUTORS UFOAI.app
	@cp $(ROOTDIR)COPYING UFOAI.app
	@hdiutil create -srcfolder UFOAI.app $(PACKAGE_NAME).dmg

update_installer_data:
	@sed 's/@VERSION@/$(VERSION_UC)/g' UFOAI.app/Contents/Info.plist.in > UFOAI.app/Contents/Info.plist
