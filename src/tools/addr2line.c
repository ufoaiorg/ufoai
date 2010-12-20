/**
 * @file addr2line.c
 * @note Derived from addr2line.c and associated binutils files, version 2.18.
 */

#include <unistd.h>

#include <sys/stat.h>

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>

#pragma GCC visibility push(hidden)
#include "bfd.h"
#include "bucomm.h"
#include "demangle.h"
#include "libiberty.h"
#pragma GCC visibility pop

typedef struct _libtrace_data
{
	bfd_boolean unwind_inlines; /**< Unwind inlined functions. */
	bfd_boolean with_functions; /**< Show function names.  */
	bfd_boolean do_demangle; /**< Demangle names.  */
	bfd_boolean base_names; /**< Strip directory names.  */
	asymbol **syms; /**< Symbol table.  */

	bfd *abfd;
	asection *section;
} libtrace_data;

static libtrace_data m_libtrace_data = { .unwind_inlines = TRUE, .with_functions = TRUE, .do_demangle = TRUE,
		.base_names = TRUE, .syms = NULL, .abfd = NULL, .section = NULL, };

/** These variables are used to pass information between
 translate_addresses and find_address_in_section.  */
typedef struct _sym_info
{
	bfd_vma pc;
	const char *filename;
	const char *functionname;
	unsigned int line;
	bfd_boolean found;
} sym_info;

static int slurp_symtab (bfd *);
static void find_address_in_section (bfd *, asection *, void *);
static void find_offset_in_section (bfd *, asection *, sym_info *);
static int translate_addresses (bfd *abfd, asection *section, void *addr, char *buf_func, size_t buf_func_len,
		char *buf_file, size_t buf_file_len);

static const char *program_name = "addr2line";

void bfd_nonfatal (const char *string)
{
	const char *errmsg = bfd_errmsg(bfd_get_error());

	if (string)
		fprintf(stderr, "%s: %s: %s\n", program_name, string, errmsg);
	else
		fprintf(stderr, "%s: %s\n", program_name, errmsg);
}

void report (const char * format, va_list args)
{
	fprintf(stderr, "%s: ", program_name);
	vfprintf(stderr, format, args);
	putc('\n', stderr);
}

void non_fatal (const char *format, ...)
{
	va_list args;

	va_start(args, format);
	report(format, args);
	va_end(args);
}

/** Returns the size of the named file.  If the file does not
 * exist, or if it is not a real file, then a suitable non-fatal
 * error message is printed and zero is returned. */
off_t get_file_size (const char * file_name)
{
	struct stat statbuf;

	if (stat(file_name, &statbuf) < 0) {
		if (errno == ENOENT)
			non_fatal("'%s': No such file", file_name);
		else
			non_fatal("Warning: could not locate '%s'.  reason: %s", file_name, strerror(errno));
	} else if (!S_ISREG (statbuf.st_mode))
		non_fatal("Warning: '%s' is not an ordinary file", file_name);
	else
		return statbuf.st_size;

	return 0;
}

/** After a FALSE return from bfd_check_format_matches with
 * bfd_get_error () == bfd_error_file_ambiguously_recognized, print
 * the possible matching targets. */
void list_matching_formats (char **p)
{
	if (!p || !*p)
		return;

	fprintf(stderr, "%s: Matching formats: ", program_name);
	while (*p)
		fprintf(stderr, " %s", *p++);
	fputc('\n', stderr);
}

/** Set the default BFD target based on the configured target.  Doing
 * this permits the binutils to be configured for a particular target,
 * and linked against a shared BFD library which was configured for a
 * different target. */
void set_default_bfd_target (void)
{
	/* The macro TARGET is defined by Makefile.
	 E.g.: -DTARGET='"i686-pc-linux-gnu"'.  */
	const char *target = TARGET;

	if (!bfd_set_default_target(target)) {
		non_fatal("can't set BFD default target to `%s': %s", target, bfd_errmsg(bfd_get_error()));
		return;
	}

	return;
}

