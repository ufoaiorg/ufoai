/**
 * @file
 */

#pragma once

#include <sys/types.h>
#include <vector>
#include "../shared/sharedptr.h"


/** @defgroup dbuffer Data buffers (dbuffer)
 * @ingroup datastructure
 * A dbuffer is a dynamically sized buffer that stores arbitrary bytes
 * in a queue. It does not provide random access; characters may be
 * inserted only at the end and removed only from the beginning.
 */

class dbuffer
{
private:
	/** this is the length of the entries in the
	 * buffer, not the allocated size of the buffer */
	size_t _length;
	std::vector<char> _data;

public:
	dbuffer (int reserve = 512);
	dbuffer (const dbuffer& other);
	virtual ~dbuffer ();

	/* Append the given byte string to the buffer */
	void add (const char* , size_t);
	/* Read the given number of bytes from the start of the buffer */
	size_t get (char* , size_t) const;
	/* Read the given number of bytes from the given position */
	size_t getAt (size_t, char* , size_t) const;
	/* Remove the given number of bytes from the start of the buffer */
	size_t remove (size_t);
	/* Read and remove in one pass */
	size_t extract (char* , size_t);

	size_t length () const;
};

typedef SharedPtr<dbuffer> dbufferptr;

inline size_t dbuffer::length () const
{
	return _length;
}
