CC=gcc
ODIR=.

QBSP3BASE=qbsp3
QBSP3=$(ODIR)/qbsp3

QRAD3BASE=qrad3
QRAD3=$(ODIR)/qrad3

QBSPINFOBASE=qbspinfo3
QBSPINFO=$(ODIR)/qbspinfo3

FILES_QBSP3=$(ODIR)/bspfile.o \
 $(ODIR)/cmdlib.o \
 $(ODIR)/brushbsp.o \
 $(ODIR)/faces.o \
 $(ODIR)/nodraw.o \
 $(ODIR)/glfile.o \
 $(ODIR)/leakfile.o \
 $(ODIR)/map.o \
 $(ODIR)/mathlib.o \
 $(ODIR)/polylib.o \
 $(ODIR)/portals.o \
 $(ODIR)/prtfile.o \
 $(ODIR)/qbsp3.o \
 $(ODIR)/scriplib.o \
 $(ODIR)/textures.o \
 $(ODIR)/threads.o \
 $(ODIR)/tree.o \
 $(ODIR)/levels.o \
 $(ODIR)/writebsp.o \
 $(ODIR)/csg.o \
 $(ODIR)/trace.o \
 $(ODIR)/routing.o
# $(ODIR)/l3dslib.o \
# $(ODIR)/lbmlib.o \
# $(ODIR)/md4.o \
# $(ODIR)/trilib.o

FILES_QRAD3 = $(ODIR)/bspfile.o \
 $(ODIR)/cmdlib.o \
 $(ODIR)/lbmlib.o \
 $(ODIR)/mathlib.o \
 $(ODIR)/scriplib.o \
 $(ODIR)/polylib.o \
 $(ODIR)/qrad3.o \
 $(ODIR)/threads.o \
 $(ODIR)/trace.o \
 $(ODIR)/lightmap.o \
 $(ODIR)/patches.o

FILES_QBSPINFO = $(ODIR)/bspinfo3.o \
 $(ODIR)/bspfile.o \
 $(ODIR)/cmdlib.o \
 $(ODIR)/scriplib.o

#CFLAGS=-c -g -Icommon -DDOUBLEVEC_T
CFLAGS=-c -O4 -Icommon -DDOUBLEVEC_T
LDFLAGS=

all: $(QBSP3) $(QRAD3) $(QBSPINFO)

$(QBSP3): $(FILES_QBSP3)
	@if [ ! -d $(ODIR) ];then mkdir $(ODIR);fi
	$(CC) $(LDFLAGS) -lm -o $(QBSP3) $(FILES_QBSP3)
$(QBSPINFO): $(FILES_QBSPINFO)
	@if [ ! -d $(ODIR) ];then mkdir $(ODIR);fi
	$(CC) $(LDFLAGS) -lm -o $(QBSPINFO) $(FILES_QBSPINFO)
$(QRAD3): $(FILES_QRAD3)
	@if [ ! -d $(ODIR) ];then mkdir $(ODIR);fi
	$(CC) $(LDFLAGS) -lm -o $(QRAD3) $(FILES_QRAD3)

clean:
	rm -f $(ODIR)/*.o $(ODIR)/$(QBSP3BASE) $(ODIR)/$(QRAD3BASE) $(ODIR)/$(QBSPINFOBASE)

$(ODIR)/brushbsp.o : bsp/qbsp3/brushbsp.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/faces.o : bsp/qbsp3/faces.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/nodraw.o : bsp/qbsp3/nodraw.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/glfile.o : bsp/qbsp3/glfile.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/leakfile.o : bsp/qbsp3/leakfile.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/map.o : bsp/qbsp3/map.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/portals.o : bsp/qbsp3/portals.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/prtfile.o : bsp/qbsp3/prtfile.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/qbsp3.o : bsp/qbsp3/qbsp3.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/tree.o : bsp/qbsp3/tree.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/textures.o : bsp/qbsp3/textures.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/writebsp.o : bsp/qbsp3/writebsp.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/csg.o : bsp/qbsp3/csg.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/levels.o : bsp/qbsp3/levels.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/routing.o : bsp/qbsp3/routing.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(ODIR)/qrad3.o : bsp/qrad3/qrad3.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/patches.o : bsp/qrad3/patches.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/lightmap.o : bsp/qrad3/lightmap.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(ODIR)/cmdlib.o : common/cmdlib.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/mathlib.o : common/mathlib.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/polylib.o : common/polylib.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/scriplib.o : common/scriplib.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/threads.o : common/threads.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/bspfile.o : common/bspfile.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/l3dslib.o : common/l3dslib.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/lbmlib.o : common/lbmlib.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/md4.o : common/md4.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/trace.o : common/trace.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(ODIR)/trilib.o : common/trilib.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(ODIR)/bspinfo3.o : bsp/bspinfo3/bspinfo3.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean all $(QBSP3) $(QRAD3)
