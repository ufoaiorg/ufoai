/**
 * @file
 * @brief Memory handling with sentinel checking and pools with tags for grouped free'ing
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/game/q_shared.c
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

#include "common.h"
#include <SDL_thread.h>

#define MEM_MAX_POOLNAME 64
#define MEM_HASH         11

struct memBlockFoot_t {
	uint32_t sentinel;		/**< For memory integrity checking */
};

struct memBlock_t {
	memBlock_t* next;

	uint32_t topSentinel;	/**< For memory integrity checking */

	memPool_t* pool;		/**< Owner pool */
	int tagNum;				/**< For group free */

	char const* allocFile;	/**< File the memory was allocated in */
	int allocLine;			/**< Line the memory was allocated at */

	size_t memSize;			/**< Size minus the header */

	uint32_t botSentinel;	/**< For memory integrity checking */
};

struct memPool_t {
	char name[MEM_MAX_POOLNAME]; /**< Name of pool */
	bool inUse;				/**< Slot in use? */

	memBlock_t* blocks[MEM_HASH]; /**< Allocated blocks */

	uint32_t blockCount;	/**< Total allocated blocks */
	uint32_t byteCount;		/**< Total allocated bytes */

	char const* createFile; /**< File this pool was created on */
	int createLine;			/**< Line this pool was created on */
};

#define MEM_HEAD_SENTINEL_TOP	0xFEBDFAED
#define MEM_HEAD_SENTINEL_BOT	0xD0BAF0FF
#define MEM_FOOT_SENTINEL		0xF00DF00D

static SDL_mutex* z_lock;

#define MEM_MAX_POOLCOUNT	32

static memPool_t m_poolList[MEM_MAX_POOLCOUNT];
static uint32_t m_numPools;

/*==============================================================================
POOL MANAGEMENT
==============================================================================*/

static memPool_t* Mem_FindPool (const char* name)
{
	memPool_t* pool;
	uint32_t i;

	for (i = 0, pool = &m_poolList[0]; i < m_numPools; pool++, i++) {
		if (!pool->inUse)
			continue;
		if (!Q_streq(name, pool->name))
			continue;

		return pool;
	}

	return nullptr;
}

/**
 * @sa _Mem_DeletePool
 */
memPool_t* _Mem_CreatePool (const char* name, const char* fileName, const int fileLine)
{
	memPool_t* pool;
	uint32_t i;

	/* Check name */
	if (!name || !name[0])
		Sys_Error("Mem_CreatePool: nullptr name %s:#%i", fileName, fileLine);
	if (strlen(name) + 1 >= MEM_MAX_POOLNAME)
		Com_Printf("Mem_CreatePoole: name '%s' too long, truncating!\n", name);

	/* See if it already exists */
	pool = Mem_FindPool(name);
	if (pool)
		return pool;

	/* Nope, create a slot */
	for (i = 0, pool = &m_poolList[0]; i < m_numPools; pool++, i++) {
		if (!pool->inUse)
			break;
	}
	if (i == m_numPools) {
		if (m_numPools + 1 >= MEM_MAX_POOLCOUNT)
			Sys_Error("Mem_CreatePool: MEM_MAX_POOLCOUNT");
		pool = &m_poolList[m_numPools++];
	}

	/* Store values */
	for (i = 0; i < MEM_HASH; i++)
		pool->blocks[i] = nullptr;
	pool->blockCount = 0;
	pool->byteCount = 0;
	pool->createFile = fileName;
	pool->createLine = fileLine;
	pool->inUse = true;
	Q_strncpyz(pool->name, name, sizeof(pool->name));

	return pool;
}

/**
 * @brief
 * @sa _Mem_CreatePool
 * @sa _Mem_FreePool
 */
void _Mem_DeletePool (memPool_t* pool, const char* fileName, const int fileLine)
{
	if (!pool)
		return;

	/* Release all allocated memory */
	_Mem_FreePool(pool, fileName, fileLine);

	/* Simple, yes? */
	pool->inUse = false;
	pool->name[0] = '\0';
}

