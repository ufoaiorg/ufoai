linuxinstaller: lang maps-sync pk3 packdata update_installer_data makeself

EXT=pk3
ROOTDIR=.
DIR=contrib/installer/linux
#everything inside this dir will be compressed to the self extracting archive
DATADIR=$(DIR)/data
#this is only for arranging the path names and tar the zip files
#this step is needed - otherwise loki_setup would automatically extract
#all zip files
TEMPDIR=$(DIR)/tmp
#get the size of the zip data archives
SIZE=$(shell LANG=C; du -sch $(TEMPDIR)/base/*.$(EXT) $(BINARIES) | tail -1 | cut -f1;)

#grab the gtkradiant 1.5 archive from this url
GTKRADIANT_URL=http://mattn.ninex.info/download/gtkradiant.tar.bz2

BINARIES = \
	$(ROOTDIR)/ufo \
	$(ROOTDIR)/ufoded \
	$(ROOTDIR)/ufo2map \
	$(ROOTDIR)/base/game.so

BINARIES_64 = \
	$(ROOTDIR)/ufo.x86_64 \
	$(ROOTDIR)/ufoded.x86_64 \
	$(ROOTDIR)/ufo2map.x86_64 \
	$(ROOTDIR)/base/game_x86_64.so

packdata:
	@mkdir -p $(TEMPDIR)/base
	@tar -cvjp -f $(DATADIR)/ufo-x86.tar.bz2 $(BINARIES)
	@tar -cvjp -f $(DATADIR)/ufo-x86_64.tar.bz2 $(BINARIES_64)
	@cd $(ROOTDIR); tar -cvjp -f $(DIR)$(DATADIR)/i18n.tar.bz2 $(ROOTDIR)/base/i18n/ --exclude .svn --exclude updated*
# FIXME Use find -name "*.bsp" and exclude invalid files like autosaves and so on
	cd $(ROOTDIR); tar -cvjp -f $(DIR)/$(DATADIR)/mapsource.tar.bz2 $(ROOTDIR)/base/maps/ --exclude .svn --exclude *.bsp --exclude *.autosave* --exclude *.bak
	@cp -u $(ROOTDIR)base/*.$(EXT) $(TEMPDIR)/base
	@cd $(TEMPDIR); tar -cvp -f ../$(DATADIR)/data.tar $(ROOTDIR)/base
# FIXME: Get the 64bit binary, too
	@cd $(DATADIR); wget -nc $(GTKRADIANT_URL)

update_installer_data:
	@sed 's/@VERSION@/$(UFOAI_VERSION)/g' $(DIR)/setup.xml.in | sed 's/@SIZE@/$(SIZE)/g' > $(DATADIR)/setup.data/setup.xml
	@sed 's/@VERSION@/$(UFOAI_VERSION)/g' $(DIR)/README.in > $(DATADIR)/setup.data/README

makeself:
	@cd $(DIR); ./makeself.sh --nocomp $(DATADIR)/ ufoai-$(UFOAI_VERSION)-linux.run "UFO:Alien Invasion $(UFOAI_VERSION) Installer" sh setup.sh
