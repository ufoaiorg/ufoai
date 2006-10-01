###################################################################################################
# SDL
###################################################################################################

SND_SDL_SRCS=ports/linux/snd_sdl.c
SND_SDL_OBJS=$(SND_SDL_SRCS:%.c=$(BUILDDIR)/snd-sdl/%.o)
SND_SDL_DEPS=$(SND_SDL_OBJS:%.o=%.d)
SND_SDL_TARGET=snd_sdl.$(SHARED_EXT)

ifeq ($(HAVE_SND_SDL),1)
	TARGETS += $(SND_SDL_TARGET)
	ALL_DEPS += $(SND_SDL_DEPS)
	ALL_OBJS += $(SND_SDL_OBJS)
endif

# Say about to build the target
$(SND_SDL_TARGET) : $(SND_SDL_OBJS) $(BUILDDIR)/.dirs
	@echo " * [SDL] ... linking"; \
		$(LIBTOOL_LD) -o $(BUILDDIR)/libsnd_sdl.la $(SND_SDL_OBJS:%.o=%.lo)
	@cp $(BUILDDIR)/.libs/libsnd_sdl.$(SHARED_EXT) $@

# Say how to build .o files from .c files for this module
$(BUILDDIR)/snd-sdl/%.o: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [SDL] $<"; \
		$(LIBTOOL_CC) -o $@ -c $<

# Say how to build the dependencies
$(BUILDDIR)/snd-sdl/%.d: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [DEP] $<"; $(DEP)

###################################################################################################
# ALSA
###################################################################################################

SND_ALSA_SRCS=ports/linux/snd_alsa.c
SND_ALSA_OBJS=$(SND_ALSA_SRCS:%.c=$(BUILDDIR)/snd-alsa/%.o)
SND_ALSA_DEPS=$(SND_ALSA_OBJS:%.o=%.d)
SND_ALSA_TARGET=snd_alsa.$(SHARED_EXT)

ifeq ($(HAVE_SND_ALSA),1)
	TARGETS += $(SND_ALSA_TARGET)
	ALL_DEPS += $(SND_ALSA_DEPS)
	ALL_OBJS += $(SND_ALSA_OBJS)
endif

# Say about to build the target
$(SND_ALSA_TARGET) : $(SND_ALSA_OBJS) $(BUILDDIR)/.dirs
	@echo " * [ALSA] ... linking"; \
		$(LIBTOOL_LD) -o $(BUILDDIR)/libsnd_alsa.la $(SND_ALSA_OBJS:%.o=%.lo)
	@cp $(BUILDDIR)/.libs/libsnd_alsa.$(SHARED_EXT) $@

# Say how to build .o files from .c files for this module
$(BUILDDIR)/snd-alsa/%.o: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [ALSA] $<"; \
		$(LIBTOOL_CC) -o $@ -c $<

# Say how to build the dependencies
$(BUILDDIR)/snd-alsa/%.d: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [DEP] $<"; $(DEP)

###################################################################################################
# OSS
###################################################################################################

SND_OSS_SRCS=ports/linux/snd_oss.c
SND_OSS_OBJS=$(SND_OSS_SRCS:%.c=$(BUILDDIR)/snd-oss/%.o)
SND_OSS_DEPS=$(SND_OSS_OBJS:%.o=%.d)
SND_OSS_TARGET=snd_oss.$(SHARED_EXT)

ifeq ($(HAVE_SND_OSS),1)
	TARGETS += $(SND_OSS_TARGET)
	ALL_DEPS += $(SND_OSS_DEPS)
	ALL_OBJS += $(SND_OSS_OBJS)
endif

# Say about to build the target
$(SND_OSS_TARGET) : $(SND_OSS_OBJS) $(BUILDDIR)/.dirs
	@echo " * [OSS] ... linking"; \
		$(LIBTOOL_LD) -o $(BUILDDIR)/libsnd_oss.la $(SND_OSS_OBJS:%.o=%.lo)
	@cp $(BUILDDIR)/.libs/libsnd_oss.$(SHARED_EXT) $@

# Say how to build .o files from .c files for this module
$(BUILDDIR)/snd-oss/%.o: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [OSS] $<"; \
		$(LIBTOOL_CC) -o $@ -c $<

# Say how to build the dependencies
$(BUILDDIR)/snd-oss/%.d: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [DEP] $<"; $(DEP)

###################################################################################################
# ARTS
###################################################################################################

SND_ARTS_SRCS=ports/linux/snd_arts.c
SND_ARTS_OBJS=$(SND_ARTS_SRCS:%.c=$(BUILDDIR)/snd-arts/%.o)
SND_ARTS_DEPS=$(SND_ARTS_OBJS:%.o=%.d)
SND_ARTS_TARGET=snd_arts.$(SHARED_EXT)

ifeq ($(HAVE_SND_ARTS),1)
	TARGETS += $(SND_ARTS_TARGET)
	ALL_DEPS += $(SND_ARTS_DEPS)
	ALL_OBJS += $(SND_ARTS_OBJS)
endif

# Say about to build the target
$(SND_ARTS_TARGET) : $(SND_ARTS_OBJS) $(BUILDDIR)/.dirs
	@echo " * [ARTS] ... linking"; \
		$(LIBTOOL_LD) $(ARTS_LIBS) -o $(BUILDDIR)/libsnd_arts.la $(SND_ARTS_OBJS:%.o=%.lo)
	@cp $(BUILDDIR)/.libs/libsnd_arts.$(SHARED_EXT) $@

# Say how to build .o files from .c files for this module
$(BUILDDIR)/snd-arts/%.o: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [ARTS] $<"; \
		$(LIBTOOL_CC) $(ARTS_CFLAGS) -o $@ -c $<

# Say how to build the dependencies
$(BUILDDIR)/snd-arts/%.d: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [DEP] $<"; $(DEP) $(ARTS_CFLAGS)

