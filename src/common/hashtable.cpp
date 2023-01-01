/**
 * @file
 * @brief Basic hash table for fast lookups.
 */

/*
Copyright (C) 2002-2023 UFO: Alien Invasion.

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

#include "hashtable.h"

#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "common.h"
#include "mem.h"

/*
	The memory structure being build by the hash table looks like this:

	T = hashTable_s
	B = hashBucket_s
	I = hashItem_s
	0 = NULL

	[T]
	  ...
	  [0]
	  [B]
	    [I]->[0]
	  [B]
	    [I]->[0]
	  [0]
	  ...
	  [B]
		[I]->[I]->[I]->[0]
	  [0]
	  ...
*/

// the hash table index size */
#define HASH_TABLE_SIZE 256

// forward declarations
struct hashBucket_s;

/**
 * @brief The linked list item.
 */
struct hashItem_s {
	/**< The key stored in this entry. */
    void* key;
    /**< The key length in bytes. */
    int nkey;
    /**< The value stored in this entry. */
    void* value;
    /**< The value lenght in bytes. */
    int nvalue;
    /**< Points to the next item in the list, or NULL. */
	hashItem_s* next;
	/**< Points to the bucket holding the list. */
	hashBucket_s* root;
};

/**
 * @brief The hash bucket structure, contains a linked list of items.
 */
struct hashBucket_s {
	/**< The number of items in this bucket. */
	int count;
	/**< The first item in the linked list. */
	hashItem_s* root;
};

/**
 * @brief The hash table structure, contains an array of buckets being indexed by the hash function.
 */
struct hashTable_s {
	/**< The initial array of buckets. */
	hashBucket_s* table[HASH_TABLE_SIZE];
	/**< The hash function for this table. */
	hashTable_hash hash;
    /**< The compare function for this table. */
    hashTable_compare compare;
    /**< If true, the hash table creates a copy of the key. */
    bool ownsKeys;
    /**< If true, the hash table creates a copy of the value. */
    bool ownsValues;
    /**< If a (key,value) pair is inserted and there already is a (key,value) pair, then if this is set to
	true (the default), the value is overwritten by the new value, else the operation asserts. */
    bool duplicateOverwrite;
    /**< Memory pool used to allocate keys. */
    memPool_t* keyPool;
    /**< Memory pool used to duplicate item values, in case the table owns the values. */
    memPool_t* valuePool;
    /**< Memory pool used to allocate internal table structures, including the table structure. */
    memPool_t* internalPool;
};

#ifdef __DEBUG__
static unsigned long _num_allocs = 0u;
#endif // __DEBUG__

static void* _hash_alloc (memPool_t* pool, int n, size_t s) {
	#ifdef __DEBUG__
	_num_allocs++;
	#endif // __DEBUG__
	if (pool) return Mem_PoolAlloc (n * s, pool, 0);
	return Mem_Alloc(n * s);
}

static void _hash_free (void* p) {
	#ifdef __DEBUG__
	_num_allocs--;
	#endif // __DEBUG__
	Mem_Free(p);
}

/**
 * @brief Default hash function to map keys into a 0..255 index.
 * @param[in] key A pointer to a key value.
 * @param[in] len The length of the key value.
 * @note The algorithm is based on Fowler-Noll-Vo.
 */
static HASH_INDEX default_hash(const void* key, int len) {
    const unsigned char* p = (const unsigned char*) key;
    unsigned int h = 2166136261u;
    int i;

    for (i = 0; i < len; i++) {
        h = (h*16777619u) ^ p[i];
    }

    return (h % HASH_TABLE_SIZE);
}

/**
 * @brief Default compare function for comparing key values.
 * @param[in] key1 The first key value to compare.
 * @param[in] len1 The length in bytes of the first key value.
 * @param[in] key2 The second key value to compare.
 * @param[in] len2 The length in bytes of the second key value.
 * @return A value of 0 if key1 == key2, -1 if key1 < key2 and +1 if key1 > key2.
 * @note The default compare uses byte-by-byte comparison. If the key value is a basic data type, a custom
 * compare function should be supplied.
 */
static int default_compare(const void* key1, int len1, const void* key2, int len2) {
	int n = (len1 < len2) ? len1 : len2;
	return memcmp(key1, key2, n);
}

