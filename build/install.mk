VERSION=$(shell grep UFO_VERSION src/common/common.h | sed -e 's/.*UFO_VERSION \"\(.*\)\"/\1/')
RADIANT_VERSION=1.5

installer: wininstaller linuxinstaller sourcearchive mappack

mappack: maps
	tar -cvjp --exclude-from=src/ports/linux/tar.ex -f ufoai-$(VERSION)-mappack.tar.bz2 ./base/maps
	scp ufoai-$(VERSION)-mappack.tar.bz2 ufo:~/public_html/download

wininstaller: lang maps pk3
	makensis src/ports/windows/installer.nsi
	md5sum src/ports/windows/ufoai-$(VERSION)-win32.exe > src/ports/windows/ufoai-$(VERSION)-win32.md5

dataarchive: pk3
	tar -cvp -f ufoai-$(VERSION)-data.tar base/*.pk3

linuxinstaller: lang maps pk3
	cd src/ports/linux/installer; $(MAKE) packdata; $(MAKE)

macinstaller: lang pk3
	# Replacing existing compiled maps with downloaded precompiled maps,
	# otherwise multiplayer won't work due to mismatching checksums
	# FIXME: Use the maps from the current release at sourceforge.net
	# The mapfile in the address below is not recent enough to be useful in trunk.
	# Removed for trunk builds but should be added for branch builds with the
	# correct 0maps.pk3 linked.
	# cd base; wget -N http://mattn.ninex.info/download/0maps.pk3
	cd src/ports/macosx/installer; $(MAKE) TARGET_CPU=universal

USER=tlh2000
upload-sf:
	rsync -avP -e ssh uforadiant-$(RADIANT_VERSION)-macosx-universal.dmg $(USER)@frs.sourceforge.net:uploads/
	rsync -avP -e ssh ufoai-$(VERSION)-macosx-universal.dmg $(USER)@frs.sourceforge.net:uploads/
	rsync -avP -e ssh ufoai-$(VERSION)-source.tar.bz2 $(USER)@frs.sourceforge.net:uploads/
	rsync -avP -e ssh ufoai-$(VERSION)-linux.run $(USER)@frs.sourceforge.net:uploads/
	rsync -avP -e ssh ufoai-$(VERSION)-win32.exe $(USER)@frs.sourceforge.net:uploads/
	rsync -avP -e ssh ufoai-$(VERSION)-data.tar $(USER)@frs.sourceforge.net:uploads/

upload-mirror:
	scp src/ports/macosx/installer/ufoai-$(VERSION)-macosx-universal.dmg ufo:~/public_html/download
	scp src/ports/macosx/installer/uforadiant-$(RADIANT_VERSION)-macosx-universal.dmg ufo:~/public_html/download
	scp src/ports/linux/installer/ufoai-$(VERSION)-linux.run ufo:~/public_html/download
	scp src/ports/windows/ufoai-$(VERSION)-win32.exe src/ports/windows/ufoai-$(VERSION)-win32.md5 ufo:~/public_html/download
	scp src/ports/macosx/installer/ufoai-$(VERSION)-macosx-universal.dmg mirror:~/public_html
	scp src/ports/macosx/installer/uforadiant-$(RADIANT_VERSION)-macosx-universal.dmg mirror:~/public_html
	scp src/ports/linux/installer/ufoai-$(VERSION)-linux.run mirror:~/public_html
	scp src/ports/windows/ufoai-$(VERSION)-win32.exe src/ports/windows/ufoai-$(VERSION)-win32.md5 mirror:~/public_html

create-release: dataarchive wininstaller linuxinstaller macinstaller sourcearchive upload-sf
create-dev: dataarchive wininstaller linuxinstaller macinstaller sourcearchive upload-mirror

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
