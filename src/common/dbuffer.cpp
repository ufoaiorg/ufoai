/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "common.h"

dbuffer::dbuffer (int reserve) : _length(0)
{
	_data.reserve(reserve);
}

dbuffer::dbuffer (const dbuffer& other)
{
	_data = other._data;
	_length = other._length;
}

dbuffer::~dbuffer ()
{
}

void dbuffer::add (const char* data, size_t len)
{
	_data.insert(_data.begin() + _length, data, data + len);
	_length += len;
}

/**
 * @brief Read data from a dbuffer
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
size_t dbuffer::get (char* data, size_t len) const
{
	if (len > _length) {
		len = _length;
	}
	std::vector<char>::const_iterator copyEnd = _data.begin() + len;
	std::copy(_data.begin(), copyEnd, data);

	return len;
}

/**
 * @brief Read data from a dbuffer
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
size_t dbuffer::getAt (size_t offset, char* data, size_t len) const
{
	if (offset > _length)
		return 0;

	std::vector<char>::const_iterator copyBegin = _data.begin() + offset;
	len = std::min(len, _length - offset);
	std::vector<char>::const_iterator copyEnd = copyBegin + len;
	std::copy(copyBegin, copyEnd, data);

	return len;
}

/**
 * @brief Deletes data from a dbuffer
 * @param[in] len number of bytes to delete
 * Deletes the given number of bytes from the start of the dbuffer
 */
size_t dbuffer::remove (size_t len)
{
	if (len <= 0) {
		return 0;
	}

	if (len > _length) {
		len = _length;
	}
	std::vector<char>::iterator eraseEnd = _data.begin() + len;
	_data.erase(_data.begin(), eraseEnd);
	_length -= len;
	return len;
}

/**
 * @brief Read and delete data from a dbuffer
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
size_t dbuffer::extract (char* data, size_t len)
{
	len = get(data, len);
	remove(len);
	return len;
}