/** Read in the symbol table. */
static int slurp_symtab (bfd *abfd)
{
	long symcount;
	unsigned int size;

	if ((bfd_get_file_flags(abfd) & HAS_SYMS) == 0)
		return -1;

	symcount = bfd_read_minisymbols(abfd, FALSE, (void *) &m_libtrace_data.syms, &size);
	if (symcount == 0)
		symcount = bfd_read_minisymbols(abfd, TRUE /* dynamic */, (void *) &m_libtrace_data.syms, &size);

	if (symcount < 0) {
		bfd_nonfatal(bfd_get_filename(abfd));
		return -1;
	}

	return 0;
}

/** Look for an address in a section.  This is called via
 * bfd_map_over_sections. */
static void find_address_in_section (bfd *abfd, asection *section, void *data)
{
	bfd_vma vma;
	bfd_size_type size;
	sym_info *psi = (sym_info*) data;

	if (psi->found)
		return;

	if ((bfd_get_section_flags(abfd, section) & SEC_ALLOC) == 0)
		return;

	vma = bfd_get_section_vma(abfd, section);
	if (psi->pc < vma)
		return;

	size = bfd_get_section_size(section);
	if (psi->pc >= vma + size)
		return;

	psi->found = bfd_find_nearest_line(abfd, section, m_libtrace_data.syms, psi->pc - vma, &psi->filename,
			&psi->functionname, &psi->line);
}

/** Look for an offset in a section.  This is directly called. */
static void find_offset_in_section (bfd *abfd, asection *section, sym_info *psi)
{
	bfd_size_type size;

	if (psi->found)
		return;

	if ((bfd_get_section_flags(abfd, section) & SEC_ALLOC) == 0)
		return;

	size = bfd_get_section_size(section);
	if (psi->pc >= size)
		return;

	psi->found = bfd_find_nearest_line(abfd, section, m_libtrace_data.syms, psi->pc, &psi->filename,
			&psi->functionname, &psi->line);
}

/** Translate xaddr into
 * file_name:line_number and optionally function name. */
static int translate_addresses (bfd *abfd, asection *section, void *xaddr, char *buf_func, size_t buf_func_len,
		char *buf_file, size_t buf_file_len)
{
#define ADDR_BUF_LEN ((CHAR_BIT/4)*(sizeof(void*))+1)
	char addr[ADDR_BUF_LEN + 1] = { 0 };
	sym_info si = { 0 };

	sprintf(addr, "%p", xaddr);
	si.pc = bfd_scan_vma(addr, NULL, 16);

	si.found = FALSE;
	if (section)
		find_offset_in_section(abfd, section, &si);
	else
		bfd_map_over_sections(abfd, find_address_in_section, &si);

	if (!si.found) {
		if (buf_func != NULL)
			snprintf(buf_func, buf_func_len, "%s ??:0", m_libtrace_data.with_functions ? "??" : "");
	} else {
		do {
			if (m_libtrace_data.with_functions) {
				const char *name;
				char *alloc = NULL;

				name = si.functionname;
				if (name == NULL || *name == '\0')
					name = "??";
				else if (m_libtrace_data.do_demangle) {
					alloc = bfd_demangle(abfd, name, DMGL_ANSI | DMGL_PARAMS);
					if (alloc != NULL)
						name = alloc;
				}

				if (buf_func != NULL)
					snprintf(buf_func, buf_func_len, "%s", name);

				if (alloc != NULL)
					free(alloc);
			}

			if (m_libtrace_data.base_names && si.filename != NULL) {
				char *h = strrchr(si.filename, '/');
				if (h != NULL)
					si.filename = h + 1;
			}

			if (buf_file != NULL)
				snprintf(buf_file, buf_file_len, "%s:%u", si.filename ? si.filename : "??", si.line);
			if (!m_libtrace_data.unwind_inlines)
				si.found = FALSE;
			else
				si.found = bfd_find_inliner_info(abfd, &si.filename, &si.functionname, &si.line);
		} while (si.found);
	}

	return si.found;
}
/* --------------------------------------------------------------- */

