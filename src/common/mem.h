/**
 * @file mem.h
 * @brief Memory handling with sentinel checking and pools with tags for grouped free'ing
 */

/*
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

#ifndef _COMMON_MEM_H
#define _COMMON_MEM_H

#define MEM_MAX_POOLNAME	64
#define MEM_HASH			11

typedef struct memBlockFoot_s {
	uint32_t sentinel;				/**< For memory integrity checking */
} memBlockFoot_t;

typedef struct memBlock_s {
	struct memBlock_s *next;

	uint32_t topSentinel;			/**< For memory integrity checking */

	struct memPool_s *pool;			/**< Owner pool */
	int tagNum;						/**< For group free */
	size_t size;					/**< Size of allocation including this header */

	const char *allocFile;			/**< File the memory was allocated in */
	int allocLine;					/**< Line the memory was allocated at */

	void *memPointer;				/**< pointer to allocated memory */
	size_t memSize;					/**< Size minus the header */

	memBlockFoot_t *footer;			/**< Allocated in the space AFTER the block to check for overflow */

	uint32_t botSentinel;			/**< For memory integrity checking */
} memBlock_t;

typedef struct memPool_s {
	char name[MEM_MAX_POOLNAME];	/**< Name of pool */
	qboolean inUse;					/**< Slot in use? */

	memBlock_t *blocks[MEM_HASH];	/**< Allocated blocks */

	uint32_t blockCount;			/**< Total allocated blocks */
	uint32_t byteCount;				/**< Total allocated bytes */

	const char *createFile;			/**< File this pool was created on */
	int createLine;					/**< Line this pool was created on */
} memPool_t;

/* constants */
#define Mem_CreatePool(name)							_Mem_CreatePool((name),__FILE__,__LINE__)
#define Mem_DeletePool(pool)							_Mem_DeletePool((pool),__FILE__,__LINE__)

#define Mem_Free(ptr)									_Mem_Free((ptr),__FILE__,__LINE__)
#define Mem_FreeTag(pool,tagNum)						_Mem_FreeTag((pool),(tagNum),__FILE__,__LINE__)
#define Mem_FreePool(pool)								_Mem_FreePool((pool),__FILE__,__LINE__)
#define Mem_AllocType(type)								((type*)Mem_Alloc(sizeof(type)))
#define Mem_Alloc(size)									_Mem_Alloc((size),qtrue,com_genericPool,0,__FILE__,__LINE__)
#define Mem_AllocExt(size,zeroFill)						_Mem_Alloc((size),(zeroFill),com_genericPool,0,__FILE__,__LINE__)
#define Mem_PoolAlloc(size,pool,tagNum)					_Mem_Alloc((size),qtrue,(pool),(tagNum),__FILE__,__LINE__)
#define Mem_PoolAllocExt(size,zeroFill,pool,tagNum)		_Mem_Alloc((size),(zeroFill),(pool),(tagNum),__FILE__,__LINE__)
#define Mem_ReAlloc(ptr,size)							_Mem_ReAlloc((ptr),(size),__FILE__,__LINE__)
#define Mem_SafeReAlloc(ptr, size)						((ptr) ? Mem_ReAlloc(ptr, size) : Mem_Alloc(size))

#define Mem_Dup(in,size)								_Mem_PoolDup((in),(size),com_genericPool,0,__FILE__,__LINE__)
#define Mem_StrDup(in)									_Mem_PoolStrDup((in),com_genericPool,0,__FILE__,__LINE__)
#define Mem_PoolStrDupTo(in,out,pool,tagNum)			_Mem_PoolStrDupTo((in),(out),(pool),(tagNum),__FILE__,__LINE__)
#define Mem_PoolStrDup(in,pool,tagNum)					_Mem_PoolStrDup((in),(pool),(tagNum),__FILE__,__LINE__)
#define Mem_PoolSize(pool)								_Mem_PoolSize((pool))
#define Mem_ChangeTag(pool,tagFrom,tagTo)				_Mem_ChangeTag((pool),(tagFrom),(tagTo))

#define Mem_CheckPoolIntegrity(pool)					_Mem_CheckPoolIntegrity((pool),__FILE__,__LINE__)
#define Mem_CheckGlobalIntegrity()						_Mem_CheckGlobalIntegrity(__FILE__,__LINE__)

#define Mem_TouchPool(pool)								_Mem_TouchPool((pool),__FILE__,__LINE__)
#define Mem_TouchGlobal()								_Mem_TouchGlobal(__FILE__,__LINE__)

/* functions */
memPool_t *_Mem_CreatePool(const char *name, const char *fileName, const int fileLine) __attribute__ ((malloc));
void _Mem_DeletePool(memPool_t *pool, const char *fileName, const int fileLine);

void _Mem_Free(void *ptr, const char *fileName, const int fileLine);
void _Mem_FreeTag(memPool_t *pool, const int tagNum, const char *fileName, const int fileLine);
void _Mem_FreePool(memPool_t *pool, const char *fileName, const int fileLine);
void* _Mem_Alloc(size_t size, qboolean zeroFill, memPool_t *pool, const int tagNum, const char *fileName, const int fileLine) __attribute__ ((malloc));
void* _Mem_ReAlloc(void *ptr, size_t size, const char *fileName, const int fileLine);

char* _Mem_PoolStrDupTo(const char *in, char **out, memPool_t *pool, const int tagNum, const char *fileName, const int fileLine);
void *_Mem_PoolDup(const void *in, size_t size, memPool_t *pool, const int tagNum, const char *fileName, const int fileLine);
char* _Mem_PoolStrDup(const char *in, memPool_t *pool, const int tagNum, const char *fileName, const int fileLine) __attribute__ ((malloc));
uint32_t _Mem_PoolSize(memPool_t *pool);
uint32_t _Mem_ChangeTag(memPool_t *pool, const int tagFrom, const int tagTo);

void _Mem_CheckPoolIntegrity(memPool_t *pool, const char *fileName, const int fileLine);
void _Mem_CheckGlobalIntegrity(const char *fileName, const int fileLine);

void _Mem_TouchPool(memPool_t *pool, const char *fileName, const int fileLine);
void _Mem_TouchGlobal(const char *fileName, const int fileLine);

void* _Mem_AllocatedInPool(memPool_t *pool, const void *pointer);

void Mem_Init(void);
void Mem_Shutdown(void);

#endif
