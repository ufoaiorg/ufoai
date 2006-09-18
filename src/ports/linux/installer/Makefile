all: packdata update_installer_data makeself

ROOTDIR=../../../../

VERSION_UC=$(shell grep UFO_VERSION $(ROOTDIR)src/qcommon/qcommon.h | sed -e 's/.*UFO_VERSION\s*\(.*\)/\1/' | sed -e 's/\"//g')

VERSION_LC=$(shell echo $(VERSION_UC) | sed -e 'y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/;')

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
	tar -cvjp -f data/ufo-x86.tar.bz2 $(BINARIES)
	tar -cvjp -f data/ufo-x86_64.tar.bz2 $(BINARIES)
	tar -cvjp --exclude-from=../tar.ex -f data/data.tar.bz2 ../../../../base

update_installer_data:
	sed 's/@VERSION@/$(VERSION_UC)/g' setup.xml.in > data/setup.data/setup.xml
	sed 's/@VERSION@/$(VERSION_UC)/g' README.in > data/setup.data/README

makeself:
	./makeself.sh --nocomp data/ ufoai-$(VERSION_LC)-linux.run "UFO:Alien Invasion $(VERSION_UC) Installer" sh setup.sh
