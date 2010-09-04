SRCS = \
	$(CLIENT_SRCS) \
	$(GAME_SRCS) \
	$(SERVER_SRCS)

SPLINT_SRCS = \
	$(patsubst %.c, $(SRCDIR)/%.c, $(filter %.c, $(SRCS)))

splint:
	splint $(SPLINT_SRCS) -badflag $(CFLAGS) $(CLIENT_CFLAGS) $(SDL_CFLAGS)
