ifeq ($(wildcard .git),.git)
	GIT_REV2=-$(shell LANG=C git rev-parse HEAD)
	ifeq ($(GIT_REV2),-)
		GIT_REV2=
	endif
endif

# sync sourceforget.net git into a local dir
LOCAL_GIT_DIR ?= ~/ufoai-git.sf
rsync:
	rsync -av ufoai.git.sourceforge.net::gitroot/ufoai/* $(LOCAL_GIT_DIR)

# generate doxygen docs
doxygen-docs:
	doxygen src/docs/doxyall

# debian packages
deb:
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
	@echo " * rsync        - Creates a local copy of the whole repository (no checkout)"
	@echo " * update-po    - Updates the po files with latest source and script files"
	@echo " -----"
	@echo " * clean        - Removes binaries, no datafiles (e.g. maps)"
	@echo " * clean-maps   - Removes compiled maps (bsp files)"
	@echo " * clean-pk3    - Removes the pk3 archives"
