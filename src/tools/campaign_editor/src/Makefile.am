## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

bin_PROGRAMS = campaign_editor

campaign_editor_SOURCES = \
	main.c \
	support.c support.h \
	interface.c interface.h \
	scripts.c scripts.h \
	campaign.c campaign.h \
	weapon.c weapon.h \
	particle.c particle.h \
	callbacks.c callbacks.h

campaign_editor_LDADD = @PACKAGE_LIBS@ $(INTLLIBS)