int libtrace_init (const char *file_name, const char *section_name, const char *target)
{
	char **matching = NULL;

	bfd_init();
	set_default_bfd_target();

	if (get_file_size(file_name) < 1)
		return -1;

	m_libtrace_data.abfd = bfd_openr(file_name, target);
	if (m_libtrace_data.abfd == NULL) {
		bfd_nonfatal(file_name);
		return -1;
	}

	if (bfd_check_format(m_libtrace_data.abfd, bfd_archive)) {
		non_fatal("%s: cannot get addresses from archive", file_name);
		return -1;
	}

	if (!bfd_check_format_matches(m_libtrace_data.abfd, bfd_object, &matching)) {
		bfd_nonfatal(bfd_get_filename(m_libtrace_data.abfd));
		if (bfd_get_error() == bfd_error_file_ambiguously_recognized) {
			list_matching_formats(matching);
			free(matching);
		}
		return -1;
	}

	if (section_name != NULL) {
		m_libtrace_data.section = bfd_get_section_by_name(m_libtrace_data.abfd, section_name);
		if (m_libtrace_data.section == NULL)
			non_fatal("%s: cannot find section %s", file_name, section_name);
	} else
		m_libtrace_data.section = NULL;

	if (0 != slurp_symtab(m_libtrace_data.abfd))
		return -1;

	return 0;
}

int libtrace_close (void)
{
	if (m_libtrace_data.syms != NULL) {
		free(m_libtrace_data.syms);
		m_libtrace_data.syms = NULL;
	}

	bfd_close(m_libtrace_data.abfd);

	return 0;
}

int libtrace_resolve (void *addr, char *buf_func, size_t buf_func_len, char *buf_file, size_t buf_file_len, ...)
{
	int ret = FALSE;
	ret = translate_addresses(m_libtrace_data.abfd, m_libtrace_data.section, addr, buf_func, buf_func_len, buf_file,
			buf_file_len);
	assert(0 == ret);

	return 0;
}

/* --------------------------------------------------------------- */

#ifdef MAIN_FUNC
static void usage (FILE *stream, int status)
{
	fprintf(stream, "Usage: %s image addr <addr...>\n", program_name);
	fprintf(stream, "Ex: %s cpptraced 0x8048f0e 0x8048fd4 \n", program_name);
	/*
	 list_supported_targets(program_name, stream). e.g.:
	 elf32-i386    a.out-i386-linux  efi-app-ia32
	 elf32-little  elf32-big         elf64-x86-64
	 elf64-little  elf64-big srec    symbolsrec
	 tekhex        binary            ihex
	 trad-core
	 */
	exit(status);
}

int main (int argc, char **argv)
{
	const char *file_name = NULL;
	const char *section_name = NULL;
	char *target = NULL;
	int c;

#if defined (HAVE_SETLOCALE) && defined (HAVE_LC_MESSAGES)
	setlocale (LC_MESSAGES, "");
#endif
#if defined (HAVE_SETLOCALE)
	setlocale (LC_CTYPE, "");
#endif
	/*  bindtextdomain(PACKAGE, LOCALEDIR);*/
	/*  textdomain(PACKAGE);*/
	/*  expandargv(&argc, &argv); /*libiberty*/

	if (argc < 3) {
		usage(stderr, 1);
	}

	file_name = argv[1];

#define FUNC_MAX (PATH_MAX)
	if (0 == libtrace_init(file_name, section_name, target)) {
		int i;
		void *sym = NULL;
		char func[FUNC_MAX + 1] = { 0 };
		char file[PATH_MAX + 1] = { 0 };

		for (i = argc - 2; i < argc; i++) {
			sscanf(argv[i], "%p", &sym);
			libtrace_resolve(sym, func, FUNC_MAX, file, PATH_MAX);
			printf("%s [%s]\n", func, file);
		}

		libtrace_close();
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}
#endif /*MAIN_FUNC*/
