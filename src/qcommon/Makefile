buildcommon?=$(BUILDDIR)/common

COMMON_HDR+=$(COMMON_SRC)/qcommon.h

commonobjs+=$(buildcommon)/cmd.o
commonobjs+=$(buildcommon)/cmodel.o
commonobjs+=$(buildcommon)/common.o
commonobjs+=$(buildcommon)/crc.o
commonobjs+=$(buildcommon)/cvar.o
commonobjs+=$(buildcommon)/files.o
commonobjs+=$(buildcommon)/md4.c
commonobjs+=$(buildcommon)/net_chan.c
commonobjs+=$(buildcommon)/scripts.c

$(buildcommon)/%.o: $(COMMON_SRC)/%.c $(COMMON_SRC)/%.h $(COMMON_HDR)
	@echo " * [COMMON] $@"; $(CC) $(NOTSHLIBCFLAGS) $(CFLAGS) -o $@ -c $<