UFOAI_POFILES = $(wildcard $(SRCDIR)/po/ufoai-*.po)
RADIANT_POFILES = $(wildcard $(SRCDIR)/po/uforadiant-*.po)

UFOAI_MOFILES = $(patsubst $(SRCDIR)/po/ufoai-%.po, base/i18n/%/LC_MESSAGES/ufoai.mo, $(UFOAI_POFILES))
RADIANT_MOFILES = $(patsubst $(SRCDIR)/po/uforadiant-%.po, radiant/i18n/%/LC_MESSAGES/uforadiant.mo, $(RADIANT_POFILES))

$(UFOAI_MOFILES) : base/i18n/%/LC_MESSAGES/ufoai.mo : $(SRCDIR)/po/ufoai-%.po
	@echo $^
	$(Q)mkdir -p $(dir $@)
	$(Q)msgfmt -c -v -o $@ $^

$(RADIANT_MOFILES) : radiant/i18n/%/LC_MESSAGES/uforadiant.mo : $(SRCDIR)/po/uforadiant-%.po
	@echo $^
	$(Q)mkdir -p $(dir $@)
	$(Q)msgfmt -c -v -o $@ $^

clean-lang:
	$(Q)rm -f $(UFOAI_MOFILES) $(RADIANT_MOFILES)

lang: $(UFOAI_MOFILES) $(RADIANT_MOFILES)

update-po: po-check
	$(Q)$(MAKE) -C $(SRCDIR)/po update-po

update-po-uforadiant: po-check
	$(Q)xgettext -j --keyword="_" --keyword="C_:1c,2" -C -o $(SRCDIR)/po/uforadiant.pot --omit-header \
		$(SRCDIR)/tools/radiant/libs/*.h \
		$(SRCDIR)/tools/radiant/libs/*/*.h \
		$(SRCDIR)/tools/radiant/libs/*/*.cpp \
		$(SRCDIR)/tools/radiant/include/*.h \
		$(SRCDIR)/tools/radiant/radiant/*/*.cpp \
		$(SRCDIR)/tools/radiant/radiant/*/*.h \
		$(SRCDIR)/tools/radiant/radiant/*.cpp \
		$(SRCDIR)/tools/radiant/radiant/*.h

# From coreutils.
# Verify that all source files using _() are listed in po/POTFILES.in.
# The idea is to run this before making pretests, as well as official
# releases, so that
po-check:
	cd src; \
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
	@echo "Gamers don't have to do this - translators should use ./$(SRCDIR)/po/update_po_from_wiki.sh <lang> directly"
	@echo "Hit any key if you are sure you really want to start the sync"
	pofiles='$(UFOAI_POFILES)'; \
	read enter; cd $(SRCDIR)/po; \
	for po in $$pofiles; do \
	  po=`basename $$po .po`; \
	  po=`echo $$po | cut -b 7-`; \
	  echo $$po; \
	  ./update_po_from_wiki.sh $$po; \
	done

# call this after po-sync and log file checks
po-move:
	$(Q)cd $(SRCDIR)/po; \
	for po in `ls updated_*.po`; do \
	  new=`echo $$po | cut -b 9-`; \
	  mv $$po ufoai-$$new; \
	done
