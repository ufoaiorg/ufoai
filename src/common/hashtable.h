/**
 * @file
 * @brief Header file for hashtable.
 */

/*
Copyright (C) 2002-2017 UFO: Alien Invasion.

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

/*
	use:

	hashTable* t = HASH_NewTable ();

	HASH_Insert ("A1", 123);
	HASH_Insert ("A2", 234);
	HASH_Insert ("A3", 345);

*/

#pragma once

#ifndef HASHTABLE_H
#define HASHTABLE_H

// forward declarations
struct memPool_t;

/**< The hash table */
struct hashTable_s;

/**< The hash uses a 256 byte table, so we define a index type here */
typedef unsigned short int HASH_INDEX;
/**< The hash function signature. */
typedef unsigned short int (*hashTable_hash) (const void* key, int len);
/**< The compare function signature. */
typedef int (*hashTable_compare) (const void* key1, int len1, const void* key2, int len2);

hashTable_s* HASH_NewTable (bool ownsKeys, bool ownsValues, bool duplicateOverwrite);
hashTable_s* HASH_NewTable (bool ownsKeys, bool ownsValues, bool duplicateOverwrite, hashTable_hash h, hashTable_compare c);
hashTable_s* HASH_NewTable (bool ownsKeys, bool ownsValues, bool duplicateOverwrite, hashTable_hash h, hashTable_compare c, memPool_t* keys, memPool_t* values, memPool_t* table);
hashTable_s* HASH_CloneTable (hashTable_s* source);
void HASH_DeleteTable (hashTable_s** t);

bool HASH_Insert (hashTable_s* t, const void* key, int nkey, const void* value, int nvalue);
void* HASH_Remove (hashTable_s* t, const void* key, int nkey);
void HASH_Clear (hashTable_s* t);
void* HASH_Get (hashTable_s* t, const void* key, int nkey);
int HASH_Count (hashTable_s* t);

#ifdef __DEBUG__
bool HASH_test ();
#endif // __DEBUG__

#endif // HASHTABLE_H


