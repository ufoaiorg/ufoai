TARGET             := ufomodel

# if the linking should be static
$(TARGET)_STATIC   ?= $(STATIC)
ifeq ($($(TARGET)_STATIC),1)
$(TARGET)_LDFLAGS  += -static
endif

$(TARGET)_LINKER   := $(CXX)
$(TARGET)_FILE     := $(TARGET)$(EXE_EXT)
$(TARGET)_CFLAGS   += -DCOMPILE_MAP $(SDL_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(LUA_CFLAGS)
$(TARGET)_LDFLAGS  += $(PNG_LIBS) $(JPEG_LIBS) -lz -lm $(SDL_LIBS) $(LUA_LIBS)
ifeq (,$(findstring clang,$(CXX)))
	$(TARGET)_CFLAGS   += -ffloat-store
endif
ifneq ($(findstring $(TARGET_OS), openbsd netbsd freebsd),)
	$(TARGET)_LDFLAGS += -lexecinfo
endif


ifeq ($(SSE),1)
   $(TARGET)_CFLAGS := $(filter-out -ffloat-store,$($(TARGET)_CFLAGS))
endif

$(TARGET)_SRCS      = \
	tools/ufomodel/ufomodel.cpp \
	tools/ufomodel/md2.cpp \
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

ifneq ($(findstring $(TARGET_OS), mingw32 mingw64 mingw64_64),)
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
