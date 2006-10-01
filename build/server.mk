SERVER_SRCS += \
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
	game/q_shared.c \
	\
	ports/null/cl_null.c \
	ports/null/cd_null.c

ifeq ($(TARGET_OS),linux-gnu)
	SERVER_SRCS += \
		ports/linux/q_shlinux.c \
		ports/linux/sys_linux.c \
		ports/unix/sys_unix.c \
		ports/unix/glob.c \
		ports/unix/$(NET_UDP).c
else
	error "don't know about platform $(TARGET_OS)"
endif

ifeq ($(TARGET_OS),freebsd)
	SERVER_SRCS +=
		ports/linux/q_shlinux.c \
		ports/linux/sys_linux.c \
		ports/unix/sys_unix.c \
		ports/unix/glob.c \
		ports/unix/$(NET_UDP).c
endif

ifeq ($(OSTYPE),Darwin)
	SERVER_SRCS+=\
		ports/macosx/sys_osx.m \
		ports/unix/glob.c \
		ports/unix/sys_unix.c \
		ports/unix/$(NET_UDP).c \
		ports/macosx/q_shosx.c
endif

ifeq ($(HAVE_IPV6),1)
	# FIXME: flags!
	NET_UDP=net_udp6
else
	NET_UDP=net_udp
endif


SERVER_OBJS= \
	$(patsubst %.c, $(BUILDDIR)/server/%.o, $(filter %.c, $(SERVER_SRCS))) \
	$(patsubst %.m, $(BUILDDIR)/server/%.o, $(filter %.m, $(SERVER_SRCS)))

SERVER_DEPS=$(SERVER_OBJS:%.o=%.d)
SERVER_TARGET=ufo2ded$(EXE_EXT)

ifeq ($(BUILD_DEDICATED),1)
	ALL_OBJS+=$(SERVER_OBJS)
	ALL_DEPS+=$(SERVER_DEPS)
	TARGETS+=$(SERVER_TARGET)
endif

DEDICATED_CFLAGS=-DDEDICATED_ONLY

# Say how to like the exe
$(SERVER_TARGET): $(SERVER_OBJS) $(BUILDDIR)/.dirs
	@echo " * [DED] ... linking"; \
		$(CC) $(LDFLAGS) -o $@ $(SERVER_OBJS) $(SDL_LIBS)

# Say how to build .o files from .c files for this module
$(BUILDDIR)/server/%.o: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [DED] $<"; \
		$(CC) $(CFLAGS) $(DEDICATED_CFLAGS) -o $@ -c $<

# Say how to build .o files from .m files for this module
$(BUILDDIR)/server/%.m: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [DED] $<"; \
		$(CC) $(CFLAGS) $(DEDICATED_CFLAGS) -o $@ -c $<

# Say how to build the dependencies
ifdef BUILDDIR
$(BUILDDIR)/server/%.d: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [DEP] $<"; \
		$(DEP) $(DEDICATED_CFLAGS)
endif





