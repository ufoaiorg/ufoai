all: update_installer_data makeself

EXT=pk3
ROOTDIR=../../../../
#relative from root dir
INSTDIR=src/ports/linux/installer/
#everything inside this dir will be compressed to the self extracting archive
DATADIR=data
#this is only for arranging the path names and tar the zip files
#this step is needed - otherwise loki_setup would automatically extract
#all zip files
TEMPDIR=tmp
#get the version of ufo
VERSION_UC=$(shell grep UFO_VERSION $(ROOTDIR)src/common/common.h | sed -e 's/.*UFO_VERSION\s*\(.*\)/\1/' | sed -e 's/\"//g')
VERSION_LC=$(shell echo $(VERSION_UC) | sed -e 'y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/;')
#get the size of the zip data archives
SIZE=$(shell LANG=C; du -sch $(TEMPDIR)/base/*.$(EXT) $(BINARIES) | tail -1 | cut -f1;)

#grab the gtkradiant 1.5 archive from this url
GTKRADIANT_URL=http://mattn.ninex.info/download/gtkradiant.tar.bz2

BINARIES = \
	$(ROOTDIR)ufo \
	$(ROOTDIR)ufoded \
	$(ROOTDIR)ufo2map \
	$(ROOTDIR)base/game.so

BINARIES_64 = \
	$(ROOTDIR)ufo.x86_64 \
	$(ROOTDIR)ufoded.x86_64 \
	$(ROOTDIR)ufo2map.x86_64 \
	$(ROOTDIR)base/game_x86_64.so

packdata:
	@mkdir -p $(TEMPDIR)/base
	@tar -cvjp -f $(DATADIR)/ufo-x86.tar.bz2 $(BINARIES)
	@tar -cvjp -f $(DATADIR)/ufo-x86_64.tar.bz2 $(BINARIES_64)
	@cd $(ROOTDIR); tar -cvjp -f $(INSTDIR)$(DATADIR)/i18n.tar.bz2 base/i18n/ --exclude .svn --exclude updated*
# FIXME Use find -name "*.bsp" and exclude invalid files like autosaves and so on
	cd $(ROOTDIR); tar -cvjp -f $(INSTDIR)$(DATADIR)/mapsource.tar.bz2 base/maps/ --exclude .svn --exclude *.bsp --exclude *.autosave* --exclude *.bak
	@cp -u $(ROOTDIR)base/*.$(EXT) $(TEMPDIR)/base
	@cd $(TEMPDIR); tar -cvp -f ../$(DATADIR)/data.tar base
# FIXME: Get the 64bit binary, too
	@cd $(DATADIR); wget -nc $(GTKRADIANT_URL)

update_installer_data:
	@sed 's/@VERSION@/$(VERSION_UC)/g' setup.xml.in | sed 's/@SIZE@/$(SIZE)/g' > $(DATADIR)/setup.data/setup.xml
	@sed 's/@VERSION@/$(VERSION_UC)/g' README.in > $(DATADIR)/setup.data/README

makeself:
	./makeself.sh --nocomp $(DATADIR)/ ufoai-$(VERSION_LC)-linux.run "UFO:Alien Invasion $(VERSION_UC) Installer" sh setup.sh
