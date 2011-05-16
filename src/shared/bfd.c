/**
 * @file bfd.c
 * @brief Stacktrace functions (libbfd/binutils)
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

Copyright (C) 1997-2001 Id Software, Inc.

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

#include "../common/common.h"

#ifdef HAVE_BFD_H
#include "bfd.h"

void output_init (struct output_buffer *ob, char * buf, size_t sz)
{
	ob->buf = buf;
	ob->sz = sz;
	ob->ptr = 0;
	ob->buf[0] = '\0';
}

void output_print (struct output_buffer *ob, const char * format, ...)
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

void find (struct bfd_ctx * b, size_t offset, const char **file, const char **func, unsigned *line)
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

static void list_matching_formats (struct output_buffer *ob, const char * procname, char **p)
{
	if (!p || !*p)
		return;

	output_print(ob, "%s: Matching formats: ", procname);
	while (*p)
		output_print(ob, " %s", *p++);
	output_print(ob, "\n");
}

static int init_bfd_ctx (struct bfd_ctx *bc, const char * procname, struct output_buffer *ob)
{
	bfd *b;
	void *symbol_table;
	unsigned dummy = 0;
	char **matching = NULL;

	bc->handle = NULL;
	bc->symbol = NULL;

	b = bfd_openr(procname, 0);
	if (!b) {
		output_print(ob, "Failed to open bfd from (%s)\n", procname);
		return 1;
	}

	if (bfd_check_format(b, bfd_archive)) {
		output_print(ob, "Cannot get addresses from archive (%s)\n", b->filename);
		bfd_close(b);
		return -1;
	}

	if (!bfd_check_format_matches(b, bfd_object, &matching)) {
		const char *errmsg = bfd_errmsg(bfd_get_error());
		output_print(ob, "%s (%s)\n", errmsg, b->filename);
		if (bfd_get_error() == bfd_error_file_ambiguously_recognized) {
			list_matching_formats(ob, b->filename, matching);
			free(matching);
		}
		bfd_close(b);
		return -1;
	}

	if ((bfd_get_file_flags(b) & HAS_SYMS) == 0) {
		const char *errmsg = bfd_errmsg(bfd_get_error());
		output_print(ob, "Failed to get file flags from (%s) %s\n", b->filename, errmsg);
		bfd_close(b);
		return 1;
	}

	if (bfd_read_minisymbols(b, FALSE, &symbol_table, &dummy) == 0) {
		if (bfd_read_minisymbols(b, TRUE, &symbol_table, &dummy) < 0) {
			const char *errmsg = bfd_errmsg(bfd_get_error());
			output_print(ob, "Failed to read symbols from (%s): %s\n", b->filename, errmsg);
			free(symbol_table);
			bfd_close(b);
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

struct bfd_ctx *get_bc (struct output_buffer *ob, struct bfd_set *set, const char *procname)
{
	struct bfd_ctx bc;

	while (set->name) {
		if (Q_streq(set->name, procname)) {
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

void release_set (struct bfd_set *set)
{
	while (set->next) {
		struct bfd_set * temp = set->next;
		free(set->name);
		close_bfd_ctx(set->bc);
		free(set);
		set = temp;
	}
}

#endif
