
POFILES = $(wildcard src/po/*.po)

MOFILES = $(patsubst src/po/%.po, base/i18n/%/LC_MESSAGES/ufoai.mo, $(POFILES))

$(MOFILES) : base/i18n/%/LC_MESSAGES/ufoai.mo : src/po/%.po
	@mkdir -p $(dir $@)
	msgfmt -v -o $@ $^

lang: $(MOFILES)

update-po:
	$(MAKE) -C src/po update-po

update-po-radiant:
	xgettext -j --keyword="_" -C -o src/po/uforadiant.pot --omit-header \
		src/tools/radiant/radiant/*.cpp \
		src/tools/radiant/radiant/*.h

po-sync:
	@echo "This will sync all po files with the wiki articles - run update-po before this step"
	@echo "Gamers don't to do this - translators should use ./src/po/update_po_from_wiki <lang> directly"
	@echo "Hit any key if you are sure you really want to start the sync"
	@pofiles='$(POFILES)'; \
	read enter; cd src/po; \
	for po in $$pofiles; do \
	  po=`basename $$po .po`; \
	  echo $$po; \
	  ./update_po_from_wiki.sh $$po; \
	done
