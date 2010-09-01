UFOAI_VERSION=$(shell grep UFO_VERSION $(SRCDIR)/common/common.h | sed -e 's/.*UFO_VERSION \"\(.*\)\"/\1/')
UFORADIANT_VERSION=$(shell grep RADIANT_VERSION $(SRCDIR)/tools/radiant/include/version.h | sed -e 's/.*RADIANT_VERSION \"\(.*\)\"/\1/')

ROOTDIR=$(shell pwd)
INSTALLER_DIR=$(ROOTDIR)/contrib/installer

BINARIES = \
	ufo$(EXE_EXT) \
	ufoded$(EXE_EXT) \
	ufo2map$(EXE_EXT) \
	ufomodel$(EXE_EXT)
BINARIES_BASE = \
	base/game.$(SHARED_EXT)
BINARIES_RADIANT = \
	radiant/uforadiant$(EXE_EXT)

include build/install_linux.mk
include build/install_mac.mk
include build/install_mojo.mk
include build/install_windows.mk

installer-pre: lang maps models pk3

installer: wininstaller linuxinstaller mojoinstaller

mappack: maps-sync
	tar -cvjp --exclude-from=build/tar.ex -f ufoai-$(UFOAI_VERSION)-mappack.tar.bz2 base/maps

dataarchive: pk3
	tar -cvp -f ufoai-$(UFOAI_VERSION)-data.tar $(PAK_PATHS)

USER?=tlh2000,ufoai
PATH?=/home/frs/project/u/uf/ufoai/"UFO_AI\ 2.x"/$(VERSION)/
UPLOADHOST?=frs.sourceforge.net
URL=$(USER)@$(UPLOADHOST):$(PATH)
upload-sf:
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-source.tar.bz2 $(URL)
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-mappack.tar.bz2 $(URL)
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-linux.run $(URL)
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-data.tar $(URL)
	rsync -avP -e ssh uforadiant-$(UFORADIANT_VERSION)-win32.exe $(URL)
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-win32.exe $(URL)

upload-sf-mac:
	rsync -avP -e ssh uforadiant-$(UFORADIANT_VERSION)-macosx-$(TARGET_CPU).dmg $(URL)
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-macosx-$(TARGET_CPU).dmg $(URL)

create-release-mac: macinstaller upload-sf-mac
create-release: mappack dataarchive wininstaller linuxinstaller sourcearchive upload-sf
create-dev: mappack dataarchive wininstaller linuxinstaller sourcearchive

#
# Generate a tar archive of the sources.
#
sourcearchive:
# Create the tarsrc/ufoai-$(VERSION)-source directory in order that the
# resulting tar archive extracts to one directory.
	mkdir -p ./tarsrc
	ln -fsn ../ tarsrc/ufoai-$(UFOAI_VERSION)-source
# Take a list of files from SVN. Trim SVN's output to include only the filenames
# and paths. Then feed that list to tar.
	svn status -v|grep -v "^?"|cut -c 7-|awk '{print $$4}'|sed "s/^/ufoai-$(UFOAI_VERSION)-source\//"> ./tarsrc/filelist
# Also tell tar to exclude base/ and contrib/ directories.
	tar -cvjh --no-recursion	\
		-C ./tarsrc				\
		--exclude "*base/*"		\
		--exclude "*contrib*"	\
		-T ./tarsrc/filelist	\
		-f ./tarsrc/ufoai-$(UFOAI_VERSION)-source.tar.bz2
	mv ./tarsrc/ufoai-$(UFOAI_VERSION)-source.tar.bz2 ./
	rm -rf ./tarsrc
