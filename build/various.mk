ifeq ($(wildcard .git),.git)
	GIT_REV2=-$(shell LANG=C git rev-parse HEAD)
	ifeq ($(GIT_REV2),-)
		GIT_REV2=
	endif
endif

# auto generate icon packs
resources:
	$(Q)python ./src/resources/sprite_pack.py

clean-resources:
	$(Q)rm ./base/pics/banks/autogen*
	$(Q)rm ./base/ufos/*_autogen.ufo

clean-docs: manual-clean doxygen-clean

# sync sourceforget.net git into a local dir
LOCAL_GIT_DIR ?= ~/ufoai-git.sf
rsync:
	$(Q)rsync -av ufoai.git.sourceforge.net::gitroot/ufoai/* $(LOCAL_GIT_DIR)

# generate doxygen docs
doxygen-docs:
	$(Q)doxygen src/docs/doxyall

manual:
	$(Q)make -C src/docs/tex

manual-clean:
	$(Q)make -C src/docs/tex clean

# debian packages
deb:
	$(Q)debuild binary

license-generate:
	$(Q)python contrib/licenses/generate.py

doxygen:
	$(Q)doxygen src/docs/doxywarn

doxygen-clean:
	$(Q)rm -rf ./src/docs/doxygen

help:
	@echo "Makefile targets:"
	@echo " -----"
	@echo " * deb          - Builds a debian package"
	@echo " * doxygen      - Compiles the developer code documentation"
	@echo " * lang         - Compiles the language files"
	@echo " * manual       - Build the user manual(s)"
	@echo " * maps         - Compiles the maps"
	@echo " * models       - Compiles the model mdx files (faster model loading)"
	@echo " * pk3          - Generate the pk3 archives for the installers"
	@echo " * rsync        - Creates a local copy of the whole repository (no checkout)"
	@echo " * update-po    - Updates the po files with latest source and script files"
	@echo " -----"
	@echo " * clean        - Removes binaries, no datafiles (e.g. maps)"
	@echo " * clean-maps   - Removes compiled maps (bsp files)"
	@echo " * clean-pk3    - Removes the pk3 archives"
	@echo " * clean-docs   - Removes the doxygen docs and manual"
