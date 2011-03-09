TARGET             := ufo2map

# if the linking should be static
$(TARGET)_STATIC   ?= $(STATIC)
ifeq ($($(TARGET)_STATIC),1)
$(TARGET)_LDFLAGS  += -static
endif

$(TARGET)_LINKER   := $(CC)
$(TARGET)_FILE     := $(TARGET)$(EXE_EXT)
$(TARGET)_CFLAGS   += -DCOMPILE_MAP -ffloat-store $(SDL_CFLAGS) $(SDL_IMAGE_CFLAGS)
$(TARGET)_LDFLAGS  += -lm -lpng -ljpeg -lz $(SDL_LIBS) $(SDL_IMAGE_LIBS)

ifeq ($(SSE),1)
   $(TARGET)_CFLAGS := $(filter-out -ffloat-store,$($(TARGET)_CFLAGS))
endif

$(TARGET)_SRCS      = \
	tools/ufo2map/ufo2map.c \
	tools/ufo2map/lighting.c \
	tools/ufo2map/bsp.c \
	tools/ufo2map/bspbrush.c \
	tools/ufo2map/csg.c \
	tools/ufo2map/faces.c \
	tools/ufo2map/levels.c \
	tools/ufo2map/lightmap.c \
	tools/ufo2map/map.c \
	tools/ufo2map/patches.c \
	tools/ufo2map/portals.c \
	tools/ufo2map/routing.c \
	tools/ufo2map/textures.c \
	tools/ufo2map/tree.c \
	tools/ufo2map/threads.c \
	tools/ufo2map/writebsp.c \
	tools/ufo2map/check/checkentities.c \
	tools/ufo2map/check/checklib.c \
	tools/ufo2map/check/check.c \
	tools/ufo2map/common/aselib.c \
	tools/ufo2map/common/bspfile.c \
	tools/ufo2map/common/polylib.c \
	tools/ufo2map/common/scriplib.c \
	tools/ufo2map/common/trace.c \
	\
	shared/mathlib.c \
	shared/mutex.c \
	shared/byte.c \
	shared/images.c \
	shared/parse.c \
	shared/shared.c \
	shared/entitiesdef.c \
	shared/utf8.c \
	\
	common/files.c \
	common/list.c \
	common/mem.c \
	common/unzip.c \
	common/tracing.c \
	common/routing.c \
	common/ioapi.c

ifeq ($(TARGET_OS),mingw32)
	$(TARGET)_SRCS+=\
		ports/windows/win_shared.c
else
	$(TARGET)_SRCS+= \
		ports/unix/unix_files.c \
		ports/unix/unix_shared.c \
		ports/unix/unix_main.c
endif

$(TARGET)_OBJS     := $(call ASSEMBLE_OBJECTS,$(TARGET))
$(TARGET)_CXXFLAGS := $($(TARGET)_CFLAGS)
$(TARGET)_CCFLAGS  := $($(TARGET)_CFLAGS)
