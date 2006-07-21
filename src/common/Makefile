$(BUILDDIR)/common/%.o: %.c %.h $(COMMON_HDR)
	@echo " * [UFO] $@"; $(CC) $(NOTSHLIBCFLAGS) $(CFLAGS) -o $@ -c $<


DO_CC=echo " * [UFO] $@"; $(CC) $(NOTSHLIBCFLAGS) $(CFLAGS) -o $@ -c



$(BUILDDIR)/common/mem.o: $(COMMON_HDR) mem.h mem.c
	$(DO_CC) mem.c