/*==============================================================================
POOL AND TAG MEMORY ALLOCATION
==============================================================================*/

static memBlock_t* Mem_PtrToBlock(void* const ptr)
{
	return static_cast<memBlock_t*>(ptr) - 1;
}

static void* Mem_BlockToPtr(memBlock_t* const mem)
{
	return mem + 1;
}

static memBlockFoot_t* Mem_BlockToFooter(memBlock_t* const mem)
{
	return reinterpret_cast<memBlockFoot_t*>(reinterpret_cast<byte*>(Mem_BlockToPtr(mem)) + mem->memSize);
}

static size_t Mem_BlockRawSize(memBlock_t const* const mem)
{
	return mem->memSize + sizeof(memBlock_t) + sizeof(memBlockFoot_t);
}

static void _Mem_CheckSentinels (memBlock_t* const mem, const char* fileName, const int fileLine)
{
	/* Check sentinels */
	if (mem->topSentinel != MEM_HEAD_SENTINEL_TOP) {
		Sys_Error("Mem_CheckSentinels: bad memory header top sentinel [buffer underflow]\n"
			"free: %s:#%i", fileName, fileLine);
	} else if (mem->botSentinel != MEM_HEAD_SENTINEL_BOT) {
		Sys_Error("Mem_CheckSentinels: bad memory header bottom sentinel [buffer underflow]\n"
			"free: %s:#%i", fileName, fileLine);
	} else if (Mem_BlockToFooter(mem)->sentinel != MEM_FOOT_SENTINEL) {
		Sys_Error("Mem_CheckSentinels: bad memory footer sentinel [buffer overflow]\n"
			"pool: %s\n"
			"alloc: %s:#%i\n"
			"free: %s:#%i",
			mem->pool ? mem->pool->name : "UNKNOWN", mem->allocFile, mem->allocLine, fileName, fileLine);
	}
}

/**
 * @sa _Mem_FreePool
 */
void _Mem_Free (void* ptr, const char* fileName, const int fileLine)
{
	memBlock_t* search;
	memBlock_t** prev;

	if (!ptr)
		return;

	memBlock_t* const mem = Mem_PtrToBlock(ptr);
	_Mem_CheckSentinels(mem, fileName, fileLine);

	SDL_LockMutex(z_lock);

	/* Decrement counters */
	mem->pool->blockCount--;
	mem->pool->byteCount -= Mem_BlockRawSize(mem);

	/* De-link it */
	prev = &mem->pool->blocks[(uintptr_t)mem % MEM_HASH];
	for (;;) {
		search = *prev;
		if (!search)
			break;

		if (search == mem) {
			*prev = search->next;
			break;
		}
		prev = &search->next;
	}

	SDL_UnlockMutex(z_lock);

	/* Free it */
	free(mem);
}

/**
 * @brief Free memory blocks assigned to a specified tag within a pool
 */
void _Mem_FreeTag (memPool_t* pool, const int tagNum, const char* fileName, const int fileLine)
{
	if (!pool)
		return;

	for (int j = 0; j < MEM_HASH; j++) {
		memBlock_t* mem, *next;
		for (mem = pool->blocks[j]; mem; mem = next) {
			next = mem->next;
			if (mem->tagNum == tagNum)
				_Mem_Free(Mem_BlockToPtr(mem), fileName, fileLine);
		}
	}
}


/**
 * @brief Free all items within a pool
 * @sa _Mem_CreatePool
 * @sa _Mem_DeletePool
 */
void _Mem_FreePool (memPool_t* pool, const char* fileName, const int fileLine)
{
	if (!pool)
		return;

	for (int j = 0; j < MEM_HASH; j++) {
		memBlock_t* mem, *next;
		for (mem = pool->blocks[j]; mem; mem = next) {
			next = mem->next;
			_Mem_Free(Mem_BlockToPtr(mem), fileName, fileLine);
		}
	}

	assert(pool->blockCount == 0);
	assert(pool->byteCount == 0);
}


