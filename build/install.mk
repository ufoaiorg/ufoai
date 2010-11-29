UFOAI_VERSION=$(shell grep UFO_VERSION $(SRCDIR)/common/common.h | sed -e 's/.*UFO_VERSION \"\(.*\)\"/\1/')
UFORADIANT_VERSION=$(shell grep RADIANT_VERSION $(SRCDIR)/tools/radiant/include/version.h | sed -e 's/.*RADIANT_VERSION \"\(.*\)\"/\1/')

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
include build/install_windows.mk

installer-pre: lang maps models pk3

installer: wininstaller linuxinstaller sourcearchive mappack

mappack: maps-sync
	tar -cvjp --exclude-from=build/tar.ex -f ufoai-$(UFOAI_VERSION)-mappack.tar.bz2 base/maps

dataarchive: pk3
	tar -cvp -f ufoai-$(UFOAI_VERSION)-data.tar base/*.pk3

USER?=tlh2000,ufoai
PATH?=/home/frs/project/u/uf/ufoai/"UFO_AI\ 2.x"/$(VERSION)/
UPLOADHOST?=frs.sourceforge.net
URL=$(USER)@$(UPLOADHOST):$(PATH)
upload-sf:
	rsync -avP -e ssh uforadiant-$(UFORADIANT_VERSION)-macosx-$(TARGET_CPU).dmg $(URL)
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-macosx-$(TARGET_CPU).dmg $(URL)
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-source.tar.bz2 $(URL)
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-linux.run $(URL)
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-data.tar $(URL)
	rsync -avP -e ssh uforadiant-$(UFORADIANT_VERSION)-win32.exe $(URL)
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-win32.exe $(URL)

create-release: dataarchive wininstaller linuxinstaller macinstaller sourcearchive upload-sf
create-dev: dataarchive wininstaller linuxinstaller macinstaller sourcearchive

#
# Generate a tar archive of the sources.
#
sourcearchive:
	$(Q)git archive --format=tar --prefix=ufoai-$(UFOAI_VERSION)-source/ HEAD | bzip2 -9 > ufoai-$(UFOAI_VERSION)-source.tar.bz2