/**
 * @brief Internal function to scan the list of entries in the bucket for an exact match.
 * @param[in] b The bucket to search in.
 * @param[in] c The compare function to use.
 * @param[in] key The key value to search for.
 * @param[in] nkey The lenght of the key value.
 * @param[out] prev A reference to a hashItem_s pointer for the entry preceding the found item. Set to NULL if
 * the preceding entry is not needed.
 * @return If an exact match is found, a hashItem_s* to the entry, else NULL.
 * @note The value of prev is set to NULL if there is no preceding item.
 */
static hashItem_s* _find_bucket_entry (hashBucket_s* b, hashTable_compare c, const void* key, int nkey, hashItem_s** prev) {
	hashItem_s* p = NULL; // points to preceding item in linked list or NULL if there is none
	hashItem_s* i = b->root; // points to current item in linked list
	while (i) {
        if (c(key, nkey, i->key, i->nkey) == 0) {
				// return preceding item only if reference for this value is supplied
				if (prev) (*prev) = p;
				return i;
        }
        p = i;
        i = i->next;
	}
	if (prev) (*prev) = NULL;
	return NULL;
}

/**
 * @brief Internal function, release an entry including key and value if owned.
 */
static void _release_entry (hashItem_s** i, bool ownsKeys, bool ownsValues) {
	if (ownsKeys) _hash_free ((*i)->key);
	if (ownsValues) _hash_free ((*i)->value);
	_hash_free (*i);
	(*i)=NULL;
}

/**
 * @brief Internal function, releases bucket including entries.
 */
static void _release_bucket (hashBucket_s** b, bool ownsKeys, bool ownsValues) {
    // delete entries
    hashItem_s* i = (*b)->root;
    while (i) {
		hashItem_s* p=i;
		i = i->next;
		_release_entry(&p, ownsKeys, ownsValues);
	}
	(*b)->root = NULL;
	// delete the bucket
	_hash_free (*b);
	(*b) = NULL;
}

/**
 * @brief Internal function, releases entire table.
 * @param[out] t table pointer to release
 * @param[in] full If true, the tabel node is also freed.
 */
static void _release_table (hashTable_s** t, bool full) {
	// delete buckets
	for (int i=0; i<HASH_TABLE_SIZE; i++) {
		if ((*t)->table[i]) _release_bucket(&((*t)->table[i]), (*t)->ownsKeys, (*t)->ownsValues);
	}
	if (full) {
		// delete table
		_hash_free ((*t));
		(*t) = NULL;
	}
}
/**
 * @brief start iterating with the first item.
 * @note the order is not necessarily sorted
 */
static hashItem_s* _iterator_first (hashTable_s* t) {
	if (t) {
		for (int ti = 0; ti < HASH_TABLE_SIZE; ti++) {
			if (t->table[ti]) return t->table[ti]->root;
		}
	}
	return nullptr;
}

/**
 * @brief iterate to next item
 * @note the order is not necessarily sorted
 */
static hashItem_s* _iterator_next (hashTable_s* t, hashItem_s* i) {
	if (i) {
		if (i->next) {
			return i->next;
		}
		// compute the hash key of the last item iterated
		int h = t->hash(i->key, i->nkey);
		// now get the next filled bucket in the array
		h++;
		while (h < HASH_TABLE_SIZE) {
			hashBucket_s* b = (t->table)[h];
			if (b) {
				return b->root;
			}
			h++;
		}
	}
	return nullptr;
}

/**
 * @brief Creates a new hash table and sets it initial capacity.
 * @param[in] ownsKeys If true, the hash table stores a copy of the key.
 * @param[in] ownsValues If true, the hash table stores a copy of the value.
 * @param[in] duplicateOverwrite If true, inserting a value twice will overwrite the old value. If false, the
 * operation asserts.
 * @return A pointer to a hashTable_s.
 * @note The table hash an initial index of HASH_TABLE_SIZE buckets.
 * @note The hash table will use the default hash function and default compare function.
 */
hashTable_s* HASH_NewTable (bool ownsKeys, bool ownsValues, bool duplicateOverwrite) {
	return HASH_NewTable (ownsKeys, ownsValues, duplicateOverwrite, &default_hash, &default_compare);
}


