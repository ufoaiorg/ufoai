########################################################################################################
# ufo2map
########################################################################################################

UFO2MAP_SRCS = \
	tools/ufo2map/ufo2map.c \
	tools/ufo2map/qrad3.c \
	tools/ufo2map/qbsp3.c \
	tools/ufo2map/brushbsp.c \
	tools/ufo2map/csg.c \
	tools/ufo2map/faces.c \
	tools/ufo2map/glfile.c \
	tools/ufo2map/levels.c \
	tools/ufo2map/lightmap.c \
	tools/ufo2map/map.c \
	tools/ufo2map/nodraw.c \
	tools/ufo2map/patches.c \
	tools/ufo2map/portals.c \
	tools/ufo2map/routing.c \
	tools/ufo2map/textures.c \
	tools/ufo2map/tree.c \
	tools/ufo2map/writebsp.c \
	tools/ufo2map/common/bspfile.c \
	tools/ufo2map/common/cmdlib.c \
	tools/ufo2map/common/mathlib.c \
	tools/ufo2map/common/polylib.c \
	tools/ufo2map/common/scriplib.c \
	tools/ufo2map/common/trace.c \
	tools/ufo2map/common/lbmlib.c \
	tools/ufo2map/common/threads.c

UFO2MAP_OBJS=$(UFO2MAP_SRCS:%.c=$(BUILDDIR)/tools/ufo2map/%.o)
UFO2MAP_DEPS=$(UFO2MAP_OBJS:%.o=%.d)
UFO2MAP_TARGET=ufo2map$(EXE_EXT)

ifeq ($(BUILD_UFO2MAP),1)
	ALL_OBJS+=$(UFO2MAP_OBJS)
	ALL_DEPS+=$(UFO2MAP_DEPS)
	TARGETS+=$(UFO2MAP_TARGET)
endif

# Say how to like the exe
$(UFO2MAP_TARGET): $(UFO2MAP_OBJS) $(BUILDDIR)/.dirs
	@echo " * [MAP] ... linking"; \
		$(CC) $(LDFLAGS) -o $@ $(UFO2MAP_OBJS) $(LIBS)

# Say how to build .o files from .c files for this module
$(BUILDDIR)/tools/ufo2map/%.o: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [MAP] $<"; \
		$(CC) $(CFLAGS) -o $@ -c $<

# Say how to build the dependencies
ifdef BUILDDIR
$(BUILDDIR)/tools/ufo2map/%.d: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [DEP] $<"; $(DEP)
endif

########################################################################################################
# qdata
########################################################################################################

QDATA_SRCS=\
	tools/qdata/qdata.c \
	tools/qdata/models.c \
	tools/qdata/sprites.c \
	tools/qdata/images.c \
	tools/qdata/tables.c \
	tools/qdata/video.c \
	\
	qcommon/md4.c \
	\
	tools/qdata/common/trilib.c \
	tools/qdata/common/l3dslib.c \
	\
	tools/ufo2map/common/bspfile.c \
	tools/ufo2map/common/cmdlib.c \
	tools/ufo2map/common/lbmlib.c \
	tools/ufo2map/common/mathlib.c \
	tools/ufo2map/common/scriplib.c \
	tools/ufo2map/common/threads.c

QDATA_OBJS=$(QDATA_SRCS:%.c=$(BUILDDIR)/tools/qdata/%.o)
QDATA_DEPS=$(QDATA_OBJS:%.o=%.d)
QDATA_TARGET=qdata$(EXE_EXT)

ifeq ($(BUILD_QDATA),1)
	ALL_OBJS+=$(QDATA_OBJS)
	ALL_DEPS+=$(QDATA_DEPS)
	TARGETS+=$(QDATA_TARGET)
endif

# Say how to like the exe
$(QDATA_TARGET): $(QDATA_OBJS) $(BUILDDIR)/.dirs
	@echo " * [QDT] ... linking"; \
		$(CC) $(LDFLAGS) -o $@ $(QDATA_OBJS) $(LIBS)

# Say how to build .o files from .c files for this module
$(BUILDDIR)/tools/qdata/%.o: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [QDT] $<"; \
		$(CC) $(CFLAGS) -o $@ -c $<

# Say how to build the dependencies
ifdef BUILDDIR
$(BUILDDIR)/tools/qdata/%.d: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [DEP] $<"; $(DEP)
endif




