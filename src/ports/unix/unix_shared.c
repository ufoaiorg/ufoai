/**
 * @file unix_files.c
 * @brief Some generic *nix file related functions
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <locale.h>
#include <signal.h>
#include <dirent.h>

#include "../../common/common.h"
#include "../system.h"

int Sys_Milliseconds (void)
{
	struct timeval tp;
	struct timezone tzp;
	static int secbase = 0;

	gettimeofday(&tp, &tzp);

	if (!secbase) {
		secbase = tp.tv_sec;
		return tp.tv_usec / 1000;
	}

	return (tp.tv_sec - secbase) * 1000 + tp.tv_usec / 1000;
}

/**
 * @brief set/unset environment variables (empty value removes it)
 */
int Sys_Setenv (const char *name, const char *value)
{
	if (value && value[0] != '\0')
		return setenv(name, value, 1);
	else
		return unsetenv(name);
}

void Sys_Sleep (int milliseconds)
{
#if 0
	struct timespec sleep, remaining;
	sleep.tv_sec = (long) milliseconds / 1000;
	sleep.tv_nsec = 1000000 * (milliseconds - (long) milliseconds);
	while (nanosleep(&sleep, &remaining) < 0 && errno == EINTR)
		/* If nanosleep has been interrupted by a signal, adjust the
		 * sleeping period and return to sleep.  */
		sleep = remaining;
#endif
	if (milliseconds < 1)
		milliseconds = 1;
	usleep(milliseconds * 1000);
}

#ifdef COMPILE_UFO
const char *Sys_SetLocale (const char *localeID)
{
	const char *locale;

# ifndef __sun
	unsetenv("LANGUAGE");
# endif /* __sun */
# ifdef __APPLE__
	if (localeID[0] != '\0') {
		if (Sys_Setenv("LANGUAGE", localeID) != 0)
			Com_Printf("...setenv for LANGUAGE failed: %s\n", localeID);
		if (Sys_Setenv("LC_ALL", localeID) != 0)
			Com_Printf("...setenv for LC_ALL failed: %s\n", localeID);
	}
# endif /* __APPLE__ */

	Sys_Setenv("LC_NUMERIC", "C");
	setlocale(LC_NUMERIC, "C");

	/* set to system default */
	setlocale(LC_ALL, "C");
	locale = setlocale(LC_MESSAGES, localeID);
	if (!locale) {
		if (Sys_Setenv("LANGUAGE", localeID) != 0) {
			locale = localeID;
		}
	}
	if (!locale) {
		Com_DPrintf(DEBUG_CLIENT, "...could not set to language: %s\n", localeID);
		locale = setlocale(LC_MESSAGES, "");
		if (!locale) {
			Com_DPrintf(DEBUG_CLIENT, "...could not set to system language\n");
		}
		return NULL;
	}

	Com_Printf("...using language: %s\n", locale);
	return locale;
}

const char *Sys_GetLocale (void)
{
	/* Calling with NULL param should return current system settings. */
	const char *currentLocale = setlocale(LC_MESSAGES, NULL);
	if (currentLocale != NULL && currentLocale[0] != '\0')
		return currentLocale;
	else
		return "C";
}
#endif

#ifdef COMPILE_UFO
void Sys_SetAffinityAndPriority (void)
{
	if (sys_affinity->modified) {
		sys_affinity->modified = qfalse;
	}

	if (sys_priority->modified) {
		sys_priority->modified = qfalse;
	}
}
#endif

#ifdef USE_SIGNALS
/**
 * @brief Catch kernel interrupts and dispatch the appropriate exit routine.
 */
static void Sys_Signal (int s)
{
	switch (s) {
	case SIGHUP:
	case SIGTERM:
	case SIGQUIT:
#ifndef COMPILE_UFO
	case SIGINT:
#endif
		Com_Printf("Received signal %d, quitting..\n", s);
		Sys_Quit();
		break;
#ifdef COMPILE_UFO
	case SIGINT:
		Com_Printf("Received signal %d, quitting..\n", s);
		Com_Quit();
		break;
#endif
	default:
		Sys_Error("Received signal %d.\n", s);
		break;
	}
}
#endif

void Sys_InitSignals (void)
{
#ifdef USE_SIGNALS
	signal(SIGHUP, Sys_Signal);
	signal(SIGINT, Sys_Signal);
	signal(SIGQUIT, Sys_Signal);
	signal(SIGILL, Sys_Signal);
	signal(SIGABRT, Sys_Signal);
	signal(SIGFPE, Sys_Signal);
	signal(SIGSEGV, Sys_Signal);
	signal(SIGTERM, Sys_Signal);
#endif
}
