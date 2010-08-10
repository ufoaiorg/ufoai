ifeq ($(wildcard .svn),.svn)
	SVN_REV2=-$(shell LANG=C svn info | awk '$$1 == "Revision:" {print $$2; exit 0}')
	ifeq ($(SVN_REV2),-)
		SVN_REV2=
	endif
endif

# sync sourceforget.net svn to local svn dir
LOCAL_SVN_DIR ?= /var/lib/svn
rsync:
	rsync -avz ufoai.svn.sourceforge.net::svn/ufoai/* $(LOCAL_SVN_DIR)

# generate doxygen docs
doxygen-docs:
	doxygen src/docs/doxyall

# debian packages
deb-pre:
	sed 's/-SVNREVISION/$(SVN_REV2)/' debian/changelog.in > debian/changelog

deb: deb-pre
	debuild binary

pdf-manual:
	$(MAKE) -C src/docs/tex

license-generate:
	@mkdir -p licenses/html
	@python contrib/licenses/generate.py -u $(readlink -f licenses/html)/

help:
	@echo "Makefile targets:"
	@echo " -----"
	@echo " * deb          - Builds a debian package"
	@echo " * lang         - Compiles the language files"
	@echo " * maps         - Compiles the maps"
	@echo " * models       - Compiles the model mdx files (faster model loading)"
	@echo " * pk3          - Generate the pk3 archives for the installers"
	@echo " * rsync        - Creates a local copy of the whole svn (no checkout)"
	@echo " * update-po    - Updates the po files with latest source and script files"
	@echo " -----"
	@echo " * clean        - Removes binaries, no datafiles (e.g. maps)"
	@echo " * clean-maps   - Removes compiled maps (bsp files)"
	@echo " * clean-pk3    - Removes the pk3 archives"
