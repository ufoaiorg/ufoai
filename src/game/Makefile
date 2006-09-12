buildgame?=$(BUILDDIR)/game

game_hdr+=g_local.h

gameobjs+=$(buildgame)/g_client.o
gameobjs+=$(buildgame)/g_cmds.o
gameobjs+=$(buildgame)/g_main.o
gameobjs+=$(buildgame)/g_spawn.o
gameobjs+=$(buildgame)/g_svcmds.o
gameobjs+=$(buildgame)/g_utils.o
gameobjs+=$(buildgame)/q_shared.o


$(buildgame)/%.o: %.c %.h $(COMMON_HDR) $(game_hdr)
	@echo " * [GAME] $@"; $(CC) $(SHLIBCFLAGS) $(CFLAGS) -o $@ -c $<