/**
 * @brief Optionally returns 0 filled memory allocated in a pool with a tag
 */
void* _Mem_Alloc (size_t size, bool zeroFill, memPool_t* pool, const int tagNum, const char* fileName, const int fileLine)
{
	memBlock_t* mem;

	/* Check pool */
	if (!pool)
		Sys_Error("Mem_Alloc: Error - no pool given\n" "alloc: %s:#%i", fileName, fileLine);

	/* Check size */
	if (size <= 0)
		Sys_Error("Mem_Alloc: Attempted allocation of '" UFO_SIZE_T "' memory ignored\n" "alloc: %s:#%i", size, fileName, fileLine);

	if (size > 0x40000000)
		Sys_Error("Mem_Alloc: Attempted allocation of '" UFO_SIZE_T "' bytes!\n" "alloc: %s:#%i", size, fileName, fileLine);

	/* Add header and round to cacheline */
	size = (size + sizeof(memBlock_t) + sizeof(memBlockFoot_t) + 31) & ~31;
	mem = static_cast<memBlock_t* >(malloc(size));
	if (!mem)
		Sys_Error("Mem_Alloc: failed on allocation of '" UFO_SIZE_T "' bytes\n" "alloc: %s:#%i", size, fileName, fileLine);

	/* Zero fill */
	if (zeroFill)
		memset(mem, 0, size);

	/* Fill in the header */
	mem->topSentinel = MEM_HEAD_SENTINEL_TOP;
	mem->tagNum = tagNum;
	mem->memSize = size - sizeof(memBlock_t) - sizeof(memBlockFoot_t);
	mem->pool = pool;
	mem->allocFile = fileName;
	mem->allocLine = fileLine;
	mem->botSentinel = MEM_HEAD_SENTINEL_BOT;

	/* Fill in the footer */
	Mem_BlockToFooter(mem)->sentinel = MEM_FOOT_SENTINEL;

	SDL_LockMutex(z_lock);

	/* For integrity checking and stats */
	pool->blockCount++;
	pool->byteCount += size;

	/* Link it in to the appropriate pool */
	mem->next = pool->blocks[(uintptr_t)mem % MEM_HASH];
	pool->blocks[(uintptr_t)mem % MEM_HASH] = mem;

	SDL_UnlockMutex(z_lock);

	return Mem_BlockToPtr(mem);
}

void* _Mem_ReAlloc (void* ptr, size_t size, const char* fileName, const int fileLine)
{
	memPool_t* pool;
	void* newPtr;

	if (!size)
		Sys_Error("Use Mem_Free instead");

	if (!ptr)
		Sys_Error("Use Mem_Alloc instead");

	memBlock_t* const mem = Mem_PtrToBlock(ptr);
	_Mem_CheckSentinels(mem, fileName, fileLine);

	/* if size matches, do nothing */
	if (mem->memSize == size)
		return ptr;

	pool = mem->pool;

	/* allocate memory for the new size */
	newPtr = _Mem_Alloc(size, false, pool, mem->tagNum, fileName, fileLine);

	/* copy old data */
	memcpy(newPtr, ptr, std::min(mem->memSize, size));
	if (mem->memSize < size) {
		const size_t delta = size - mem->memSize;
		memset((byte*)newPtr + mem->memSize, 0, delta);
	}

	/* if there was old data, free it */
	_Mem_Free(ptr, fileName, fileLine);

	_Mem_CheckSentinels(Mem_PtrToBlock(newPtr), fileName, fileLine);

	return newPtr;
}

/*==============================================================================
MISC FUNCTIONS
==============================================================================*/

/**
 * @brief Saves a string to client hunk
 * @param[in] in String to store in the given pool
 * @param[out] out The location where you want the pool pointer to be stored
 * @param[in] pool The pool to allocate the memory in
 * @param[in] tagNum
 * @param[in] fileName The filename where this function was called from
 * @param[in] fileLine The line where this function was called from
 */
