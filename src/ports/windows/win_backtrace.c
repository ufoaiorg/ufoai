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
#include "../../common/common.h"
#ifdef HAVE_BFD_H
#include <excpt.h>
#include <imagehlp.h>
#include <bfd.h>
#include <psapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#define CURL_STATICLIB
#include <curl/curl.h>

#define BUFFER_MAX (16*1024)

struct bfd_ctx {
	bfd * handle;
	asymbol ** symbol;
};

struct bfd_set {
	char * name;
	struct bfd_ctx * bc;
	struct bfd_set *next;
};

struct find_info {
	asymbol **symbol;
	bfd_vma counter;
	const char *file;
	const char *func;
	unsigned line;
};

struct output_buffer {
	char * buf;
	size_t sz;
	size_t ptr;
};

static void output_init (struct output_buffer *ob, char * buf, size_t sz)
{
	ob->buf = buf;
	ob->sz = sz;
	ob->ptr = 0;
	ob->buf[0] = '\0';
}

static void output_print (struct output_buffer *ob, const char * format, ...)
{
	va_list ap;
	if (ob->sz == ob->ptr)
		return;
	ob->buf[ob->ptr] = '\0';
	va_start(ap, format);
	vsnprintf(ob->buf + ob->ptr, ob->sz - ob->ptr, format, ap);
	va_end(ap);

	ob->ptr = strlen(ob->buf + ob->ptr) + ob->ptr;
}

static void lookup_section (bfd *abfd, asection *sec, void *opaque_data)
{
	struct find_info *data = opaque_data;
	bfd_vma vma;

	if (data->func)
		return;

	if (!(bfd_get_section_flags(abfd, sec) & SEC_ALLOC))
		return;

	vma = bfd_get_section_vma(abfd, sec);
	if (data->counter < vma || vma + bfd_get_section_size(sec) <= data->counter)
		return;

	bfd_find_nearest_line(abfd, sec, data->symbol, data->counter - vma,
			&(data->file), &(data->func), &(data->line));
}

static void find (struct bfd_ctx * b, DWORD offset, const char **file, const char **func, unsigned *line)
{
	struct find_info data;
	data.func = NULL;
	data.symbol = b->symbol;
	data.counter = offset;
	data.file = NULL;
	data.func = NULL;
	data.line = 0;

	bfd_map_over_sections(b->handle, &lookup_section, &data);
	if (file) {
		*file = data.file;
	}
	if (func) {
		*func = data.func;
	}
	if (line) {
		*line = data.line;
	}
}

static int init_bfd_ctx (struct bfd_ctx *bc, const char * procname, struct output_buffer *ob)
{
	int r1, r2, r3;
	bfd *b;
	void *symbol_table;
	unsigned dummy = 0;

	bc->handle = NULL;
	bc->symbol = NULL;

	b = bfd_openr(procname, 0);
	if (!b) {
		output_print(ob, "Failed to open bfd from (%s)\n", procname);
		return 1;
	}

	r1 = bfd_check_format(b, bfd_object);
	r2 = bfd_check_format_matches(b, bfd_object, NULL);
	r3 = bfd_get_file_flags(b) & HAS_SYMS;

	if (!(r1 && r2 && r3)) {
		bfd_close(b);
		output_print(ob, "Failed to init bfd from (%s)\n", procname);
		return 1;
	}

	if (bfd_read_minisymbols(b, FALSE, &symbol_table, &dummy) == 0) {
		if (bfd_read_minisymbols(b, TRUE, &symbol_table, &dummy) < 0) {
			free(symbol_table);
			bfd_close(b);
			output_print(ob, "Failed to read symbols from (%s)\n", procname);
			return 1;
		}
	}

	bc->handle = b;
	bc->symbol = symbol_table;

	return 0;
}

static void close_bfd_ctx (struct bfd_ctx *bc)
{
	if (bc->symbol) {
		free(bc->symbol);
	}
	if (bc->handle) {
		bfd_close(bc->handle);
	}
}

static struct bfd_ctx *get_bc (struct output_buffer *ob, struct bfd_set *set, const char *procname)
{
	struct bfd_ctx bc;

	while (set->name) {
		if (strcmp(set->name, procname) == 0) {
			return set->bc;
		}
		set = set->next;
	}
	if (init_bfd_ctx(&bc, procname, ob)) {
		return NULL;
	}
	set->next = calloc(1, sizeof(*set));
	set->bc = malloc(sizeof(struct bfd_ctx));
	memcpy(set->bc, &bc, sizeof(bc));
	set->name = strdup(procname);

