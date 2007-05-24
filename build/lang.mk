
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
	@cd src/po
	@pofiles='$(POFILES)'; \
	for po in $$pofiles; do \
	  po=`basename $$po|awk -F\. '{print $1}'`; \
	  ./update_po_from_wiki $$po;
	done
