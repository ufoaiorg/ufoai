REF_GL_SRCS = \
	renderer/r_anim.c \
	renderer/r_shader.c \
	renderer/r_draw.c \
	renderer/r_font.c \
	renderer/r_image.c \
	renderer/r_light.c \
	renderer/r_mesh.c \
	renderer/r_mesh_md2.c \
	renderer/r_mesh_md3.c \
	renderer/r_model.c \
	renderer/r_model_alias.c \
	renderer/r_model_brush.c \
	renderer/r_model_md2.c \
	renderer/r_model_md3.c \
	renderer/r_model_obj.c \
	renderer/r_model_sp2.c \
	renderer/r_main.c \
	renderer/r_misc.c \
	renderer/r_surf.c \
	renderer/r_state.c \
	renderer/r_warp.c \
	renderer/r_particle.c \
	renderer/r_shadows.c \
	renderer/qgl.c \
	\
	game/q_shared.c \
	shared/byte.c \
	shared/shared.c

ifneq ($(findstring $(TARGET_OS), netbsd freebsd linux-gnu),)
	REF_GL_SRCS += \
			ports/linux/linux_qgl.c \
			ports/linux/linux_shared.c \
			ports/unix/unix_glob.c
	REF_SDL_SRCS = ports/unix/unix_ref_sdl.c
	REF_SDL_TARGET=ref_sdl.$(SHARED_EXT)
	REF_GLX_SRCS = ports/linux/linux_ref_glx.c
endif

ifeq ($(TARGET_OS),mingw32)
	REF_GL_SRCS += \
		ports/windows/win_qgl.c \
		ports/windows/glw_imp.c \
		ports/windows/win_shared.c
	REF_SDL_SRCS =
	REF_SDL_TARGET=ref_gl.$(SHARED_EXT)
endif

ifeq ($(TARGET_OS),darwin)
	REF_GL_SRCS += \
			ports/macosx/osx_qgl.c \
			ports/macosx/osx_shared.c \
			ports/unix/unix_glob.c
	REF_SDL_SRCS = ports/unix/unix_ref_sdl.c
	REF_SDL_TARGET=ref_sdl.$(SHARED_EXT)
endif

ifeq ($(TARGET_OS),solaris)
	#TODO
	REF_GL_SRCS += \
			ports/solaris/solaris_qgl.c \
			ports/solaris/solaris_shared.c \
			ports/unix/unix_glob.c
	REF_SDL_SRCS = ports/unix/unix_ref_sdl.c
	REF_SDL_TARGET=ref_sdl.$(SHARED_EXT)
endif

#---------------------------------------------------------------------------------------------------------------------

REF_GL_OBJS=$(REF_GL_SRCS:%.c=$(BUILDDIR)/renderer/%.o)

REF_GLX_OBJS=$(REF_GLX_SRCS:%.c=$(BUILDDIR)/renderer/%.o)
REF_GLX_TARGET=ref_glx.$(SHARED_EXT)

ifeq ($(BUILD_CLIENT), 1)
ifeq ($(HAVE_VID_GLX), 1)
	TARGETS += $(REF_GLX_TARGET)
	ALL_OBJS += $(REF_GL_OBJS) $(REF_GLX_OBJS)
endif
endif

# Say about to build the target
$(REF_GLX_TARGET) : $(REF_GLX_OBJS) $(REF_GL_OBJS) $(BUILDDIR)/.dirs
	@echo " * [GLX] ... linking $(LNKFLAGS) ($(REF_GLX_LIBS))"; \
		$(CC) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(REF_GLX_OBJS) $(REF_GL_OBJS) $(REF_GLX_LIBS) $(LNKFLAGS)

#---------------------------------------------------------------------------------------------------------------------

REF_SDL_OBJS=$(REF_SDL_SRCS:%.c=$(BUILDDIR)/renderer/%.o)

ifeq ($(BUILD_CLIENT), 1)
ifeq ($(HAVE_SDL), 1)
	TARGETS += $(REF_SDL_TARGET)
	ALL_OBJS += $(REF_GL_OBJS) $(REF_SDL_OBJS)
endif
endif

# Say about to build the target
$(REF_SDL_TARGET) : $(REF_SDL_OBJS) $(REF_GL_OBJS) $(BUILDDIR)/.dirs
	@echo " * [SDL] ... linking $(LNKFLAGS) ($(REF_SDL_LIBS) $(SDL_LIBS))"; \
		$(CC) $(LDFLAGS) $(LNKFLAGS) $(SHARED_LDFLAGS) -o $@ $(REF_SDL_OBJS) $(REF_GL_OBJS) $(REF_SDL_LIBS) $(SDL_LIBS) $(LNKFLAGS)

#---------------------------------------------------------------------------------------------------------------------

# Say how to build .o files from .c files for this module
$(BUILDDIR)/renderer/%.o: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [RGL] $<"; \
		$(CC) $(CFLAGS) $(CPPFLAGS) $(SHARED_CFLAGS) $(SDL_CFLAGS) $(JPEG_CFLAGS) $(REF_GLX_CFLAGS) -o $@ -c $< -MD -MT $@ -MP
