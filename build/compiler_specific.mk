
# FIXME
ifeq ($(CC),icc)
	RELEASE_CFLAGS+=
	CFLAGS+=-DHAVE_CONFIG_H
	DEP=/bin/false
else
	RELEASE_CFLAGS+=
	CFLAGS+=-DHAVE_CONFIG_H -Wall -pipe -Winline -Wcast-qual -Wcast-align -ansi
	DEP=$(CC) $(CFLAGS) -c $< -MM -MT "$(@:%.d=%.o) $@" -MF $@
#	-Wshadow -Wpointer-arith -Wmissing-prototypes -Wmissing-declarations \
#	-Wbad-function-cast -pedantic -std=c99
#	-combine -fwhole-program
endif
