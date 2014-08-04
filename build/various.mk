ifeq ($(wildcard .git),.git)
	GIT_REV2=-$(shell LANG=C git rev-parse HEAD)
	ifeq ($(GIT_REV2),-)
		GIT_REV2=
	endif
endif

# auto generate icon packs
resources:
	$(Q)$(PROGRAM_PYTHON) ./src/resources/sprite_pack.py

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
	$(Q)$(PROGRAM_PYTHON) contrib/licenses/generate.py

doxygen:
	$(Q)doxygen src/docs/doxywarn

doxygen-clean:
	$(Q)rm -rf ./src/docs/doxygen

pngcrush:
	@s="${SINCE}" ;\
	test -z "$$s" && s="$$(git log --grep=crush --pretty=format:%H|head -n 1)" ;\
	echo "Crushing images introdced since $$s" ;\
	git diff --diff-filter=AM --name-only "$$s" HEAD|grep '.png$$' \
	| xargs -I {} sh -c 'pngcrush -brute -rem alla {} /tmp/crush.png; mv -f /tmp/crush.png {}'

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
	@echo " * pngcrush     - Optimize all png files incrementally since the last crush"
	@echo " -----"
	@echo " * clean        - Removes binaries, no datafiles (e.g. maps)"
	@echo " * clean-maps   - Removes compiled maps (bsp files)"
	@echo " * clean-pk3    - Removes the pk3 archives"
	@echo " * clean-docs   - Removes the doxygen docs and manual"

ISSUE_FILES=$$(find . -type f '(' -name '*.cpp' -or -name '*.h' -or -name '*.m' -or -name '*.mm' -or -name '*.c' ')' )
ISSUE_TYPES=deprecated todo xxx fixme

local-issues: $(addprefix local-,${ISSUE_TYPES})
$(addprefix local-,${ISSUE_TYPES}):
	@itype="$@" ; \
	itype="$${itype#local-}" ; \
	git diff | grep --color=auto -in -e "$${itype}:" -e "@$${itype}" -e "$${itype} "- || true

issues: ${ISSUE_TYPES}
${ISSUE_TYPES}:
	@grep --color=auto -in -e '$@:' -e '@$@' -e '$@ ' ${ISSUE_FILES} || true

.PHONY: monthly
monthly:
	$(Q)MONTH=$$(LOCALE=C date '+%m'); \
	MONTHLY_DATE_START=$$(LANG=C date -d "$${MONTH}/1 - 1 month"); \
	MONTHLY_DATE_END=$$(LANG=C date -d "$${MONTH}/1 - 1 sec"); \
	COMMITS=$$(git log --since="$${MONTHLY_DATE_START}" --until="$${MONTHLY_DATE_END}" --format="%s" | wc -l); \
	MONTH=$$(LANG=C date -d "$${MONTH}/1 - 1 month" '+%B'); \
	YEAR=$$(LANG=C date '+%Y'); \
	echo "show commits from '$${MONTHLY_DATE_START}' until '$${MONTHLY_DATE_END}'"; \
	echo "{{news"; \
	echo "|title=Monthly update for $${MONTH}, $${YEAR}"; \
	echo "|author=$${USER}"; \
	echo "|date=$$(LANG=C date '+%Y-%m-%d')"; \
	echo "|content="; \
	git log --since="$${MONTHLY_DATE_START}" --until="$${MONTHLY_DATE_END}" --format="%s" | sed 's/\(.*\)bug #\([0-9]\+\)\(.*\)/\1{{bug|\2}}\3/g'; \
	echo "" ; \
	echo "" ; \
	echo "In total, $${COMMITS} commits were made in the UFO:AI repository in $${MONTH}." ;\
	echo "}}";
