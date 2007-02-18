VERSION=$(shell grep UFO_VERSION src/qcommon/qcommon.h | sed -e 's/.*UFO_VERSION \"\(.*\)\"/\1/')

installer: win32installer linuxinstaller sourcearchive
mappack:
	tar -cvjp --exclude-from=src/ports/linux/tar.ex -f ufoai-$(VERSION)-mappack.tar.bz2 ./base/maps

win32installer:
	makensis src/ports/win32/installer.nsi

linuxarchive:
	tar -cvjp --exclude-from=src/ports/linux/tar.ex -f ufoai-$(VERSION)-linux.tar.bz2 ./

linuxinstaller:
	$(MAKE) -f src/ports/linux/installer/Makefile

sourcearchive:
	mkdir -p ./ufoai-$(VERSION)-source
	mkdir -p tarsrc
	cp -rf * ./ufoai-$(VERSION)-source
	mv ./ufoai-$(VERSION)-source  ./tarsrc
	tar -cj \
		-X src/ports/linux/tar.ex \
		-f ufoai-$(VERSION)-source.tar.bz2 \
		-C tarsrc ufoai-$(VERSION)-source
	rm -rf ./tarsrc

packfiles:
	cd base; zip -r 0textures.zip textures -x \*.svn\*
	cd base; zip -r 0sounds.zip sound -x \*.svn\*
	cd base; zip -r 0pics.zip pics -x \*.svn\*
	cd base; zip -r 0music.zip music -x \*.svn\*
	cd base; zip -r 0maps.zip maps -x \*.svn\*
	cd base; zip -r 0models.zip models -x \*.svn\*
	cd base; zip -r 0ufos.zip ufos -x \*.svn\*

