TARGET             := ufomodel

# if the linking should be static
$(TARGET)_STATIC   ?= $(STATIC)
ifeq ($($(TARGET)_STATIC),1)
$(TARGET)_LDFLAGS  += -static
endif

$(TARGET)_LINKER   := $(CXX)
$(TARGET)_FILE     := $(TARGET)$(EXE_EXT)
$(TARGET)_LDFLAGS  += -lpng -ljpeg -lz -lm $(SDL_LIBS)
$(TARGET)_CFLAGS   += -DCOMPILE_MAP -ffloat-store $(SDL_CFLAGS)

ifeq ($(SSE),1)
   $(TARGET)_CFLAGS := $(filter-out -ffloat-store,$($(TARGET)_CFLAGS))
endif

$(TARGET)_SRCS      = \
	tools/ufomodel/ufomodel.cpp \
	\
	shared/mathlib.cpp \
	shared/aabb.cpp \
	shared/byte.cpp \
	shared/images.cpp \
	shared/parse.cpp \
	shared/shared.cpp \
	shared/utf8.cpp \
	\
	common/files.cpp \
	common/list.cpp \
	common/mem.cpp \
	common/unzip.cpp \
	common/ioapi.cpp \
	\
	client/renderer/r_model.cpp \
	client/renderer/r_model_alias.cpp \
	client/renderer/r_model_md2.cpp \
	client/renderer/r_model_md3.cpp \
	client/renderer/r_model_obj.cpp

ifeq ($(TARGET_OS),mingw32)
	$(TARGET)_SRCS+=\
		ports/windows/win_shared.cpp
else
	$(TARGET)_SRCS+= \
		ports/unix/unix_files.cpp \
		ports/unix/unix_shared.cpp \
		ports/unix/unix_main.cpp
endif

$(TARGET)_OBJS     := $(call ASSEMBLE_OBJECTS,$(TARGET))
$(TARGET)_CXXFLAGS := $($(TARGET)_CFLAGS)
$(TARGET)_CCFLAGS  := $($(TARGET)_CFLAGS)