char* _Mem_PoolStrDupTo (const char* in, char** out, memPool_t* pool, const int tagNum, const char* fileName, const int fileLine)
{
	if (!out)
		return nullptr;
	*out = _Mem_PoolStrDup(in, pool, tagNum, fileName, fileLine);
	return *out;
}

void* _Mem_PoolDup (const void* in, size_t size, memPool_t* pool, const int tagNum, const char* fileName, const int fileLine)
{
	void* copy;

	assert(in != nullptr);
	assert(size > 0);

	copy = _Mem_Alloc(size, false, pool, tagNum, fileName, fileLine);
	memcpy(copy, in, size);
	return copy;
}

/**
 * @brief No need to null terminate the extra spot because Mem_Alloc returns zero-filled memory
 * @param[in] in String to store in the given pool
 * @param[in] pool The pool to allocate the memory in
 * @param[in] tagNum
 * @param[in] fileName The filename where this function was called from
 * @param[in] fileLine The line where this function was called from
 */
char* _Mem_PoolStrDup (const char* in, memPool_t* pool, const int tagNum, const char* fileName, const int fileLine)
{
	char* out;

	out = (char*)_Mem_Alloc((size_t)(strlen(in) + 1), true, pool, tagNum, fileName, fileLine);
	strcpy(out, in);

	return out;
}

/**
 * @param[in] pool The pool to get the size from
 */
uint32_t _Mem_PoolSize (memPool_t* pool)
{
	if (!pool)
		return 0;

	return pool->byteCount;
}


uint32_t _Mem_ChangeTag (memPool_t* pool, const int tagFrom, const int tagTo)
{
	if (!pool)
		return 0;

	uint32_t numChanged = 0;

	for (int j = 0; j < MEM_HASH; j++) {
		for (memBlock_t* mem = pool->blocks[j]; mem; mem = mem->next) {
			if (mem->tagNum == tagFrom) {
				mem->tagNum = tagTo;
				numChanged++;
			}
		}
	}

	return numChanged;
}


static void _Mem_CheckPoolIntegrity (memPool_t* pool, const char* fileName, const int fileLine)
{
	uint32_t blocks;
	uint32_t size;
	int j = 0;

	assert(pool);
	if (!pool)
		return;

	/* Check sentinels */
	for (j = 0, blocks = 0, size = 0; j < MEM_HASH; j++) {
		for (memBlock_t* mem = pool->blocks[j]; mem; blocks++, mem = mem->next) {
			size += Mem_BlockRawSize(mem);
			_Mem_CheckSentinels(mem, fileName, fileLine);
		}
	}

	/* Check block/byte counts */
	if (pool->blockCount != blocks)
		Sys_Error("Mem_CheckPoolIntegrity: bad block count\n" "check: %s:#%i", fileName, fileLine);
	if (pool->byteCount != size)
		Sys_Error("Mem_CheckPoolIntegrity: bad pool size\n" "check: %s:#%i", fileName, fileLine);
}


void _Mem_CheckGlobalIntegrity (const char* fileName, const int fileLine)
{
	memPool_t* pool;
	uint32_t i;

	for (i = 0, pool = &m_poolList[0]; i < m_numPools; pool++, i++) {
		if (pool->inUse)
			_Mem_CheckPoolIntegrity(pool, fileName, fileLine);
	}
}


/**
 * Searches a given pointer in all memory pool blocks
 * @param pool The pool to search the pointer in
 * @param pointer The pointer to search in the pool
 */
bool _Mem_AllocatedInPool (memPool_t* pool, const void* pointer)
{
	if (!pool)
		return false;

	/* if it's in the pool, it must be in THIS block */
	memBlock_t* mem = pool->blocks[(uintptr_t)pointer % MEM_HASH];
	/* Cycle through the blocks */
	for ( ; mem; mem = mem->next) {
		if (Mem_BlockToPtr(mem) == pointer)
			return true;
	}

	return false;
}

