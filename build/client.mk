CLIENT_CFLAGS=

CLIENT_SRCS = \
	client/cl_actor.c \
	client/cl_aliencont.c \
	client/cl_aircraft.c \
	client/cl_airfight.c \
	client/cl_basemanagement.c \
	client/cl_basesummary.c \
	client/cl_campaign.c \
	client/cl_cinematic.c \
	client/cl_console.c \
	client/cl_event.c \
	client/cl_employee.c \
	client/cl_fx.c \
	client/cl_hospital.c \
	client/cl_http.c \
	client/cl_inventory.c \
	client/cl_input.c \
	client/cl_irc.c \
	client/cl_keys.c \
	client/cl_language.c \
	client/cl_le.c \
	client/cl_main.c \
	client/cl_menu.c \
	client/cl_market.c \
	client/cl_map.c \
	client/cl_mapfightequip.c \
	client/cl_popup.c \
	client/cl_produce.c \
	client/cl_parse.c \
	client/cl_particle.c \
	client/cl_radar.c \
	client/cl_research.c \
	client/cl_save.c \
	client/cl_shader.c \
	client/cl_screen.c \
	client/cl_sequence.c \
	client/cl_team.c \
	client/cl_tip.c \
	client/cl_transfer.c \
	client/cl_ufo.c \
	client/cl_ufopedia.c \
	client/cl_vid.c \
	client/cl_view.c \
	client/snd_ogg.c \
	client/snd_ref.c \
	client/snd_mix.c \
	client/snd_wave.c \
	\
	common/cmd.c \
	common/ioapi.c \
	common/unzip.c \
	common/cmodel.c \
	common/common.c \
	common/cvar.c \
	common/files.c \
	common/md4.c \
	common/md5.c \
	common/mem.c \
	common/msg.c \
	common/net.c \
	common/netpack.c \
	common/dbuffer.c \
	common/scripts.c \
	\
	server/sv_ccmds.c \
	server/sv_game.c \
	server/sv_init.c \
	server/sv_main.c \
	server/sv_send.c \
	server/sv_user.c \
	server/sv_world.c \
	\
	game/q_shared.c \
	game/inv_shared.c \
	\
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
	shared/byte.c \
	shared/infostring.c \
	shared/shared.c

ifeq ($(HAVE_OPENAL),1)
	CLIENT_SRCS+= \
		client/snd_openal.c \
		client/qal.c
endif

ifneq ($(findstring $(TARGET_OS), netbsd freebsd linux-gnu),)
	CLIENT_SRCS+= \
		ports/linux/linux_main.c \
		ports/unix/unix_ref_sdl.c \
		ports/unix/unix_console.c \
		ports/unix/unix_main.c \
		ports/unix/unix_glob.c

	ifeq ($(HAVE_OPENAL),1)
		CLIENT_SRCS+= \
			ports/linux/linux_qal.c
	endif
endif

ifeq ($(TARGET_OS),mingw32)
	CLIENT_SRCS+=\
		ports/unix/unix_ref_sdl.c \
		ports/windows/win_qgl.c \
		ports/windows/win_shared.c \
		ports/windows/win_vid.c \
		ports/windows/win_input.c \
		ports/windows/win_main.c \
		ports/windows/ufo.rc

	ifeq ($(HAVE_OPENAL),1)
		CLIENT_SRCS+=\
			ports/windows/win_qal.c
	endif
endif

ifeq ($(TARGET_OS),darwin)
	CLIENT_SRCS+= \
		ports/macosx/osx_main.m \
		ports/macosx/osx_qal.c \
		ports/unix/unix_ref_sdl.c \
		ports/unix/unix_glob.c \
		ports/unix/unix_console.c \
		ports/unix/unix_main.c
endif

ifeq ($(TARGET_OS),solaris)
	CLIENT_SRCS+= \
		ports/solaris/solaris_main.c \
		ports/unix/unix_ref_sdl.c \
		ports/unix/unix_console.c \
		ports/unix/unix_main.c \
		ports/unix/unix_glob.c
	ifeq ($(HAVE_OPENAL),1)
		CLIENT_SRCS+=\
			ports/linux/linux_qal.c
	endif
endif

ifeq ($(HAVE_CURL),1)
	CLIENT_CFLAGS+=-DHAVE_CURL
endif

CLIENT_LIBS+=$(SDL_LIBS) $(REF_SDL_LIBS)

CLIENT_OBJS= \
	$(patsubst %.c, $(BUILDDIR)/client/%.o, $(filter %.c, $(CLIENT_SRCS))) \
	$(patsubst %.m, $(BUILDDIR)/client/%.o, $(filter %.m, $(CLIENT_SRCS))) \
	$(patsubst %.rc, $(BUILDDIR)/client/%.o, $(filter %.rc, $(CLIENT_SRCS)))

CLIENT_TARGET=ufo$(EXE_EXT)

ifeq ($(BUILD_CLIENT),1)
	ALL_OBJS+=$(CLIENT_OBJS)
	TARGETS+=$(CLIENT_TARGET)
endif

# Say how to link the exe
$(CLIENT_TARGET): $(CLIENT_OBJS) $(BUILDDIR)/.dirs
	@echo " * [UFO] ... linking $(LNKFLAGS) ($(CLIENT_LIBS))"; \
		$(CC) $(LDFLAGS) -o $@ $(CLIENT_OBJS) $(LNKFLAGS) $(CLIENT_LIBS)

# Say how to build .o files from .c files for this module
$(BUILDDIR)/client/%.o: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [UFO] $<"; \
		$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) $(CLIENT_CFLAGS) -o $@ -c $< -MD -MT $@ -MP

# Say how to build .o files from .m files for this module
$(BUILDDIR)/client/%.o: $(SRCDIR)/%.m $(BUILDDIR)/.dirs
	@echo " * [UFO] $<"; \
		$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) $(CLIENT_CFLAGS) -o $@ -c $< -MD -MT $@ -MP

ifeq ($(TARGET_OS),mingw32)
# Say how to build .o files from .rc files for this module
$(BUILDDIR)/client/%.o: $(SRCDIR)/%.rc $(BUILDDIR)/.dirs
	@echo " * [RC ] $<"; \
		$(WINDRES) -DCROSSBUILD -i $< -o $@
endif
