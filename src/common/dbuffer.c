/**
 * @file dbuffer.c
 * @brief A dbuffer is a dynamically sized buffer that stores arbitrary bytes
 * in a queue. It does not provide random access; characters may be
 * inserted only at the end and removed only from the beginning.
 */

/*
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

#include "common.h"
#include "../shared/mutex.h"

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

struct dbuffer_element {
	struct dbuffer_element *next;
	char data[DBUFFER_ELEMENT_SIZE];
	size_t space, len;
};

static int free_elements = 0, allocated_elements = 0;
static struct dbuffer_element *free_element_list = NULL;

static int free_dbuffers = 0, allocated_dbuffers = 0;
static struct dbuffer *free_dbuffer_list = NULL;

static threads_mutex_t *dbuf_lock;

/**
 * @sa free_element
 */
static struct dbuffer_element * allocate_element (void)
{
	struct dbuffer_element *e;

	TH_MutexLock(dbuf_lock);

	if (free_elements == 0) {
		struct dbuffer_element *newBuf = (struct dbuffer_element *)Mem_PoolAlloc(sizeof(struct dbuffer_element), com_genericPool, 0);
		newBuf->next = free_element_list;
		free_element_list = newBuf;
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

	TH_MutexUnlock(dbuf_lock);

	return e;
}

/**
 * @sa allocate_element
 */
static void free_element (struct dbuffer_element *e)
{
	TH_MutexLock(dbuf_lock);

	e->next = free_element_list;
	free_element_list = e;
	free_elements++;
	while (free_elements > DBUFFER_ELEMENTS_FREE_THRESHOLD) {
		e = free_element_list;
		free_element_list = e->next;
		Mem_Free(e);
		free_elements--;
		allocated_elements--;
	}

	TH_MutexUnlock(dbuf_lock);
}

void dbuffer_cleanup (void)
{
	struct dbuffer_element *e;
	while ((e = free_element_list)) {
		free_element_list = e->next;
		Mem_Free(e);
		free_elements--;
		allocated_elements--;
	}
	assert(free_elements == 0);
}

/**
 * @brief Allocate a dbuffer
 * @return the newly allocated buffer
 * Allocates a new dbuffer and initialises it to be empty
 */
struct dbuffer * new_dbuffer (void)
{
	struct dbuffer *buf;

	TH_MutexLock(dbuf_lock);

	if (free_dbuffers == 0) {
		struct dbuffer *newBuf = (struct dbuffer *)Mem_PoolAlloc(sizeof(struct dbuffer), com_genericPool, 0);
		newBuf->next_free = free_dbuffer_list;
		free_dbuffer_list = newBuf;
		free_dbuffers++;
		allocated_dbuffers++;
	}
	assert(free_dbuffer_list);

	buf = free_dbuffer_list;
	free_dbuffer_list = free_dbuffer_list->next_free;
	assert(free_dbuffers > 0);
	free_dbuffers--;

	TH_MutexUnlock(dbuf_lock);

	buf->len = 0;
	buf->space = DBUFFER_ELEMENT_SIZE;
	buf->head = buf->tail = allocate_element();
	buf->start = buf->end = &buf->head->data[0];

	return buf;
}

/**
 * @brief Deallocate a dbuffer
 * @param[in,out] buf the dbuffer to deallocate
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

	TH_MutexLock(dbuf_lock);

	buf->next_free = free_dbuffer_list;
	free_dbuffer_list = buf;
	free_dbuffers++;

	/* now we should free them, as we are still having a lot of them allocated */
	while (free_dbuffers > DBUFFER_FREE_THRESHOLD) {
		buf = free_dbuffer_list;
		free_dbuffer_list = buf->next_free;
		Mem_Free(buf);
		free_dbuffers--;
		allocated_dbuffers--;
	}

	TH_MutexUnlock(dbuf_lock);
}

/**
 * @brief This grows the buffer to provide at least enough space for the
 * @param[in,out] buf the target buffer
 * @param[in] len number of bytes to add
 * given length, plus one character (see the comments in dbuffer_add()
 * for the explanation of that extra character).
 */
