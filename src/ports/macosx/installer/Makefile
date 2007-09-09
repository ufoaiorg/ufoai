all: update_installer_data

EXT=pk3
ROOTDIR=../../../../
#relative from root dir
INSTDIR=src/ports/macosx/installer/
#everything inside this dir will be compressed to the self extracting archive
DATADIR=data
#this is only for arranging the path names and tar the zip files
#this step is needed - otherwise loki_setup would automatically extract
#all zip files
TEMPDIR=UfoApp.app/Contents/MacOS
#get the version of ufo
VERSION_UC=$(shell grep UFO_VERSION $(ROOTDIR)src/common/common.h | sed -e 's/.*UFO_VERSION\s*\(.*\)/\1/' | sed -e 's/\"//g')
VERSION_LC=$(shell echo $(VERSION_UC) | sed -e 'y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/;')
#get the size of the zip data archives
SIZE=$(shell LANG=C; du -sch $(TEMPDIR)/base/*.$(EXT) $(BINARIES) | tail -1 | cut -f1;)

#grab the gtkradiant 1.5 archive from this url

BINARIES = \
	$(ROOTDIR)ufo \
	$(ROOTDIR)ufomaster \
	$(ROOTDIR)ufoded \
	$(ROOTDIR)ufo2map \
	$(ROOTDIR)snd_alsa.so \
	$(ROOTDIR)snd_arts.so \
	$(ROOTDIR)snd_jack.so \
	$(ROOTDIR)snd_oss.so \
	$(ROOTDIR)snd_sdl.so \
	$(ROOTDIR)base/game.so

packdata:
	@mkdir -p $(TEMPDIR)/base
	@cd $(ROOTDIR); tar -cvjp -f $(INSTDIR)$(DATADIR)/i18n.tar.bz2 base/i18n/ --exclude .svn --exclude updated*
	@cd $(ROOTDIR)base; ./archives.sh $(EXT)
	@cp -u $(ROOTDIR)base/*.$(EXT) $(TEMPDIR)/base

update_installer_data:
	@sed 's/@VERSION@/$(VERSION_UC)/g' UfoApp.app/Contents/Info.plist.in > UfoApp.app/Contents/Info.plist