	return set->bc;
}

static void release_set (struct bfd_set *set)
{
	while (set->next) {
		struct bfd_set * temp = set->next;
		free(set->name);
		close_bfd_ctx(set->bc);
		free(set);
		set = temp;
	}
}

static void _backtrace (struct output_buffer *ob, struct bfd_set *set, int depth, LPCONTEXT context)
{
	char procname[MAX_PATH];
	struct bfd_ctx *bc = NULL;

	GetModuleFileNameA(NULL, procname, sizeof procname);

	STACKFRAME frame;
	memset(&frame, 0, sizeof(frame));

	frame.AddrPC.Offset = context->Eip;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrStack.Offset = context->Esp;
	frame.AddrStack.Mode = AddrModeFlat;
	frame.AddrFrame.Offset = context->Ebp;
	frame.AddrFrame.Mode = AddrModeFlat;

	HANDLE process = GetCurrentProcess();
	HANDLE thread = GetCurrentThread();

	char symbol_buffer[sizeof(IMAGEHLP_SYMBOL) + 255];
	char module_name_raw[MAX_PATH];

	while (StackWalk(IMAGE_FILE_MACHINE_I386, process, thread, &frame, context,
			0, SymFunctionTableAccess, SymGetModuleBase, 0)) {
		--depth;
		if (depth < 0)
			break;

		IMAGEHLP_SYMBOL *symbol = (IMAGEHLP_SYMBOL *) symbol_buffer;
		symbol->SizeOfStruct = (sizeof *symbol) + 255;
		symbol->MaxNameLength = 254;

		DWORD module_base = SymGetModuleBase(process, frame.AddrPC.Offset);

		const char * module_name = "[unknown module]";
		if (module_base && GetModuleFileNameA((HINSTANCE) module_base,
				module_name_raw, MAX_PATH)) {
			module_name = module_name_raw;
			bc = get_bc(ob, set, module_name);
		}

		const char * file = NULL;
		const char * func = NULL;
		unsigned line = 0;

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

static void Sys_UploadCrashDump (const char *crashDump)
{
	CURL *curl;
	struct curl_httppost* post = NULL;
	struct curl_httppost* last = NULL;

	curl = curl_easy_init();

	curl_formadd(&post, &last, CURLFORM_PTRNAME, "crashdump", CURLFORM_FILE, crashDump, CURLFORM_END);

	curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, GAME_TITLE" "UFO_VERSION);
	curl_easy_setopt(curl, CURLOPT_URL, "http://ufoai.ninex.info/CrashDump.php");

	curl_easy_perform(curl);

	curl_easy_cleanup(curl);
}

static char * g_output = NULL;
static LPTOP_LEVEL_EXCEPTION_FILTER g_prev = NULL;

static LONG WINAPI exception_filter (LPEXCEPTION_POINTERS info)
{
	struct output_buffer ob;
	SYSTEMTIME timeInfo;
	OSVERSIONINFOEX osInfo;
	FILE* crash;
	const char *dumpFile = "crashdump.txt";

	GetSystemTime(&timeInfo);

	memset(&osInfo, 0, sizeof(osInfo));
	osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	if (!GetVersionEx((OSVERSIONINFO*)&osInfo)) {
		osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx((OSVERSIONINFO*)&osInfo);
	}

	output_init(&ob, g_output, BUFFER_MAX);

	if (!SymInitialize(GetCurrentProcess(), 0, TRUE)) {
		output_print(&ob, "Failed to init symbol context\n");
	} else {
		bfd_init();
		struct bfd_set *set = calloc(1, sizeof(*set));
		_backtrace(&ob, set, 128, info->ContextRecord);
		release_set(set);

		SymCleanup(GetCurrentProcess());
	}

	crash = fopen(dumpFile, "a");
	if (crash != NULL) {
		fprintf(crash, "======start======\n");
		fprintf(crash, "Date: %.4d-%.2d-%.2d\n",
				timeInfo.wYear, timeInfo.wMonth, timeInfo.wDay);
		fprintf(crash, "Windows version %d.%d (Build %d) %s\n\n",
				osInfo.dwMajorVersion, osInfo.dwMinorVersion, osInfo.dwBuildNumber, osInfo.szCSDVersion);
		fprintf(crash, g_output);
		fprintf(crash, "======end========\n");
		fclose(crash);
	}
	fputs(g_output, stderr);

	Sys_UploadCrashDump(dumpFile);

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