#ifdef COMPILE_UFO
/*==============================================================================
CONSOLE COMMANDS
==============================================================================*/

static void Mem_Check_f (void)
{
	Mem_CheckGlobalIntegrity();
}

static void Mem_Stats_f (void)
{
	uint32_t totalBlocks, totalBytes;
	memPool_t* pool;
	uint32_t poolNum, i;

	if (Cmd_Argc() > 1) {
		memPool_t* best;
		memBlock_t* mem;

		best = nullptr;
		for (i = 0, pool = &m_poolList[0]; i < m_numPools; pool++, i++) {
			if (!pool->inUse)
				continue;
			if (strstr(pool->name, Cmd_Args())) {
				if (best) {
					Com_Printf("Too many matches for '%s'...\n", Cmd_Args());
					return;
				}
				best = pool;
			}
		}
		if (!best) {
			Com_Printf("No matches for '%s'...\n", Cmd_Args());
			return;
		}

		Com_Printf("Pool stats for '%s':\n", best->name);
		Com_Printf("block line  file                 size       \n");
		Com_Printf("----- ----- -------------------- ---------- \n");

		totalBytes = 0;
		int j = 0;
		for (j = 0; j < MEM_HASH; j++) {
			for (i = 0, mem = best->blocks[j]; mem; mem = mem->next, i++) {
				if (i & 1)
					Com_Printf(S_COLOR_GREEN);

				size_t const size = Mem_BlockRawSize(mem);

				Com_Printf("%5i %5i %20s " UFO_SIZE_T "B\n", i + 1, mem->allocLine, mem->allocFile, size);

				totalBytes += size;
			}
		}

		Com_Printf("----------------------------------------\n");
		Com_Printf("Total: %i blocks, %i bytes (%6.3fMB)\n", i, totalBytes, totalBytes/1048576.0f);
		return;
	}

	Com_Printf("Memory stats:\n");
	Com_Printf("    blocks size                  name\n");
	Com_Printf("--- ------ ---------- ---------- --------\n");

	totalBlocks = 0;
	totalBytes = 0;
	poolNum = 0;
	for (i = 0, pool = &m_poolList[0]; i < m_numPools; pool++, i++) {
		if (!pool->inUse)
			continue;

		poolNum++;
		if (poolNum & 1)
			Com_Printf(S_COLOR_GREEN);

		Com_Printf("#%2i %6i %9iB (%6.3fMB) %s\n", poolNum, pool->blockCount, pool->byteCount, pool->byteCount/1048576.0f, pool->name);

		totalBlocks += pool->blockCount;
		totalBytes += pool->byteCount;
	}

	Com_Printf("----------------------------------------\n");
	Com_Printf("Total: %i pools, %i blocks, %i bytes (%6.3fMB)\n", i, totalBlocks, totalBytes, totalBytes/1048576.0f);
}
#endif

/**
 * @sa Qcommon_Init
 * @sa Mem_Shutdown
 */
void Mem_Init (void)
{
#ifdef COMPILE_UFO
	Cmd_AddCommand("mem_stats", Mem_Stats_f, "Prints out current internal memory statistics");
	Cmd_AddCommand("mem_check", Mem_Check_f, "Checks global memory integrity");
#endif

	z_lock = SDL_CreateMutex();
}

/**
 * @sa Mem_Init
 * @sa Mem_CreatePool
 * @sa Qcommon_Shutdown
 */
void Mem_Shutdown (void)
{
	memPool_t* pool;
	int i;

	/* don't use cvars, debug print or anything else inside of this loop
	 * the mem you are trying to use here might already be wiped away */
	for (i = 0, pool = &m_poolList[0]; i < m_numPools; pool++, i++) {
		if (!pool->inUse)
			continue;
		Mem_DeletePool(pool);
	}

	SDL_DestroyMutex(z_lock);
	z_lock = nullptr;
}
