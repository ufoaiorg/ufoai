VERSION=$(shell grep UFO_VERSION src/qcommon/qcommon.h | sed -e 's/.*UFO_VERSION \"\(.*\)\"/\1/')

installer: win32installer linuxinstaller sourcearchive

mappack:
	tar -cvjp --exclude-from=src/ports/linux/tar.ex -f ufoai-$(VERSION)-mappack.tar.bz2 ./base/maps

win32installer:
	makensis src/ports/win32/installer.nsi

win32installerarchive:
	makensis src/ports/win32/installer_archive.nsi

linuxarchive:
	tar -cvjp --exclude-from=src/ports/linux/tar.ex -f ufoai-$(VERSION)-linux.tar.bz2 ./

linuxinstaller:
	$(MAKE) -f src/ports/linux/installer/Makefile

#
# Generate a tar archive of the sources.
#
sourcearchive:
# Create the tarsrc/ufoai-$(VERSION)-source directory in order that the
# resulting tar archive extracts to one directory.
	mkdir -p ./tarsrc
	ln -fsn ../ tarsrc/ufoai-$(VERSION)-source
# Take a list of files from SVN. Trim SVN's output to include only the filenames
# and paths. Then feed that list to tar.
	svn status -v|grep -v "^?"|cut -c 7-|awk '{print $$4}'|sed "s/^/ufoai-$(VERSION)-source\//"> ./tarsrc/filelist
# Also tell tar to exclude base/ and contrib/ directories.
	tar -cvjh --no-recursion	\
		-C ./tarsrc				\
		--exclude "*base/*"		\
		--exclude "*contrib*"	\
		-T ./tarsrc/filelist	\
		-f ./tarsrc/ufoai-$(VERSION)-source.tar.bz2
	mv ./tarsrc/ufoai-$(VERSION)-source.tar.bz2 ./
	rm -rf ./tarsrc

# this done by base/archives.sh
#packfiles:
#	cd base; zip -r 0textures.zip textures -x \*.svn\*
#	cd base; zip -r 0sounds.zip sound -x \*.svn\*
#	cd base; zip -r 0pics.zip pics -x \*.svn\*
#	cd base; zip -r 0music.zip music -x \*.svn\*
#	cd base; zip -r 0maps.zip maps -x \*.svn\*
#	cd base; zip -r 0models.zip models -x \*.svn\*
#	cd base; zip -r 0ufos.zip ufos -x \*.svn\*

