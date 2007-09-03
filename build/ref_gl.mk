REF_GL_SRCS = \
	ref_gl/gl_anim.c \
	ref_gl/gl_arb_shader.c \
	ref_gl/gl_draw.c \
	ref_gl/gl_font.c \
	ref_gl/gl_image.c \
	ref_gl/gl_light.c \
	ref_gl/gl_mesh.c \
	ref_gl/gl_mesh_md2.c \
	ref_gl/gl_mesh_md3.c \
	ref_gl/gl_model.c \
	ref_gl/gl_model_alias.c \
	ref_gl/gl_model_brush.c \
	ref_gl/gl_model_md2.c \
	ref_gl/gl_model_md3.c \
	ref_gl/gl_model_obj.c \
	ref_gl/gl_model_sp2.c \
	ref_gl/gl_rmain.c \
	ref_gl/gl_rmisc.c \
	ref_gl/gl_rsurf.c \
	ref_gl/gl_state.c \
	ref_gl/gl_warp.c \
	ref_gl/gl_particle.c \
	ref_gl/gl_shadows.c \
	ref_gl/qgl.c \
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

REF_GL_OBJS=$(REF_GL_SRCS:%.c=$(BUILDDIR)/ref_gl/%.o)

REF_GLX_OBJS=$(REF_GLX_SRCS:%.c=$(BUILDDIR)/ref_gl/%.o)
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

REF_SDL_OBJS=$(REF_SDL_SRCS:%.c=$(BUILDDIR)/ref_gl/%.o)

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
$(BUILDDIR)/ref_gl/%.o: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [RGL] $<"; \
		$(CC) $(CFLAGS) $(CPPFLAGS) $(SHARED_CFLAGS) $(SDL_CFLAGS) $(JPEG_CFLAGS) $(REF_GLX_CFLAGS) -o $@ -c $< -MD -MT $@ -MP
