TARGET             := ufoslicer

# if the linking should be static
$(TARGET)_STATIC   ?= $(STATIC)
ifeq ($($(TARGET)_STATIC),1)
$(TARGET)_LDFLAGS  += -static
endif

$(TARGET)_LINKER   := $(CC)
$(TARGET)_FILE     := $(TARGET)$(EXE_EXT)
$(TARGET)_LDFLAGS  += -lpng -ljpeg -lm -lz $(SDL_LIBS)
$(TARGET)_CFLAGS   += -DCOMPILE_MAP $(SDL_CFLAGS)

$(TARGET)_SRCS      = \
	tools/ufoslicer.c \
	\
	common/bspslicer.c \
	common/files.c \
	common/list.c \
	common/mem.c \
	common/unzip.c \
	common/ioapi.c \
	\
	tools/ufo2map/common/bspfile.c \
	tools/ufo2map/common/scriplib.c \
	\
	shared/mathlib.c \
	shared/mutex.c \
	shared/byte.c \
	shared/images.c \
	shared/parse.c \
	shared/shared.c \
	shared/utf8.c

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
