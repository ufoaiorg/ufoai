/************************************************************************
 *   Copyright (C) Andrew Suffield <asuffield@suffields.me.uk>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifdef DBUFFER_H
struct dbuffer;
#else
#define DBUFFER_H

#include <sys/types.h>

/** @file dbuffer.h
 * @brief Data buffers (struct dbuffer)
 */

/** @defgroup dbuffer Data buffers (struct dbuffer)
 * @ingroup datastructure
 * A dbuffer is a dynamically sized buffer that stores arbitrary bytes
 * in a queue. It does not provide random access; characters may be
 * inserted only at the end and removed only from the beginning.
 */

struct dbuffer_element;

/** @brief Opaque dbuffer struct
 * @internal
 * Stores pointers that describe the dbuffer (the actual data is kept
 * in the struct dbuffer_element members)
 *
 * When len == 0, then start == end and head == tail.
 * When start == end, then len == 0
 */
struct dbuffer
{
	/** @internal Current number of bytes stored in the buffer */
	size_t len;

	/** @internal Current number of unused bytes allocated to this buffer
	* (All unused bytes are at the end of the buffer) */
	size_t space;

	/** @internal Pointer to the first dbuffer_element allocated to this buffer */
	struct dbuffer_element *head;

	/** @internal Pointer to the last dbuffer_element allocated to this buffer */
	struct dbuffer_element *tail;

	/** @internal Pointer to the first byte stored in the buffer, or to
	* the first free space if there are no bytes in the buffer at
	* present
	*
	* This should always point into head */
	char *start;

	/** @internal Pointer to the first unused byte allocated to this
	* buffer, and therefore to the byte immediately after the last
	* byte stored
	*
	* When start == end, the buffer is empty.
	*
	* This should always point into tail. Thusly head == tail when the
	* buffer is empty. */
	char *end;

	/** @internal Pointer to the next free buffer (or NULL if this
	*  buffer is not free) */
	struct dbuffer *next_free;
};

extern struct dbuffer *new_dbuffer(void) __attribute__((malloc));
extern void free_dbuffer(struct dbuffer *);

/* Append the given byte string to the buffer */
extern void dbuffer_add(struct dbuffer *, const char *, size_t);
/* Read the given number of bytes from the start of the buffer */
extern size_t dbuffer_get(const struct dbuffer *, char *, size_t);
/* Read the given number of bytes from the given position */
extern size_t dbuffer_get_at(const struct dbuffer *, size_t, char *, size_t);
/* Remove the given number of bytes from the start of the buffer */
extern size_t dbuffer_remove(struct dbuffer *, size_t);
/* Read and remove in one pass */
extern size_t dbuffer_extract(struct dbuffer *, char *, size_t);
/* Duplicate the buffer */
extern struct dbuffer *dbuffer_dup(struct dbuffer *);
/* Duplicate the buffer and prepend the given data */
extern struct dbuffer *dbuffer_prepend(struct dbuffer *old, const char *data, size_t len);
/* Merges two dbuffers */
struct dbuffer *dbuffer_merge(struct dbuffer *old, struct dbuffer *old2);

void dbuffer_shutdown(void);
void dbuffer_init(void);
void dbuffer_cleanup(void);

#define dbuffer_len(A) (A ? (A)->len : 0)

#endif
