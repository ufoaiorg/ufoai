linuxinstaller: installer-pre packdata update_installer_data makeself

LINUX_INST_DIR=contrib/installer/linux
#everything inside this dir will be compressed to the self extracting archive
LINUX_INST_DATADIR=$(LINUX_INST_DIR)/data
#this is only for arranging the path names and tar the zip files
#this step is needed - otherwise loki_setup would automatically extract
#all zip files
LINUX_INST_TMPDIR=$(LINUX_INST_DIR)/tmp
#get the size of the zip data archives
LINUX_INST_SIZE=$(shell LANG=C; du -sch $(LINUX_INST_TMPDIR)/base/*.pk3 $(BINARIES) | tail -1 | cut -f1;)

#grab the gtkradiant 1.5 archive from this url
GTKRADIANT_URL=http://mattn.ninex.info/download/gtkradiant.tar.bz2

BINARIES = \
	ufo \
	ufoded \
	ufo2map \
	base/game.$(SHARED_EXT)

BINARIES_64 = \
	ufo.x86_64 \
	ufoded.x86_64 \
	ufo2map.x86_64 \
	base/game_x86_64.$(SHARED_EXT)

packdata:
	@mkdir -p $(LINUX_INST_TMPDIR)/base
	@tar -cvjp -f $(LINUX_INST_DATADIR)/ufo-x86.tar.bz2 $(BINARIES)
	@tar -cvjp -f $(LINUX_INST_DATADIR)/ufo-x86_64.tar.bz2 $(BINARIES_64)
	@cd $(ROOTDIR); tar -cvjp -f $(LINUX_INST_DIR)$(LINUX_INST_DATADIR)/i18n.tar.bz2 base/i18n/ --exclude .svn --exclude updated*
# FIXME Use find -name "*.bsp" and exclude invalid files like autosaves and so on
	cd $(ROOTDIR); tar -cvjp -f $(LINUX_INST_DIR)/$(LINUX_INST_DATADIR)/mapsource.tar.bz2 base/maps/ --exclude .svn --exclude *.bsp --exclude *.autosave* --exclude *.bak
	@cp -u $(ROOTDIR)base/*.pk3 $(LINUX_INST_TMPDIR)/base
	@cd $(LINUX_INST_TMPDIR); tar -cvp -f ../$(LINUX_INST_DATADIR)/data.tar base
# FIXME: Get the 64bit binary, too
	@cd $(LINUX_INST_DATADIR); wget -nc $(GTKRADIANT_URL)

update_installer_data:
	@sed 's/@VERSION@/$(UFOAI_VERSION)/g' $(LINUX_INST_DIR)/setup.xml.in | sed 's/@LINUX_INST_SIZE@/$(LINUX_INST_SIZE)/g' > $(LINUX_INST_DATADIR)/setup.data/setup.xml
	@sed 's/@VERSION@/$(UFOAI_VERSION)/g' $(LINUX_INST_DIR)/README.in > $(LINUX_INST_DATADIR)/setup.data/README

makeself:
	@cd $(LINUX_INST_DIR); ./makeself.sh --nocomp $(LINUX_INST_DATADIR)/ ufoai-$(UFOAI_VERSION)-linux.run "UFO:Alien Invasion $(UFOAI_VERSION) Installer" sh setup.sh
