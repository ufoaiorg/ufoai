/*
 * Copyright (c) 2010 ,
 * Cloud Wu . All rights reserved.
 *
 * http://www.codingnow.com
 *
 * Use, modification and distribution are subject to the "New BSD License"
 * as listed at <url: http://www.opensource.org/licenses/bsd-license.php >.
 */

#include "win_local.h"
#include "../../common/http.h"
#ifdef HAVE_BFD_H
#include "../../shared/bfd.h"

#include <excpt.h>
#include <imagehlp.h>
#include <psapi.h>

static void _backtrace (struct output_buffer *ob, struct bfd_set *set, int depth, LPCONTEXT context)
{
	char procname[MAX_PATH];
	struct bfd_ctx *bc = NULL;
	HANDLE process = GetCurrentProcess();
	HANDLE thread = GetCurrentThread();
	STACKFRAME frame;
	char symbol_buffer[sizeof(IMAGEHLP_SYMBOL) + 255];
	char module_name_raw[MAX_PATH];

	GetModuleFileNameA(NULL, procname, sizeof(procname));

	OBJZERO(frame);

	frame.AddrPC.Offset = context->Eip;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrStack.Offset = context->Esp;
	frame.AddrStack.Mode = AddrModeFlat;
	frame.AddrFrame.Offset = context->Ebp;
	frame.AddrFrame.Mode = AddrModeFlat;

	while (StackWalk(IMAGE_FILE_MACHINE_I386, process, thread, &frame, context,
			0, SymFunctionTableAccess, SymGetModuleBase, 0)) {
		const char * file = NULL;
		const char * func = NULL;
		unsigned line = 0;
		DWORD module_base = SymGetModuleBase(process, frame.AddrPC.Offset);
		const char * module_name = "[unknown module]";
		IMAGEHLP_SYMBOL *symbol = (IMAGEHLP_SYMBOL *) symbol_buffer;

		--depth;
		if (depth < 0)
			break;

		symbol->SizeOfStruct = sizeof(*symbol) + 255;
		symbol->MaxNameLength = 254;

		if (module_base && GetModuleFileNameA((HINSTANCE) module_base, module_name_raw, MAX_PATH)) {
			module_name = module_name_raw;
			bc = get_bc(ob, set, module_name);
		}

		if (bc) {
			find(bc, frame.AddrPC.Offset, &file, &func, &line);
		}

		if (file == NULL) {
			DWORD dummy = 0;
			if (SymGetSymFromAddr(process, frame.AddrPC.Offset, &dummy, symbol)) {
				file = symbol->Name;
			} else {
				file = "[unknown file]";
			}
		}
		if (func == NULL) {
			output_print(ob, "0x%x : %s : %s \n", frame.AddrPC.Offset,
					module_name, file);
		} else {
			output_print(ob, "0x%x : %s : %s (%d) : in function (%s) \n",
					frame.AddrPC.Offset, module_name, file, line, func);
		}
	}
}

static LPTOP_LEVEL_EXCEPTION_FILTER g_prev = NULL;
#define BUFFER_MAX (16*1024)
static char * g_output = NULL;

static LONG WINAPI exception_filter (LPEXCEPTION_POINTERS info)
{
	struct output_buffer ob;
	SYSTEMTIME timeInfo;
	OSVERSIONINFOEX osInfo;
	FILE* crash;
	const char *dumpFile = "crashdump.txt";
	int ret;

	GetSystemTime(&timeInfo);

	OBJZERO(osInfo);
	osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	if (!GetVersionEx((OSVERSIONINFO*)&osInfo)) {
		osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx((OSVERSIONINFO*)&osInfo);
	}

	output_init(&ob, g_output, BUFFER_MAX);

	if (!SymInitialize(GetCurrentProcess(), 0, TRUE)) {
		output_print(&ob, "Failed to init symbol context\n");
	} else {
		struct bfd_set *set = calloc(1, sizeof(*set));
		bfd_init();
		_backtrace(&ob, set, 128, info->ContextRecord);
		release_set(set);

		SymCleanup(GetCurrentProcess());
	}

	crash = fopen(dumpFile, "w");
	if (crash != NULL) {
		fprintf(crash, "======start======\n");
		fprintf(crash, "Date: %.4d-%.2d-%.2d\n",
				timeInfo.wYear, timeInfo.wMonth, timeInfo.wDay);
		fprintf(crash, "Windows version %d.%d (Build %d) %s\n",
				osInfo.dwMajorVersion, osInfo.dwMinorVersion, osInfo.dwBuildNumber, osInfo.szCSDVersion);
		fprintf(crash, BUILDSTRING ", cpu: " CPUSTRING ", version: " UFO_VERSION "\n\n");
		fprintf(crash, "%s", g_output);
		fprintf(crash, "======end========\n");
		fclose(crash);
	}
	fputs(g_output, stderr);

	ret = MessageBox(NULL, "Would you like to upload this crash dump and your ufoconsole.log? This will help the developers to fix the problem.", GAME_TITLE_LONG" Fatal Error", MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
	if (ret == IDYES)
		Com_UploadCrashDump(dumpFile);

	return 0;
}

static void backtrace_register (void)
{
	if (g_output == NULL) {
		g_output = malloc(BUFFER_MAX);
		g_prev = SetUnhandledExceptionFilter(exception_filter);
	}
}

static void backtrace_unregister (void)
{
	if (g_output) {
		free(g_output);
		SetUnhandledExceptionFilter(g_prev);
		g_prev = NULL;
		g_output = NULL;
	}
}
#endif

void Sys_BacktraceInit (void)
{
#ifdef HAVE_BFD_H
	backtrace_register();
#endif
}

void Sys_BacktraceShutdown (void)
{
#ifdef HAVE_BFD_H
	backtrace_unregister();
#endif
}