/**
 * @brief Creates a new hash table and sets it initial capacity.
 * @param[in] ownsKeys If true, the hash table stores a copy of the key.
 * @param[in] ownsValues If true, the hash table stores a copy of the value.
 * @param[in] duplicateOverwrite If true, inserting a value twice will overwrite the old value. If false, the
 * operation asserts.
 * @param[in] h The hash function to be used when indexing.
 * @param[in] c The compare function to be used when comparing.
 * @return A pointer to a hashTable_s.
 * @note The table hash an initial index of HASH_TABLE_SIZE buckets.
 */
hashTable_s* HASH_NewTable (bool ownsKeys, bool ownsValues, bool duplicateOverwrite, hashTable_hash h, hashTable_compare c) {
	return HASH_NewTable(ownsKeys, ownsValues, duplicateOverwrite, h, c, nullptr, nullptr, nullptr);
}

/**
 * @brief Creates a new hash table and sets it initial capacity.
 * @param[in] ownsKeys If true, the hash table stores a copy of the key.
 * @param[in] ownsValues If true, the hash table stores a copy of the value.
 * @param[in] duplicateOverwrite If true, inserting a value twice will overwrite the old value. If false, the
 * operation asserts.
 * @param[in] h The hash function to be used when indexing.
 * @param[in] c The compare function to be used when comparing.
 * @param[in] keys If not null, should point to a memory pool used to allocate key values.
 * @param[in] values If not null, should point to a memory pool used to allocated copies of inserted values.
 * @param[in] table If not null, should point to a memory pool used to allocate the table internal structures, including the
 * hashTable_s structure returned.
 * @return A pointer to a hashTable_s.
 * @note The table hash an initial index of HASH_TABLE_SIZE buckets.
 */
hashTable_s* HASH_NewTable (bool ownsKeys, bool ownsValues, bool duplicateOverwrite, hashTable_hash h, hashTable_compare c, memPool_t* keys, memPool_t* values, memPool_t* table) {
	// allocate table
	hashTable_s* t = (hashTable_s*) _hash_alloc (table, 1, sizeof(hashTable_s));
	t->hash = h;
	t->compare = c;
	t->ownsKeys = ownsKeys;
	t->ownsValues = ownsValues;
	t->duplicateOverwrite = duplicateOverwrite;
	t->keyPool = keys;
	t->valuePool = values;
	t->internalPool = table;
	return t;
}

hashTable_s* HASH_CloneTable (hashTable_s* source) {
	hashTable_s* t = HASH_NewTable(source->ownsKeys, source->ownsValues, source->duplicateOverwrite, source->hash, source->compare);

	hashItem_s* item = _iterator_first (source);
	while (item) {
		HASH_Insert (t, item->key, item->nkey, item->value, item->nvalue);
		item = _iterator_next (source, item);
	}
	return t;
}

/**
 * @brief Deletes a hash table and sets the pointer to NULL.
 * @param[in,out] t A reference to a hashTable_s pointer.
 */
void HASH_DeleteTable (hashTable_s** t) {
	_release_table (t, true);
}

/**
 * @brief Inserts a new value with given key into the hash table.
 * @param[in] t A pointer to a hashTable_s structure.
 * @param[in] key A pointer to a key.
 * @param[in] nkey The length of the key value in bytes.
 * @param[in] value A pointer to a value.
 * @param[in] nvalue The length of the value in bytes.
 * @return True if the insert succeeds, false otherwise.
 * @note If DEBUG then insert will assert when trying to insert a duplicate key value on a list created with
 * duplicateOverwrite = false.
 * @note By default the hash table creates duplicates of the inserted key and values. Should this behaviour
 * be modified, the user is responsible for making sure the pointers remain valid during the lifespan of the
 * hash table.
 */
