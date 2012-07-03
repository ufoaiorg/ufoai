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

#ifndef DBUFFER_H
#define DBUFFER_H

#include <sys/types.h>
#include <vector>
#include "../shared/sharedptr.h"

/**
 * @file
 * @brief Data buffers (dbuffer)
 */

/** @defgroup dbuffer Data buffers (dbuffer)
 * @ingroup datastructure
 * A dbuffer is a dynamically sized buffer that stores arbitrary bytes
 * in a queue. It does not provide random access; characters may be
 * inserted only at the end and removed only from the beginning.
 */

class dbuffer: public std::vector<char>
{
private:
	/** this is the length of the entries in the
	 * buffer, not the allocated size of the buffer */
	size_t _length;

public:
	dbuffer ();
	virtual ~dbuffer ();

	/* Append the given byte string to the buffer */
	void add (const char *, size_t);
	/* Read the given number of bytes from the start of the buffer */
	size_t get (char *, size_t) const;
	/* Read the given number of bytes from the given position */
	size_t getAt (size_t, char *, size_t) const;
	/* Remove the given number of bytes from the start of the buffer */
	size_t remove (size_t);
	/* Read and remove in one pass */
	size_t extract (char *, size_t);
	/* Duplicate the buffer */
	dbuffer *dup () const;

	size_t length () const;
};

typedef SharedPtr<dbuffer> dbufferptr;

inline size_t dbuffer::length () const
{
	return _length;
}

#define new_dbuffer() new dbuffer
#define free_dbuffer(dbuf) delete dbuf

/* Append the given byte string to the buffer */
#define dbuffer_add(dbuf, data, dataSize) (dbuf)->add((data), (dataSize))
/* Read the given number of bytes from the start of the buffer */
#define dbuffer_get(dbuf, data, dataSize) (dbuf)->get((data), (dataSize))
/* Read the given number of bytes from the given position */
#define dbuffer_get_at(dbuf, size, data, dataSize) (dbuf)->getAt((size), (data), (dataSize))
/* Remove the given number of bytes from the start of the buffer */
#define dbuffer_remove(dbuf, size) (dbuf)->remove((size))
/* Read and remove in one pass */
#define dbuffer_extract(dbuf, data, dataSize) (dbuf)->extract((data), (dataSize))
/* Duplicate the buffer */
#define dbuffer_dup(dbuf) (dbuf)->dup()

#define dbuffer_len(dbuf) (dbuf ? (dbuf)->length() : 0)

#endif
