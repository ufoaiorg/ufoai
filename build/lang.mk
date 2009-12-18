
UFOAI_POFILES = $(wildcard src/po/ufoai-*.po)
RADIANT_POFILES = $(wildcard src/po/uforadiant-*.po)

UFOAI_MOFILES = $(patsubst src/po/ufoai-%.po, base/i18n/%/LC_MESSAGES/ufoai.mo, $(UFOAI_POFILES))
RADIANT_MOFILES = $(patsubst src/po/uforadiant-%.po, radiant/i18n/%/LC_MESSAGES/uforadiant.mo, $(RADIANT_POFILES))

$(UFOAI_MOFILES) : base/i18n/%/LC_MESSAGES/ufoai.mo : src/po/ufoai-%.po
	@mkdir -p $(dir $@)
	msgfmt -c -v -o $@ $^

$(RADIANT_MOFILES) : radiant/i18n/%/LC_MESSAGES/uforadiant.mo : src/po/uforadiant-%.po
	@mkdir -p $(dir $@)
	msgfmt -c -v -o $@ $^

lang: $(UFOAI_MOFILES) $(RADIANT_MOFILES)

update-po: po-check
	$(MAKE) -C src/po update-po

update-po-uforadiant: po-check
	xgettext -j --keyword="_" --keyword="C_:1c,2" -C -o src/po/uforadiant.pot --omit-header \
		src/tools/radiant/libs/*.h \
		src/tools/radiant/libs/*/*.h \
		src/tools/radiant/libs/*/*.cpp \
		src/tools/radiant/include/*.h \
		src/tools/radiant/plugins/*/*.cpp \
		src/tools/radiant/plugins/*/*.h \
		src/tools/radiant/radiant/*/*.cpp \
		src/tools/radiant/radiant/*/*.h \
		src/tools/radiant/radiant/*.cpp \
		src/tools/radiant/radiant/*.h

# From coreutils.
# Verify that all source files using _() are listed in po/POTFILES.in.
# The idea is to run this before making pretests, as well as official
# releases, so that
po-check:
	@cd src; \
	if test -f ./po/POTFILES.in; then					\
	  files="./po/OTHER_STRINGS";							\
	  for file in `find ./client -name '*.[ch]'` `find ./game -name '*.[ch]'`; do \
	    case $$file in						\
	    djgpp/* | man/*) continue;;					\
	    esac;							\
	    case $$file in						\
	    *.[ch])							\
	      base=`expr " $$file" : ' \(.*\)\..'`;			\
	      { test -f $$base.l || test -f $$base.y; } && continue;;	\
	    esac;							\
	    files="$$files $$file";					\
	  done;								\
	  grep -E -l '\b(N?_|gettext|ngettext *)\([^)"]*("|$$)' $$files		\
	    | sort -u > ./po/POTFILES.in;						\
	fi; \
	cd ..

po-sync:
	@echo "This will sync all po files with the wiki articles - run update-po before this step"
	@echo "Gamers don't have to do this - translators should use ./src/po/update_po_from_wiki.sh <lang> directly"
	@echo "Hit any key if you are sure you really want to start the sync"
	@pofiles='$(UFOAI_POFILES)'; \
	read enter; cd src/po; \
	for po in $$pofiles; do \
	  po=`basename $$po .po`; \
	  po=`echo $$po | cut -b 7-`; \
	  echo $$po; \
	  ./update_po_from_wiki.sh $$po; \
	done

po-move:
	for i in `ls updated_*.po`; do \
		mv $$i `echo $$i | sed 's/^updated_\(.*\)\.po$/ufoai-\1\.po/'`; \
	done

