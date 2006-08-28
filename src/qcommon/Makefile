DO_CC=echo " * [UFO] $@"; $(CC) $(NOTSHLIBCFLAGS) $(CFLAGS) -o $@ -c

.PHONY: all

$(BUILDDIR)/common/%.o: %.c %.h $(COMMON_HDR)
	@echo " * [UFO] $@"; $(CC) $(NOTSHLIBCFLAGS) $(CFLAGS) -o $@ -c $<

all: $(BUILDDIR)/common/cmd.o

#$(BUILDDIR)/common/cmd.o: $(COMMON_HDR) cmd.c
#	$(DO_CC) cmd.c

$(BUILDDIR)/common/cmodel.o: $(COMMON_HDR) cmodel.c
	$(DO_CC) cmodel.c

$(BUILDDIR)/common/common.o: $(COMMON_HDR) common.c
	$(DO_CC) common.c

$(BUILDDIR)/common/crc.o: $(COMMON_HDR) crc.h crc.c
	$(DO_CC) crc.c

$(BUILDDIR)/common/cvar.o: $(COMMON_HDR) cvar.c
	$(DO_CC) cvar.c

$(BUILDDIR)/common/files.o: $(COMMON_HDR) files.c
	$(DO_CC) files.c

$(BUILDDIR)/common/md4.o: $(COMMON_HDR) md4.c
	$(DO_CC) md4.c

$(BUILDDIR)/common/net_chan.o: $(COMMON_HDR) net_chan.c
	$(DO_CC) net_chan.c

$(BUILDDIR)/common/scripts.o: $(COMMON_HDR) scripts.c
	$(DO_CC) scripts.c