bool HASH_Insert (hashTable_s* t, const void* key, int nkey, const void* value, int nvalue) {
	// compute hash index
	int idx=t->hash(key, nkey);
	// find the bucket, if no bucket available create a new one
	hashBucket_s* b = t->table[idx];
	if (b == NULL) {
		t->table[idx] = b = (hashBucket_s*)_hash_alloc (t->internalPool, 1, sizeof(hashBucket_s));
	}
	// find entry
	hashItem_s* i = _find_bucket_entry(b, t->compare, key, nkey, NULL);
	if (i) {
		// entry already found, so we are overwriting the current value here
		// check table settings to see if we need to assert here
		if (!t->duplicateOverwrite) {
			assert(false);
			return false;
		}
		// if the table owns the items, create a copy
		if (t->ownsValues) {
			i->value = _hash_alloc (t->valuePool, 1, nvalue);
			memcpy (i->value, value, nvalue);
		}
		else {
			i->value = const_cast<void*>(value);
		}
		i->nvalue = nvalue;
	}
	else {
		// create a new entry, add it to the top of the list
		i = (hashItem_s*) _hash_alloc (t->internalPool, 1, sizeof(hashItem_s));
		if (t->ownsKeys) {
			i->key = _hash_alloc (t->keyPool, 1, nkey);
			memcpy (i->key, key, nkey);
		}
		else {
			i->key = const_cast<void*>(key);
		}
		i->nkey = nkey;
		if (t->ownsValues) {
			i->value = _hash_alloc (t->valuePool, 1, nvalue);
			memcpy (i->value, value, nvalue);
		}
		else {
			i->value = const_cast<void*>(value);
		}
		i->nvalue = nvalue;

		i->next = b->root;
		b->root = i;
	}
	// increase the count of the bucket
	b->count++;

	return true;
}

/**
 * @brief Removes an existing value with given key from the hash table.
 * @return If the hash table does not own the data, returns the value pointer stored, otherwise returns NULL.
 */
void* HASH_Remove (hashTable_s* t, const void* key, int nkey) {
	// return value
	void* v = NULL;
	// compute index
	int idx = t->hash (key, nkey);
	if (t->table[idx]) {
		// find hash entry
		hashItem_s* prev;
		hashItem_s* i=_find_bucket_entry (t->table[idx], t->compare, key, nkey, &prev);
		if (i) {
			if (!t->ownsValues) v = i->value;
			// link prev item with next item (effectively unchaining i from the linked list)
			if (prev) {
				prev->next = i->next;
				_release_entry (&i, t->ownsKeys, t->ownsValues);
			}
			else {
				// special case: the entry found was the root entry
				t->table[idx]->root = i->next;
				_release_entry (&i, t->ownsKeys, t->ownsValues);
			}
			// drop the count in this bucket by one
			t->table[idx]->count--;
		}
	}
	return v;
}

/**
 * @brief Clears the hash table.
 */
void HASH_Clear (hashTable_s* t) {
	_release_table (&t, false);
}

/**
 * @brief Returns the value for a given key.
 */
void* HASH_Get (hashTable_s* t, const void* key, int nkey) {
	// compute bucket index
	int idx = t->hash(key, nkey);
	if (t->table[idx]) {
		// from this bucket, scan for exact match
		hashItem_s* i = _find_bucket_entry(t->table[idx], t->compare, key, nkey, NULL);
		if (i) return i->value;
	}
	return NULL;
}

/**
 * @brief Returns the number of entries in the hash table.
 */
int HASH_Count (hashTable_s* t) {
	// iteratie the table
	int cnt=0;
	for (int i=0; i<HASH_TABLE_SIZE;i++) {
		if (t->table[i]) cnt += (t->table[i]->count);
	}
	return cnt;
}

#ifdef __DEBUG__

#include <time.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * @brief Unit test function.
 */
