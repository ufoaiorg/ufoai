UFOAI_VERSION=$(shell grep UFO_VERSION $(SRCDIR)/common/common.h | sed -e 's/.*UFO_VERSION \"\(.*\)\"/\1/')
UFORADIANT_VERSION=$(shell grep RADIANT_VERSION $(SRCDIR)/tools/radiant/include/version.h | sed -e 's/.*RADIANT_VERSION \"\(.*\)\"/\1/')

include build/install_linux.mk
include build/install_mac.mk
include build/install_windows.mk

installer-pre: lang maps-sync models pk3

installer: wininstaller linuxinstaller sourcearchive mappack

mappack: maps-sync
	tar -cvjp --exclude-from=build/tar.ex -f ufoai-$(UFOAI_VERSION)-mappack.tar.bz2 base/maps

dataarchive: pk3
	tar -cvp -f ufoai-$(UFOAI_VERSION)-data.tar base/*.pk3

USER?=tlh2000
upload-sf:
	rsync -avP -e ssh uforadiant-$(UFORADIANT_VERSION)-macosx-$(TARGET_CPU).dmg $(USER)@frs.sourceforge.net:uploads/
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-macosx-$(TARGET_CPU).dmg $(USER)@frs.sourceforge.net:uploads/
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-source.tar.bz2 $(USER)@frs.sourceforge.net:uploads/
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-linux.run $(USER)@frs.sourceforge.net:uploads/
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-data.tar $(USER)@frs.sourceforge.net:uploads/
	rsync -avP -e ssh uforadiant-$(UFORADIANT_VERSION)-win32.exe $(USER)@frs.sourceforge.net:uploads/
	rsync -avP -e ssh ufoai-$(UFOAI_VERSION)-win32.exe $(USER)@frs.sourceforge.net:uploads/

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
