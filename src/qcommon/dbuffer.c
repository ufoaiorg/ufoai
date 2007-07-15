/************************************************************************
 *   Copyright (C) Andrew Suffield <asuffield@debian.org>
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

/** @file dbuffer.c
 * A dbuffer is a dynamically sized buffer that stores arbitrary bytes
 * in a queue. It does not provide random access; characters may be
 * inserted only at the end and removed only from the beginning.
 */

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "dbuffer.h"

#include <string.h>
#include <sys/types.h>
#include <assert.h>
#include <stdlib.h>

/* This should fit neatly in one page */
#define DBUFFER_ELEMENT_SIZE 4000
/* Free some buffers when we have this many free */
#ifdef DEBUG
#define DBUFFER_ELEMENTS_FREE_THRESHOLD 0
#define DBUFFER_FREE_THRESHOLD 0
#else
#define DBUFFER_ELEMENTS_FREE_THRESHOLD 1000
#define DBUFFER_FREE_THRESHOLD 1000
#endif

struct dbuffer_element
{
	struct dbuffer_element *next;
	char data[DBUFFER_ELEMENT_SIZE];
	size_t space, len;
};

static int free_elements = 0, allocated_elements = 0;
static struct dbuffer_element *free_element_list = NULL;

static int free_dbuffers = 0, allocated_dbuffers = 0;
static struct dbuffer *free_dbuffer_list = NULL;

static struct dbuffer_element * allocate_element (void)
{
	struct dbuffer_element *e;

	if (free_elements == 0) {
		struct dbuffer_element *new = malloc(sizeof(struct dbuffer_element));
		new->next = free_element_list;
		free_element_list = new;
		free_elements++;
		allocated_elements++;
	}
	assert(free_element_list);

	e = free_element_list;
	free_element_list = free_element_list->next;
	assert(free_elements > 0);
	free_elements--;
	e->space = DBUFFER_ELEMENT_SIZE;
	e->len = 0;
	e->next = NULL;
	return e;
}

static void free_element (struct dbuffer_element *e)
{
	e->next = free_element_list;
	free_element_list = e;
	free_elements++;
	while (free_elements > DBUFFER_ELEMENTS_FREE_THRESHOLD) {
		e = free_element_list;
		free_element_list = e->next;
		free(e);
		free_elements--;
		allocated_elements--;
	}
}

/** @brief Allocate a dbuffer
 * @return the newly allocated buffer
 * Allocates a new dbuffer and initialises it to be empty
 */
struct dbuffer * new_dbuffer (void)
{
	struct dbuffer *buf;
	if (free_dbuffers == 0) {
		struct dbuffer *new = malloc(sizeof(struct dbuffer));
		new->next_free = free_dbuffer_list;
		free_dbuffer_list = new;
		free_dbuffers++;
		allocated_dbuffers++;
	}
	assert(free_dbuffer_list);

	buf = free_dbuffer_list;
	free_dbuffer_list = free_dbuffer_list->next_free;
	assert(free_dbuffers > 0);
	free_dbuffers--;

	buf->len = 0;
	buf->space = DBUFFER_ELEMENT_SIZE;
	buf->head = buf->tail = allocate_element();
	buf->start = buf->end = &buf->head->data[0];

	return buf;
}

/** @brief Deallocate a dbuffer
 * @param buf the dbuffer to deallocate
 * Deallocates a dbuffer, and all memory it uses
 */
void free_dbuffer (struct dbuffer *buf)
{
	struct dbuffer_element *e;
	if (!buf)
		return;
	while ((e = buf->head)) {
		buf->head = e->next;
		free_element(e);
	}

	buf->next_free = free_dbuffer_list;
	free_dbuffer_list = buf;
	free_dbuffers++;

	while (free_dbuffers > DBUFFER_FREE_THRESHOLD) {
		buf = free_dbuffer_list;
		free_dbuffer_list = buf->next_free;
		free(buf);
		free_dbuffers--;
		allocated_dbuffers--;
	}
}

/** @brief This grows the buffer to provide at least enough space for the
 * given length, plus one character (see the comments in dbuffer_add()
 * for the explanation of that extra character).
 */
static void dbuffer_grow (struct dbuffer *buf, size_t len)
{
	struct dbuffer_element *e;
	while (buf->space <= len) {
		e = allocate_element();
		e->next = NULL;
		buf->tail->next = e;
		buf->tail = e;
		buf->space += DBUFFER_ELEMENT_SIZE;
	}
}

/** @brief Append data to a dbuffer
 * @param buf the target buffer
 * @param data pointer to the start of the bytes to add
 * @param len number of bytes to add
 * Adds the given sequence of bytes to the end of the buffer.
 */
void dbuffer_add (struct dbuffer *buf, const char *data, size_t len)
{
	struct dbuffer_element *e;

	if (!buf || !data || !len)
		return;

	e = buf->tail;
	/* We grow the buffer enough to ensure that we always have one extra
	* byte, so that buf->end has somewhere to point. The minor loss in
	* memory efficiency is offset by the code complexity this avoids
	*/
	if (len >= buf->space)
		dbuffer_grow(buf, len);
	while (len > 0) {
		size_t l = (len < e->space) ? len : e->space;
		memcpy(buf->end, data, l);
		data += l;
		len -= l;
		e->space -= l;
		e->len += l;
		buf->end += l;
		buf->len += l;
		buf->space -=l;
		if (e->space == 0) {
			e = e->next;
			buf->end = &e->data[0];
		}
	}
}

/** @brief Read data from a dbuffer
 * @param buf the source buffer
 * @param data pointer to where the data should be copied
 * @param len maximum number of bytes to copy
 * @return number of bytes copied
 *
 * @par
 * Copies up to @c len bytes into @c data
 *
 * @par
 * If the buffer does not contain at least @c len bytes, then as many
 * bytes as are present will be copied.
 */
