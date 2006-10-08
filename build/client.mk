CLIENT_SRCS = \
	client/cl_actor.c \
	client/cl_basemanagement.c \
	client/cl_research.c \
	client/cl_market.c \
	client/cl_campaign.c \
	client/cl_ufo.c \
	client/cl_radar.c \
	client/cl_popup.c \
	client/cl_produce.c \
	client/cl_employee.c \
	client/cl_map.c \
	client/cl_aircraft.c \
	client/cl_fx.c \
	client/cl_input.c \
	client/cl_le.c \
	client/cl_main.c \
	client/cl_menu.c \
	client/cl_parse.c \
	client/cl_particle.c \
	client/cl_shader.c \
	client/cl_scrn.c \
	client/cl_sequence.c \
	client/cl_team.c \
	client/cl_inventory.c \
	client/cl_ufopedia.c \
	client/cl_view.c \
	client/console.c \
	client/keys.c \
	client/snd_ref.c \
	client/snd_mem.c \
	client/snd_mix.c \
	\
	qcommon/cmd.c \
	qcommon/unzip.c \
	qcommon/cmodel.c \
	qcommon/common.c \
	qcommon/crc.c \
	qcommon/cvar.c \
	qcommon/files.c \
	qcommon/md4.c \
	qcommon/net_chan.c \
	qcommon/scripts.c \
	\
	server/sv_ccmds.c \
	server/sv_game.c \
	server/sv_init.c \
	server/sv_main.c \
	server/sv_send.c \
	server/sv_user.c \
	server/sv_world.c \
	\
	game/q_shared.c

ifeq ($(HAVE_IPV6),1)
	# FIXME: flags!
	NET_UDP=net_udp6
else
	NET_UDP=net_udp
endif

ifeq ($(TARGET_OS),linux-gnu)
	CLIENT_SRCS+= \
		ports/linux/q_shlinux.c \
		ports/linux/vid_so.c \
		ports/linux/sys_linux.c \
		ports/unix/sys_unix.c \
		ports/unix/glob.c \
		ports/unix/$(NET_UDP).c
	CLIENT_CD=ports/linux/cd_linux.c
endif

ifeq ($(TARGET_OS),freebsd)
	CLIENT_SRCS+= \
		ports/linux/q_shlinux.c \
		ports/linux/vid_so.c \
		ports/linux/sys_linux.c \
		ports/unix/sys_unix.c \
		ports/unix/glob.c \
		ports/unix/$(NET_UDP).c
	CLIENT_CD=ports/linux/cd_linux.c
endif

ifeq ($(TARGET_OS),mingw32)
	CLIENT_SRCS+=\
		ports/win32/q_shwin.c \
		ports/win32/vid_dll.c \
		ports/win32/in_win.c \
		ports/win32/conproc.c  \
		ports/win32/sys_win.c \
		ports/win32/net_wins.c
	CLIENT_CD=ports/win32/cd_win.c
endif

ifeq ($(TARGET_OS),darwin)
	CLIENT_SRCS+= \
		ports/macosx/sys_osx.m \
		ports/macosx/vid_osx.m \
		ports/macosx/in_osx.m \
		ports/macosx/snddma_osx.m \
		ports/unix/glob.c \
		ports/unix/sys_unix.c \
		ports/unix/$(NET_UDP).c \
		ports/macosx/q_shosx.c
		# FIXME Add more objects
   CLIENT_CD+=ports/macosx/cd_osx.m
endif

ifeq ($(TARGET_OS),mingw32)
	CLIENT_SRCS+=$(CLIENT_CD)
else
	ifeq ($(HAVE_SDL),1)
		CLIENT_SRCS+=ports/unix/cd_sdl.c
		CLIENT_LIBS+=$(SDL_LIBS)
	else
		CLIENT_SRCS+=$(CLIENT_CD)
	endif
endif

CLIENT_OBJS= \
	$(patsubst %.c, $(BUILDDIR)/client/%.o, $(filter %.c, $(CLIENT_SRCS))) \
	$(patsubst %.m, $(BUILDDIR)/client/%.o, $(filter %.m, $(CLIENT_SRCS)))

CLIENT_DEPS=$(CLIENT_OBJS:%.o=%.d)
CLIENT_TARGET=ufo$(EXE_EXT)

ALL_OBJS+=$(CLIENT_OBJS)
ALL_DEPS+=$(CLIENT_DEPS)
TARGETS+=$(CLIENT_TARGET)

# Say how to like the exe
$(CLIENT_TARGET): $(CLIENT_OBJS) $(BUILDDIR)/.dirs
	@echo " * [UFO] ... linking"; \
		$(CC) $(LDFLAGS) -o $@ $(CLIENT_OBJS) $(LIBS) $(CLIENT_LIBS) $(LIBS)

# Say how to build .o files from .c files for this module
$(BUILDDIR)/client/%.o: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [UFO] $<"; \
		$(CC) $(CFLAGS) -o $@ -c $<

# Say how to build .o files from .m files for this module
$(BUILDDIR)/client/%.m: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [UFO] $<"; \
		$(CC) $(CFLAGS) -o $@ -c $<

# Say how to build the dependencies
ifdef BUILDDIR
$(BUILDDIR)/client/%.d: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [DEP] $<"; $(DEP)

$(BUILDDIR)/client/%.d: $(SRCDIR)/%.m $(BUILDDIR)/.dirs
	@echo " * [DEP] $<"; $(DEP)
endif





