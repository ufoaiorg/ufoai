_BUILDDIR=$(strip $(BUILDDIR))
_MODULE=$(strip $(MODULE))
_SRCDIR=$(strip $(SRCDIR))

DEP=$(CC) $(CFLAGS) -c $< -MM -MQ "$(@:%.d=%.o) $@" -MF $@

LIBTOOL_LD=$(LIBTOOL) --silent --mode=link $(CC) -module -rpath / $(LDFLAGS)
LIBTOOL_CC=$(LIBTOOL) --silent --mode=compile $(CC) -prefer-pic $(CFLAGS)