bool HASH_test () {
	// return value
	bool result = true;

	/* test 1: create the hash table, delete the hash table */
	hashTable_s* table = HASH_NewTable(true, true, true);
	HASH_DeleteTable (&table);
	/* check table pointer is correctly set to nil */
	result = result && (table == NULL);
	if (!result) return false;
	/* check alloc total */
	result = result && (_num_allocs == 0);
	if (!result) return false;

	/* test 2: create the hash table, insert 3 values, delete the hash table */
	table = HASH_NewTable(true, true, true);
	HASH_Insert (table, "AAA", 4, "AAA", 4);
	HASH_Insert (table, "BBB", 4, "BBB", 4);
	HASH_Insert (table, "CCC", 4, "CCC", 4);
	if (HASH_Count(table) != 3) return false;
	HASH_Clear (table);
	if (HASH_Count(table) != 0) return false;
	HASH_DeleteTable (&table);
	/* check table pointer is correctly set to nil */
	result = result && (table == NULL);
	if (!result) return false;
	/* check alloc total */
	result = result && (_num_allocs == 0);
	if (!result) return false;

	/* test 3: create the hash table, insert/remove 3 values, delete the hash table */
	table = HASH_NewTable(true, true, true);
	HASH_Insert (table, "AAA", 4, "AAA", 4);
	HASH_Remove (table, "AAA", 4);
	HASH_Insert (table, "BBB", 4, "BBB", 4);
	HASH_Remove (table, "BBB", 4);
	HASH_Insert (table, "CCC", 4, "CCC", 4);
	HASH_Remove (table, "CCC", 4);
	HASH_DeleteTable (&table);
	/* check table pointer is correctly set to nil */
	result = result && (table == NULL);
	if (!result) return false;
	/* check alloc total */
	result = result && (_num_allocs == 0);
	if (!result) return false;

	/* test 4: create the hash table, insert/count/search/delete 3 values, delete the hash table */
	table = HASH_NewTable(true, true, true);
	HASH_Insert (table, "AAA", 4, "AAA", 4);
	HASH_Insert (table, "BBB", 4, "BBB", 4);
	HASH_Insert (table, "CCC", 4, "CCC", 4);
	/* check count items */
	if (HASH_Count(table) != 3) return false;
    char* aaa = (char*)HASH_Get(table, "AAA", 4);
    if (strncmp (aaa, "AAA", 4) != 0) return false;
    char* bbb = (char*)HASH_Get(table, "BBB", 4);
    if (strncmp (bbb, "BBB", 4) != 0) return false;
    char* ccc = (char*)HASH_Get(table, "CCC", 4);
    if (strncmp (ccc, "CCC", 4) != 0) return false;
	HASH_Remove (table, "AAA", 4);
	HASH_Remove (table, "BBB", 4);
	HASH_Remove (table, "CCC", 4);
	if (HASH_Count(table) != 0) return false;
	HASH_DeleteTable (&table);
	/* check table pointer is correctly set to nil */
	result = result && (table == NULL);
	if (!result) return false;
	/* check alloc total */
	result = result && (_num_allocs == 0);
	if (!result) return false;

	/* test 5: hash function test */
	const char AZ[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char buffer[26];
	/* compute the averag of the hash index of 400 random keys */
    int total = 0;
    srand(time(NULL));
    for(int i=0; i < 4000; i++) {
		/* create random key of random length between 4..23 charachters */
		int len = 4 + (rand() % 20);
		memset (buffer, 0, sizeof(buffer));
		for (int j=0; j < len; j++) {
			int v = rand() % sizeof(AZ);
			buffer[j]=AZ[v];
		}
		/* compute the hash index */
		int idx = default_hash (buffer, len);
		/* sum index */
		total += idx;
    }
    /* now compute the average of the indices */
    int avg = (total / 4000);
	/* the average idx should be somewhere halfway, allow a %10 error margin */
	int idx_low = (HASH_TABLE_SIZE/2) - (HASH_TABLE_SIZE/10);
	int idx_high = (HASH_TABLE_SIZE/2) + (HASH_TABLE_SIZE/10);
	if ( !((idx_low <= avg) && (idx_high >= avg)) ) return false;

	/* test 6: hash table without ownership test */
	table = HASH_NewTable(true, false, true);
	char item1[] = "AAA";
	char item2[] = "BBB";
	char item3[] = "CCC";
	HASH_Insert (table, item1, 4, item1, 4);
	HASH_Insert (table, item2, 4, item2, 4);
	HASH_Insert (table, item3, 4, item3, 4);
	/* check if we get the correct value pointers */
	aaa = (char*)HASH_Get(table, "AAA", 4);
	if (aaa != item1) return false;
	bbb = (char*)HASH_Get(table, "BBB", 4);
	if (bbb != item2) return false;
	ccc = (char*)HASH_Get(table, "CCC", 4);
	if (ccc != item3) return false;
	HASH_DeleteTable (&table);
	/* check table pointer is correctly set to nil */
	result = result && (table == NULL);
	if (!result) return false;
	/* check alloc total */
	result = result && (_num_allocs == 0);
	if (!result) return false;


	/* end of unit test, everything OK */
	return true;
}

#endif // __DEBUG__
