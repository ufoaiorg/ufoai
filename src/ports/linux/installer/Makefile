all: packdata update_installer_data makeself

ROOTDIR=../../../../

VERSION=$(shell grep UFO_VERSION $(ROOTDIR)src/qcommon/qcommon.h | sed -e 's/.*UFO_VERSION\s*\(.*\)/\1/')

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

packdata:
	tar -cvjp -f data/binaries.tar.bz2 $(BINARIES)
	tar -cvjp --exclude-from=../tar.ex -f data/data.tar.bz2 ../../../../base

update_installer_data:
	sed "s/@VERSION@/$VERSION/g" setup.xml.in > data/setup.data/setup.xml
	sed "s/@VERSION@/$VERSION/g" README.in > data/setup.data/README

makeself:
	./makeself.sh --nocomp data/ ufoai-$(VERSION)-linux.run "UFO:Alien Invasion $(VERSION) Installer" sh setup.sh
