all: update_installer_data makeself

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
VERSION_UC=$(shell grep UFO_VERSION $(ROOTDIR)src/qcommon/qcommon.h | sed -e 's/.*UFO_VERSION\s*\(.*\)/\1/' | sed -e 's/\"//g')
VERSION_LC=$(shell echo $(VERSION_UC) | sed -e 'y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/;')
#get the size of the zip data archives
SIZE=$(shell LANG=C; du -sch $(TEMPDIR)/base/*.zip $(BINARIES) | tail -1 | cut -f1;)

#grab the gtkradiant 1.5 archive from this url
GTKRADIANT_URL=http://mattn.ninex.info/download/gtkradiant.tar.bz2

BINARIES = \
	$(ROOTDIR)ufo \
	$(ROOTDIR)ufoded \
	$(ROOTDIR)ufo2map \
	$(ROOTDIR)ref_glx.so \
	$(ROOTDIR)ref_sdl.so \
	$(ROOTDIR)snd_alsa.so \
	$(ROOTDIR)snd_oss.so \
	$(ROOTDIR)snd_sdl.so \
	$(ROOTDIR)base/game.so

BINARIES_64 = \
	$(ROOTDIR)x86_64/ufo \
	$(ROOTDIR)x86_64/ufoded \
	$(ROOTDIR)x86_64/ufo2map \
	$(ROOTDIR)x86_64/ref_glx.so \
	$(ROOTDIR)x86_64/ref_sdl.so \
	$(ROOTDIR)x86_64/snd_alsa.so \
	$(ROOTDIR)x86_64/snd_oss.so \
	$(ROOTDIR)x86_64/snd_sdl.so \
	$(ROOTDIR)x86_64/base/game.so

packdata:
	@mkdir -p $(TEMPDIR)/base
	@tar -cvjp -f $(DATADIR)/ufo-x86.tar.bz2 $(BINARIES)
	@tar -cvjp -f $(DATADIR)/ufo-x86_64.tar.bz2 $(BINARIES)
	@cd $(ROOTDIR); tar -cvjp -f $(INSTDIR)$(DATADIR)/i18n.tar.bz2 base/i18n/ --exclude .svn --exclude updated*
	cd $(ROOTDIR); tar -cvjp -f $(INSTDIR)$(DATADIR)/mapsource.tar.bz2 base/maps/ --exclude .svn --exclude *.bsp --exclude *.autosave* --exclude *.bak
	@cd $(ROOTDIR)base; ./archives.sh
	@cp -u $(ROOTDIR)base/*.zip $(TEMPDIR)/base
	@cd $(TEMPDIR); tar -cvp -f ../$(DATADIR)/data.tar base
	@wget -nc $(GTKRADIANT_URL)

update_installer_data:
	@sed 's/@VERSION@/$(VERSION_UC)/g' setup.xml.in | sed 's/@SIZE@/$(SIZE)/g' > $(DATADIR)/setup.data/setup.xml
	@sed 's/@VERSION@/$(VERSION_UC)/g' README.in > $(DATADIR)/setup.data/README

makeself:
	./makeself.sh --nocomp $(DATADIR)/ ufoai-$(VERSION_LC)-linux.run "UFO:Alien Invasion $(VERSION_UC) Installer" sh setup.sh