static inline void dbuffer_grow (struct dbuffer *buf, size_t len)
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

/**
 * @brief Merges two dbuffers
 * @param[in] old the source buffer
 * @param[in] old2 the second source buffer
 * @return the newly allocated buffer
 * Allocates a new dbuffer and initialises it to contain a copy of the
 * data in old ones
 */
struct dbuffer *dbuffer_merge (struct dbuffer *old, struct dbuffer *old2)
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

	e = old2->head;
	p = old2->start;
	while (e && (e->len > 0)) {
		dbuffer_add(buf, p, e->len);
		e = e->next;
		p = &e->data[0];
	}

	return buf;
}

/**
 * @brief Allocate a dbuffer and prepend the given data to it
 * @param[in] old The source buffer
 * @param[in] data The data to insert at the beginning
 * @param[in] len The length of that data
 * @return the newly allocated buffer
 * Allocates a new dbuffer and initialises it to contain a copy of the
 * data in old
 */
struct dbuffer *dbuffer_prepend (struct dbuffer *old, const char *data, size_t len)
{
	/* element we're currently reading from */
	const struct dbuffer_element *e;
	struct dbuffer *buf = new_dbuffer();
	const char *p;

	dbuffer_add(buf, data, len);

	e = old->head;
	p = old->start;
	while (e && (e->len > 0)) {
		dbuffer_add(buf, p, e->len);
		e = e->next;
		p = &e->data[0];
	}

	return buf;
}

/**
 * @brief Append data to a dbuffer
 * @param[in,out] buf the target buffer
 * @param[in] data pointer to the start of the bytes to add
 * @param[in] len number of bytes to add
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
	 * memory efficiency is offset by the code complexity this avoids */
	if (len >= buf->space)
		dbuffer_grow(buf, len);

	assert(len < buf->space);
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
			assert(e != NULL);
			buf->end = &e->data[0];
		}
	}
}

/**
 * @brief Read data from a dbuffer
 * @param[in] buf the source buffer
 * @param[out] data pointer to where the data should be copied
 * @param[in] len maximum number of bytes to copy
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
	while (len > 0 && e && e->len > 0) {
		const size_t l = (len < e->len) ? len : e->len;
		memcpy(data, p, l);
		data += l;
		len -= l;
		e = e->next;
		p = &e->data[0];
		read += l;
	}
	return read;
}

/**
 * @brief Read data from a dbuffer
 * @param[in] buf the source buffer
 * @param[in] offset the offset in the source buffer where data should be copied from
 * @param[out] data pointer to where the data should be copied
 * @param[in] len maximum number of bytes to copy
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
	const char *p;

	if (!buf || !data || !len)
		return 0;

	e = buf->head;
	p = buf->start;

	while (offset > 0 && e && offset >= e->len) {
		offset -= e->len;
		e = e->next;
		p = &e->data[0];
	}

	if (e == NULL || e->len <= 0)
		return 0;

	while (len > 0 && e && e->len > 0) {
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

/**
 * @brief Allocate a dbuffer
 * @param[in] old the source buffer
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

/**
 * @brief Deletes data from a dbuffer
 * @param[in,out] buf the target buffer
 * @param[in] len number of bytes to delete
 * Deletes the given number of bytes from the start of the dbuffer
 */
size_t dbuffer_remove (struct dbuffer *buf, size_t len)
{
	size_t removed = 0;

	if (!buf  || !len)
		return 0;

	while (len > 0 && buf->len > 0) {
		const size_t l = (len < buf->head->len) ? len : buf->head->len;
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

/**
 * @brief Read and delete data from a dbuffer
 * @param[in,out] buf the source buffer
 * @param[out] data pointer to where the data should be copied
 * @param[in] len maximum number of bytes to copy
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
	while (len > 0 && buf->len > 0) {
		const size_t l = (len < buf->head->len) ? len : buf->head->len;
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

void dbuffer_init (void)
{
	dbuf_lock = TH_MutexCreate("dbuffer");
}

void dbuffer_shutdown (void)
{
	TH_MutexDestroy(dbuf_lock);
	dbuf_lock = NULL;
}
