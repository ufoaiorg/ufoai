#FIXME: Don't build with HAVE_GETTEXT - the server don't need this
#FIXME: check for -ldl (mingw doesn't have this)

SERVER_SRCS += \
	qcommon/cmd.c \
	qcommon/ioapi.c \
	qcommon/unzip.c \
	qcommon/cmodel.c \
	qcommon/common.c \
	qcommon/cvar.c \
	qcommon/files.c \
	qcommon/md4.c \
	qcommon/md5.c \
	qcommon/mem.c \
	qcommon/msg.c \
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
	game/inv_shared.c \
	\
	ports/null/cl_null.c \
	ports/null/cd_null.c

ifeq ($(HAVE_IPV6),1)
	# FIXME: flags!
	NET_UDP=net_udp6
else
	NET_UDP=net_udp
endif

ifneq ($(findstring $(TARGET_OS), netbsd freebsd linux-gnu),)
	SERVER_SRCS += \
		ports/linux/q_shlinux.c \
		ports/linux/sys_linux.c \
		ports/unix/sys_console.c \
		ports/unix/sys_unix.c \
		ports/unix/glob.c \
		ports/unix/$(NET_UDP).c
endif

ifeq ($(TARGET_OS),mingw32)
	SERVER_SRCS+=\
		ports/win32/q_shwin.c \
		ports/win32/sys_win.c \
		ports/win32/conproc.c  \
		ports/win32/net_wins.c \
		ports/win32/ufo.rc
endif

ifeq ($(TARGET_OS),darwin)
	SERVER_SRCS+=\
		ports/macosx/sys_osx.m \
		ports/unix/glob.c \
		ports/unix/sys_console.c \
		ports/unix/sys_unix.c \
		ports/unix/$(NET_UDP).c \
		ports/macosx/q_shosx.c
endif

ifeq ($(TARGET_OS),solaris)
	SERVER_SRCS += \
		ports/solaris/q_shsolaris.c \
		ports/solaris/sys_solaris.c \
		ports/unix/sys_console.c \
		ports/unix/sys_unix.c \
		ports/unix/glob.c \
		ports/unix/$(NET_UDP).c
endif

SERVER_OBJS= \
	$(patsubst %.c, $(BUILDDIR)/server/%.o, $(filter %.c, $(SERVER_SRCS))) \
	$(patsubst %.m, $(BUILDDIR)/server/%.o, $(filter %.m, $(SERVER_SRCS))) \
	$(patsubst %.rc, $(BUILDDIR)/server/%.o, $(filter %.rc, $(SERVER_SRCS)))

SERVER_TARGET=ufoded$(EXE_EXT)

ifeq ($(BUILD_DEDICATED),1)
	ALL_OBJS+=$(SERVER_OBJS)
	TARGETS+=$(SERVER_TARGET)
endif

DEDICATED_CFLAGS=-DDEDICATED_ONLY

ifeq ($(HAVE_CURSES),1)
	DEDICATED_CFLAGS+=-DHAVE_CURSES
endif

# Say how to link the exe
$(SERVER_TARGET): $(SERVER_OBJS) $(BUILDDIR)/.dirs
	@echo " * [DED] ... linking $(LNKFLAGS) ($(SERVER_LIBS))"; \
		$(CC) $(LDFLAGS) -o $@ $(SERVER_OBJS) $(SERVER_LIBS)

# Say how to build .o files from .c files for this module
$(BUILDDIR)/server/%.o: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [DED] $<"; \
		$(CC) $(CFLAGS) $(DEDICATED_CFLAGS) -o $@ -c $< -MD -MT $@ -MP

ifeq ($(TARGET_OS),mingw32)
# Say how to build .o files from .rc files for this module
$(BUILDDIR)/server/%.o: $(SRCDIR)/%.rc $(BUILDDIR)/.dirs
	@echo " * [RC ] $<"; \
		$(WINDRES) -DCROSSBUILD -i $< -o $@
endif

# Say how to build .o files from .m files for this module
$(BUILDDIR)/server/%.o: $(SRCDIR)/%.m $(BUILDDIR)/.dirs
	@echo " * [DED] $<"; \
		$(CC) $(CFLAGS) $(DEDICATED_CFLAGS) -o $@ -c $< -MD -MT $@ -MP
