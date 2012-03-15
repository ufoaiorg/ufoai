/**
 * @file unix_main.c
 * @brief Some generic *nix functions
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

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#define MAX_BACKTRACE_SYMBOLS 50
#endif

#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif

#ifdef HAVE_LINK_H
#include <link.h>
#endif

#ifndef COMPILE_UFO
#undef HAVE_BFD_H
#endif
#include "../../shared/bfd.h"

const char *Sys_GetCurrentUser (void)
{
	static char s_userName[MAX_VAR];
	struct passwd *p;

	if ((p = getpwuid(getuid())) == NULL)
		s_userName[0] = '\0';
	else {
		strncpy(s_userName, p->pw_name, sizeof(s_userName));
		s_userName[sizeof(s_userName) - 1] = '\0';
	}
	return s_userName;
}

/**
 * @brief Errors out of the game.
 * @note The error message should not have a newline - it's added inside of this function
 * @note This function does never return
 */
void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char string[1024];

	Sys_Backtrace();

#ifdef COMPILE_UFO
	Sys_ConsoleShutdown();
#endif

	/* change stdin to non blocking */
	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);

#ifdef COMPILE_MAP
	Mem_Shutdown();
#endif

	va_start(argptr,error);
	Q_vsnprintf(string, sizeof(string), error, argptr);
	va_end(argptr);

	fprintf(stderr, "Error: %s\n", string);

	exit(1);
}

/**
 * @brief
 * @sa Qcommon_Shutdown
 */
void Sys_Quit (void)
{
#ifdef COMPILE_UFO
	CL_Shutdown();
	Qcommon_Shutdown();
	Sys_ConsoleShutdown();
#elif COMPILE_MAP
	Mem_Shutdown();
#endif

	fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) & ~FNDELAY);
	exit(0);
}

#ifdef HAVE_LINK_H
static int Sys_BacktraceLibsCallback (struct dl_phdr_info *info, size_t size, void *data)
{
	int j;
	int end;
	FILE *crash = (FILE*)data;

	end = 0;

	if (!info->dlpi_name || !info->dlpi_name[0])
		return 0;

	for (j = 0; j < info->dlpi_phnum; j++) {
		end += info->dlpi_phdr[j].p_memsz;
	}

	fprintf(crash, "[0x"UFO_SIZE_T"x-0x"UFO_SIZE_T"x] %s\n", info->dlpi_addr, info->dlpi_addr + end, info->dlpi_name);
	return 0;
}

#endif

#ifdef HAVE_BFD_H

/* following code parts are taken from libcairo */

struct file_match {
	const char *file;
	void *address;
	void *base;
	void *hdr;
};

#define BUFFER_MAX (16*1024)
static char g_output[BUFFER_MAX];

static int find_matching_file (struct dl_phdr_info *info, size_t size, void *data)
{
#ifdef ElfW
	typedef ElfW(Phdr) Elf_Phdr;
	typedef ElfW(Addr) Elf_Addr;
#endif

	struct file_match *match = data;
	/* This code is modeled from Gfind_proc_info-lsb.c:callback() from libunwind */
	long n;
	Elf_Addr const  load_base = info->dlpi_addr;
	Elf_Phdr const *phdr      = info->dlpi_phdr;
	for (n = info->dlpi_phnum; --n >= 0; phdr++) {
		if (phdr->p_type == PT_LOAD) {
			Elf_Addr vaddr = phdr->p_vaddr + load_base;
			if (match->address >= (void*)vaddr && match->address < (void*)(vaddr + phdr->p_memsz)) {
				/* we found a match */
				match->file = info->dlpi_name;
				match->base = (void*)info->dlpi_addr;
			}
		}
	}
	return 0;
}

static void _backtrace (FILE *crash, void * const *buffer, int size)
{
	int x;
	struct bfd_set *set = calloc(1, sizeof(*set));
	struct output_buffer ob;
	struct bfd_ctx *bc = NULL;

	output_init(&ob, g_output, sizeof(g_output));

	bfd_init();

	for (x = 0; x < size; x++) {
		struct file_match match = {.address = buffer[x]};
		unsigned long addr;
		const char * file = NULL;
		const char * func = NULL;
		unsigned line = 0;
		const char *procname;

		dl_iterate_phdr(find_matching_file, &match);
		addr = (char *)buffer[x] - (char *)match.base;

		if (match.file && strlen(match.file))
			procname = match.file;
		else
			procname = "/proc/self/exe";

		bc = get_bc(&ob, set, procname);
		if (bc) {
			find(bc, addr, &file, &func, &line);
			procname = bc->handle->filename;
		}

		if (func == NULL) {
			output_print(&ob, "0x%x : %s : %s \n",
					addr, procname == NULL ? "unknown" : procname, file == NULL ? "unknown" : file);
		} else {
			output_print(&ob, "0x%x : %s : %s (%d) : in function (%s) \n",
					addr, procname == NULL ? "unknown" : procname, file == NULL ? "unknown" : file, line, func);
		}
	}

	fprintf(crash, "%s", g_output);

	release_set(set);
}

#endif

/**
 * @brief On platforms supporting it, print a backtrace.
 */
void Sys_Backtrace (void)
{
	const char *dumpFile = "crashdump.txt";
	FILE *file = fopen(dumpFile, "w");
	FILE *crash = file != NULL ? file : stderr;

	fprintf(crash, "======start======\n");

#ifdef HAVE_SYS_UTSNAME_H
	struct utsname	info;
	uname(&info);
	fprintf(crash, "OS Info: %s %s %s %s %s\n", info.sysname, info.nodename, info.release, info.version, info.machine);
#endif

	fprintf(crash, BUILDSTRING ", cpu: " CPUSTRING ", version: " UFO_VERSION "\n\n");
	fflush(crash);

#ifdef HAVE_EXECINFO_H
	void *symbols[MAX_BACKTRACE_SYMBOLS];
	const int i = backtrace(symbols, MAX_BACKTRACE_SYMBOLS);
#ifdef HAVE_BFD_H
	_backtrace(crash, symbols, i);
#else
	backtrace_symbols_fd(symbols, i, fileno(crash));
#endif
#endif

#ifdef HAVE_LINK_H
	fprintf(crash, "Loaded libraries:\n");
	dl_iterate_phdr(Sys_BacktraceLibsCallback, crash);
#endif

	fprintf(crash, "======end========\n");

	if (file != NULL)
		fclose(file);

#ifdef COMPILE_UFO
	Com_UploadCrashDump(dumpFile);
#endif
}