size_t dbuffer_get (const struct dbuffer *buf, char *data, size_t len)
{
	size_t read = 0;
	/* element we're currently reading from */
	const struct dbuffer_element *e;
	/* position we're currently reading from */
	const void *p;

	if (!buf || !data || !len)
		return 0;

	e = buf->head;
	p = buf->start;
	while ((len > 0) && e && (e->len > 0)) {
		size_t l = (len < e->len) ? len : e->len;
		memcpy(data, p, l);
		data += l;
		len -= l;
		e = e->next;
		p = &e->data[0];
		read += l;
	}
	return read;
}

/** @brief Read data from a dbuffer
 * @param buf the source buffer
 * @param offset the offset in the source buffer where data should be copied from
 * @param data pointer to where the data should be copied
 * @param len maximum number of bytes to copy
 * @return number of bytes copied
 *
 * @par
 * Copies up to @c len bytes into @c data
 *
 * @par
 * If the buffer does not contain at least @c len bytes after offset,
 * then as many bytes as are present will be copied.
 */
size_t dbuffer_get_at (const struct dbuffer *buf, size_t offset, char *data, size_t len)
{
	size_t read = 0;
	/* element we're currently reading from */
	const struct dbuffer_element *e;
	/* position we're currently reading from */
	const void *p;

	if (!buf || !data || !len)
		return 0;

	e = buf->head;
	p = buf->start;

	while ((offset > 0) && e && (offset >= e->len)) {
		offset -= e->len;
		e = e->next;
		p = &e->data[0];
	}

	while ((len > 0) && e && (e->len > 0)) {
		size_t l = e->len - offset;
		if (len < l)
			l = len;
		p += offset;
		memcpy(data, p, l);
		data += l;
		len -= l;
		offset = 0;
		e = e->next;
		p = &e->data[0];
		read += l;
	}

	return read;
}

/** @brief Allocate a dbuffer
 * @param old the source buffer
 * @return the newly allocated buffer
 * Allocates a new dbuffer and initialises it to contain a copy of the
 * data in old
 */
struct dbuffer *dbuffer_dup (struct dbuffer *old)
{
	/* element we're currently reading from */
	const struct dbuffer_element *e;
	struct dbuffer *buf = new_dbuffer();
	const char *p;

	e = old->head;
	p = old->start;
	while (e && (e->len > 0)) {
		dbuffer_add(buf, p, e->len);
		e = e->next;
		p = &e->data[0];
	}

	return buf;
}

/** @brief Deletes data from a dbuffer
 * @param buf the target buffer
 * @param len number of bytes to delete
 * Deletes the given number of bytes from the start of the dbuffer
 */
size_t dbuffer_remove (struct dbuffer *buf, size_t len)
{
	size_t removed = 0;

	if (!buf  || !len)
		return 0;

	while ((len > 0) && (buf->len > 0)) {
		size_t l = (len < buf->head->len) ? len : buf->head->len;
		struct dbuffer_element *e = buf->head;
		buf->len -= l;
		buf->space += l;
		len -= l;
		e->len -= l;
		e->space += l;
		removed += l;
		buf->start += l;
		if (e->len == 0) {
			if (e->next) {
				buf->head = e->next;
				free_element(e);
				buf->space -= DBUFFER_ELEMENT_SIZE;
			} else {
				assert(buf->len == 0);
				buf->end = &buf->head->data[0];
			}
			buf->start = &buf->head->data[0];
		}
	}
	return removed;
}

/** @brief Read and delete data from a dbuffer
 * @param buf the source buffer
 * @param data pointer to where the data should be copied
 * @param len maximum number of bytes to copy
 * @return number of bytes copied
 *
 * @par
 * Copies up to @c len bytes into @c data, and removes them from the dbuffer
 *
 * @par
 * If the buffer does not contain at least @c len bytes, then as many
 * bytes as are present will be copied.
 *
 * @par
 * However many bytes are copied, exactly that many will be removed
 * from the start of the dbuffer.
 */
size_t dbuffer_extract (struct dbuffer *buf, char *data, size_t len)
{
	size_t extracted = 0;
	if (!buf || !data || !len)
		return 0;
	while ((len > 0) && (buf->len > 0)) {
		size_t l = (len < buf->head->len) ? len : buf->head->len;
		struct dbuffer_element *e = buf->head;
		memcpy(data, buf->start, l);
		data += l;
		len -= l;
		buf->len -= l;
		buf->space += l;
		e->len -= l;
		e->space += l;
		extracted += l;
		buf->start += l;
		if (e->len == 0) {
			if (e->next) {
				buf->head = e->next;
				free_element(e);
				buf->space -= DBUFFER_ELEMENT_SIZE;
			} else {
				assert(buf->len == 0);
				buf->end = &buf->head->data[0];
			}
			buf->start = &buf->head->data[0];
		}
	}
	return extracted;
}

/** @brief Locate the first occurance of a character in a dbuffer
 * @param buf dbuffer to search
 * @param c character to search for
 *
 * @return the offset of the first occurance of the character from the
 * start of the buffer, or -1 if the character does not occur in the
 * buffer
 */
ssize_t dbuffer_find_char (const struct dbuffer *buf, int c)
{
	ssize_t pos = 0;
	const struct dbuffer_element *e;

	if (!buf)
		return -1;

	e = buf->head;
	while(e) {
		const char *p, *start;
		if (e == buf->head)
			start = buf->start;
		else
			start = e->data;
		if ((p = memchr(start, c, e->len))) {
			pos += p - start;
			return pos;
		}
		pos += e->len;
		e = e->next;
	}
	return -1;
}
