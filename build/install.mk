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
	base/game.$(SO_EXT)
BINARIES_RADIANT = \
	radiant/uforadiant$(EXE_EXT)

include build/install_android.mk
include build/install_linux.mk
include build/install_mac.mk
include build/install_mojo.mk
include build/install_windows.mk

installer-pre: clean-lang clean-maps clean-pk3 lang maps-sync maps models pk3

installer: wininstaller linuxinstaller mojoinstaller

mappack: maps-sync
	$(Q)git archive --format=tar --prefix=ufoai-$(UFOAI_VERSION)-mappack/ HEAD:base/maps | bzip2 -9 > ufoai-$(UFOAI_VERSION)-mappack.tar.bz2

dataarchive: pk3
	$(Q)tar -cvp -f ufoai-$(UFOAI_VERSION)-data.tar $(PAK_PATHS)

USER?=tlh2000,ufoai
PATH?=/home/frs/project/u/uf/ufoai/"UFO_AI\ 2.x"/$(VERSION)/
UPLOADHOST?=frs.sourceforge.net
URL=$(USER)@$(UPLOADHOST):$(PATH)
upload-sf:
	$(Q)rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-source.tar.bz2 $(URL)
	$(Q)rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-mappack.tar.bz2 $(URL)
	$(Q)rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-linux.run $(URL)
	$(Q)rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-data.tar $(URL)
	$(Q)rsync -avP -e ssh uforadiant-$(UFORADIANT_VERSION)-win32.exe $(URL)
	$(Q)rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-win32.exe $(URL)

upload-sf-mac:
	$(Q)rsync -avP -e ssh uforadiant-$(UFORADIANT_VERSION)-macosx-$(TARGET_CPU).dmg $(URL)
	$(Q)rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-macosx-$(TARGET_CPU).dmg $(URL)

create-release-mac: macinstaller upload-sf-mac
create-release: mappack dataarchive wininstaller linuxinstaller sourcearchive upload-sf
create-dev: mappack dataarchive wininstaller linuxinstaller sourcearchive

#
# Generate a tar archive of the sources. Some stuff is ignored here - see the .gitattributes in the root directory
#
sourcearchive:
	$(Q)git archive --format=tar --prefix=ufoai-$(UFOAI_VERSION)-source/ HEAD | bzip2 -9 > ufoai-$(UFOAI_VERSION)-source.tar.bz2

.PHONY: install-pre
install-pre: pk3 lang
	@echo "Binaries:  $(PKGBINDIR)"
	@echo "Data:      $(PKGDATADIR)"
	@echo "Libraries: $(PKGLIBDIR)"
	@echo "Locales:   $(LOCALEDIR)"
	$(Q)$(INSTALL_DIR) $(PKGDATADIR)/base
	@echo "Install locales"
	$(Q)LCDIR=$(LOCALEDIR); \
	LCDIR=$${LCDIR:-$(PKGDATADIR)/base/i18n}; \
	cd base/i18n/; for dir in *; do \
		$(INSTALL_DIR) $$LCDIR/$$dir/LC_MESSAGES && \
		$(INSTALL_DATA) $$dir/LC_MESSAGES/ufoai.mo $$LCDIR/$$dir/LC_MESSAGES/ufoai.mo; \
	done
	@echo "#!/bin/sh" > ufo.sh
	@echo "cd $(PKGDATADIR); ./ufo \$$*; exit \$$?" >> ufo.sh
	$(Q)$(INSTALL_DIR) $(PKGBINDIR)
	$(Q)$(INSTALL_SCRIPT) ufo.sh $(PKGBINDIR)/ufo
	@echo "#!/bin/sh" > ufoded.sh
	@echo "cd $(PKGDATADIR); ./ufoded \$$*; exit \$$?" >> ufoded.sh
	$(Q)$(INSTALL_SCRIPT) ufoded.sh $(PKGBINDIR)/ufoded
	@echo "cd $(PKGDATADIR)/radiant; ./uforadiant \$$*; exit \$$?" >> uforadiant.sh
	$(Q)$(INSTALL_SCRIPT) uforadiant.sh $(PKGBINDIR)/uforadiant
	$(Q)rm ufoded.sh ufo.sh uforadiant.sh
	@echo "Install pk3s"
	$(Q)$(INSTALL_DATA) base/*.pk3 $(PKGDATADIR)/base
