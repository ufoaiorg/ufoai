# Makefile for PO directory in any package using GNU gettext.
# Copyright (C) 1995-1997, 2000-2005 by Ulrich Drepper <drepper@gnu.ai.mit.edu>
#
# This file can be copied and used freely without restrictions.  It can
# be used in projects which are not available under the GNU General Public
# License but which still want to provide support for the GNU gettext
# functionality.
# Please note that the actual code of GNU gettext is covered by the GNU
# General Public License and is *not* in the public domain.
#
# Origin: gettext-0.14.4

PACKAGE = ufoai
PACKAGE_BUGREPORT = "tlh2000@users.sourceforge.net"
DOMAIN = $(PACKAGE)
# This is the copyright holder that gets inserted into the header of the
# $(DOMAIN).pot file.  Set this to the copyright holder of the surrounding
# package.  (Note that the msgstr strings, extracted from the package's
# sources, belong to the copyright holder of the package.)  Translators are
# expected to transfer the copyright for their translations to this person
# or entity, or to disclaim their copyright.  The empty string stands for
# the public domain; in this case the translators are expected to disclaim
# their copyright.
COPYRIGHT_HOLDER = The UFO:AI team

SHELL = /bin/sh

srcdir = .
top_srcdir = ..
subdir = po
top_builddir = ..

# These options get passed to xgettext.
XGETTEXT_OPTIONS = --keyword=_ --keyword=N_ --no-wrap

GMSGFMT = msgfmt
MSGFMT = msgfmt
XGETTEXT = xgettext
MSGMERGE = msgmerge
MSGMERGE_UPDATE = msgmerge --update
UFOPO := $(srcdir)/ufopo.pl
MSGCAT = msgcat

POFILES = $(wildcard ufoai-*.po)
GMOFILES = $(POFILES:.po=.gmo)
UPDATEPOFILES = $(POFILES:.po=.po-update)
DUMMYPOFILES = $(POFILES:.po=.nop)

POTFILES =

.SUFFIXES:
.SUFFIXES: .po .gmo .mo .sed .nop .po-create .po-update

.po.mo:
	$(MSGFMT) -c -o t-$@ $< && mv t-$@ $@

.po.gmo:
	@lang=`echo $* | sed -e 's,.*/,,'`; \
	test "$(srcdir)" = . && cdcmd="" || cdcmd="cd $(srcdir) && "; \
	cd $(srcdir) && rm -f $${lang}.gmo && $(GMSGFMT) -c --statistics -o t-$${lang}.gmo $${lang}.po && mv t-$${lang}.gmo $${lang}.gmo

# This target rebuilds $(DOMAIN).pot; it is an expensive operation.
# Note that $(DOMAIN).pot is not touched if it doesn't need to be changed.
$(DOMAIN).pot-update: $(POTFILES) $(srcdir)/POTFILES.in remove-potcdate.sed
	$(XGETTEXT) --default-domain=$(DOMAIN) --directory=$(top_srcdir) \
	  --add-comments=TRANSLATORS: $(XGETTEXT_OPTIONS) \
	  --files-from=$(srcdir)/POTFILES.in \
	  --copyright-holder='$(COPYRIGHT_HOLDER)' \
	  --msgid-bugs-address="$(PACKAGE_BUGREPORT)" \
	  --no-wrap \
	  --output=$(DOMAIN).c.po
	sed s/charset=CHARSET/charset=UTF-8/ $(DOMAIN).c.po > $(DOMAIN).c1.po
	${UFOPO} --directory=$(top_srcdir) `cd $(top_srcdir) && sh $(subdir)/FINDUFOS` > $(DOMAIN).ufo.po
	$(MSGCAT) --sort-by-file $(DOMAIN).c1.po $(DOMAIN).ufo.po -o $(DOMAIN).po ;\
	sed -f remove-potcdate.sed < $(srcdir)/$(DOMAIN).pot > $(DOMAIN).1po && \
	sed -f remove-potcdate.sed < $(DOMAIN).po > $(DOMAIN).2po && \
	if cmp $(DOMAIN).1po $(DOMAIN).2po >/dev/null 2>&1; then \
	  rm -f $(DOMAIN).1po $(DOMAIN).2po $(DOMAIN).po; \
	else \
	  rm -f $(DOMAIN).1po $(DOMAIN).2po $(srcdir)/$(DOMAIN).pot && \
	  mv $(DOMAIN).po $(srcdir)/$(DOMAIN).pot; \
	fi

# This rule has no dependencies: we don't need to update $(DOMAIN).pot at
# every "make" invocation, only create it when it is missing.
# Only "make $(DOMAIN).pot-update" or "make dist" will force an update.
$(srcdir)/$(DOMAIN).pot:
	$(MAKE) $(DOMAIN).pot-update

# This target rebuilds a PO file if $(DOMAIN).pot has changed.
# Note that a PO file is not touched if it doesn't need to be changed.
$(POFILES): $(srcdir)/$(DOMAIN).pot
	lang=`echo $@ | sed -e 's,.*/,,' -e 's/\.po$$//'`; \
	test "$(srcdir)" = . && cdcmd="" || cdcmd="cd $(srcdir) && "; \
	cd $(srcdir) && $(MSGMERGE_UPDATE) $${lang}.po $(DOMAIN).pot

update-po: Makefile
	$(MAKE) $(DOMAIN).pot-update
	test -z "$(UPDATEPOFILES)" || $(MAKE) $(UPDATEPOFILES)

# General rule for updating PO files.

.nop.po-update:
	lang=`echo $@ | sed -e 's/\.po-update$$//'`; \
	tmpdir=`pwd`; \
	test "$(srcdir)" = . && cdcmd="" || cdcmd="cd $(srcdir) && "; \
	cd $(srcdir); \
	if $(MSGMERGE) $$lang.po $(DOMAIN).pot --no-wrap -o $$tmpdir/$$lang.new.po; then \
	  if cmp $$lang.po $$tmpdir/$$lang.new.po >/dev/null 2>&1; then \
	    rm -f $$tmpdir/$$lang.new.po; \
	  else \
	    mv -f $$tmpdir/$$lang.new.po $$lang.po; \
	  fi; \
	else \
	  echo "msgmerge for $$lang.po failed!" 1>&2; \
	  rm -f $$tmpdir/$$lang.new.po; \
	fi

$(DUMMYPOFILES):

# Tell versions [3.59,3.63) of GNU make not to export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
