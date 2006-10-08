VERSION=$(shell grep UFO_VERSION src/qcommon/qcommon.h | sed -e 's/.*UFO_VERSION \"\(.*\)\"/\1/')

installer: win32installer linuxinstaller sourcearchive
mappack:
	tar -cvjp --exclude-from=src/ports/linux/tar.ex -f ufoai-$(VERSION)-mappack.tar.bz2 ./base/maps

win32installer:
	makensis src/ports/win32/installer.nsi

linuxinstaller:
	tar -cvjp --exclude-from=src/ports/linux/tar.ex -f ufoai-$(VERSION)-linux.tar.bz2 ./

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
