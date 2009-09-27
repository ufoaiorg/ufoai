UFOAI_VERSION=$(shell grep UFO_VERSION src/common/common.h | sed -e 's/.*UFO_VERSION \"\(.*\)\"/\1/')
UFORADIANT_VERSION=$(shell grep RADIANT_VERSION src/tools/radiant/include/version.h | sed -e 's/.*RADIANT_VERSION \"\(.*\)\"/\1/')

installer: wininstaller linuxinstaller sourcearchive mappack

mappack: maps
	tar -cvjp --exclude-from=src/ports/linux/tar.ex -f ufoai-$(UFOAI_VERSION)-mappack.tar.bz2 ./base/maps
	scp ufoai-$(UFOAI_VERSION)-mappack.tar.bz2 ufo:~/public_html/download

wininstaller: lang maps pk3
	makensis contrib/installer/ufoai.nsi
	makensis contrib/installer/uforadiant.nsi
	md5sum contrib/installer/ufoai-$(UFOAI_VERSION)-win32.exe > contrib/installer/ufoai-$(UFOAI_VERSION)-win32.md5
	md5sum contrib/installer/uforadiant-$(UFORADIANT_VERSION)-win32.exe > contrib/installer/uforadiant-$(UFORADIANT_VERSION)-win32.md5

dataarchive: pk3
	tar -cvp -f ufoai-$(UFOAI_VERSION)-data.tar base/*.pk3

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
	cd src/ports/macosx/installer; $(MAKE) create-dmg-ufoai TARGET_CPU=$(TARGET_CPU) UFOAI_VERSION=$(UFOAI_VERSION)
	cd src/ports/macosx/installer; $(MAKE) create-dmg-uforadiant TARGET_CPU=$(TARGET_CPU) UFORADIANT_VERSION=$(UFORADIANT_VERSION)

USER=tlh2000
upload-sf:
	rsync -avP -e ssh uforadiant-$(UFORADIANT_VERSION)-macosx-$(TARGET_CPU).dmg $(USER)@frs.sourceforge.net:uploads/
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-macosx-$(TARGET_CPU).dmg $(USER)@frs.sourceforge.net:uploads/
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-source.tar.bz2 $(USER)@frs.sourceforge.net:uploads/
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-linux.run $(USER)@frs.sourceforge.net:uploads/
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-win32.exe $(USER)@frs.sourceforge.net:uploads/
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-data.tar $(USER)@frs.sourceforge.net:uploads/

create-release: dataarchive wininstaller linuxinstaller macinstaller sourcearchive upload-sf
create-dev: dataarchive wininstaller linuxinstaller macinstaller sourcearchive

#
# Generate a tar archive of the sources.
#
sourcearchive:
# Create the tarsrc/ufoai-$(VERSION)-source directory in order that the
# resulting tar archive extracts to one directory.
	mkdir -p ./tarsrc
	ln -fsn ../ tarsrc/ufoai-$(UFOAI_VERSION)-source
# Take a list of files from SVN. Trim SVN's output to include only the filenames
# and paths. Then feed that list to tar.
	svn status -v|grep -v "^?"|cut -c 7-|awk '{print $$4}'|sed "s/^/ufoai-$(UFOAI_VERSION)-source\//"> ./tarsrc/filelist
# Also tell tar to exclude base/ and contrib/ directories.
	tar -cvjh --no-recursion	\
		-C ./tarsrc				\
		--exclude "*base/*"		\
		--exclude "*contrib*"	\
		-T ./tarsrc/filelist	\
		-f ./tarsrc/ufoai-$(UFOAI_VERSION)-source.tar.bz2
	mv ./tarsrc/ufoai-$(UFOAI_VERSION)-source.tar.bz2 ./
	rm -rf ./tarsrc
