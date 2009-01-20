
UFOAI_POFILES = $(wildcard src/po/ufoai-*.po)
RADIANT_POFILES = $(wildcard src/po/uforadiant-*.po)

UFOAI_MOFILES = $(patsubst src/po/ufoai-%.po, base/i18n/%/LC_MESSAGES/ufoai.mo, $(UFOAI_POFILES))
RADIANT_MOFILES = $(patsubst src/po/uforadiant-%.po, radiant/i18n/%/LC_MESSAGES/uforadiant.mo, $(RADIANT_POFILES))

$(UFOAI_MOFILES) : base/i18n/%/LC_MESSAGES/ufoai.mo : src/po/ufoai-%.po
	@mkdir -p $(dir $@)
	msgfmt -v -o $@ $^

$(RADIANT_MOFILES) : radiant/i18n/%/LC_MESSAGES/uforadiant.mo : src/po/uforadiant-%.po
	@mkdir -p $(dir $@)
	msgfmt -v -o $@ $^

lang: $(UFOAI_MOFILES) $(RADIANT_MOFILES)

update-po:
	$(MAKE) -C src/po update-po

update-po-radiant:
	xgettext -j --keyword="_" --keyword="C_:1c,2" -C -o src/po/uforadiant.pot --omit-header \
		src/tools/radiant/radiant/dialogs/*.cpp \
		src/tools/radiant/radiant/dialogs/*.h \
		src/tools/radiant/radiant/sidebar/*.cpp \
		src/tools/radiant/radiant/sidebar/*.h \
		src/tools/radiant/radiant/*.cpp \
		src/tools/radiant/radiant/*.h

po-sync:
	@echo "This will sync all po files with the wiki articles - run update-po before this step"
	@echo "Gamers don't have to do this - translators should use ./src/po/update_po_from_wiki <lang> directly"
	@echo "Hit any key if you are sure you really want to start the sync"
	@pofiles='$(UFOAI_POFILES)'; \
	read enter; cd src/po; \
	for po in $$pofiles; do \
	  po=`basename $$po .po`; \
	  echo $$po; \
	  ./update_po_from_wiki.sh $$po; \
	done
