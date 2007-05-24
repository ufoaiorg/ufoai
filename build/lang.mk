
POFILES = $(wildcard src/po/*.po)

lang:
	@echo "Making lang"
	@pofiles='$(POFILES)'; \
	for po in $$pofiles; do \
	  po=`basename $$po`; \
	  echo $$po; \
	  dir=`echo $$po | sed -e 's,\.po,,'`; \
	  mkdir -p base/i18n/$$dir/LC_MESSAGES; \
	  msgfmt -v -o base/i18n/$$dir/LC_MESSAGES/ufoai.mo src/po/$$po; \
	done

update-po:
	$(MAKE) -C src/po update-po

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
