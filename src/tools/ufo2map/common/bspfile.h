/**
 * @file bspfile.h
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

#ifndef _BSPFILE
#define _BSPFILE

#include "../../../common/qfiles.h"

dMapTile_t *LoadBSPFile(const char *filename);
long WriteBSPFile(const char *filename);
void PrintBSPFileSizes(void);

/**
 * @sa entity_t
 */
typedef struct epair_s {
	struct epair_s	*next;	/**< the next entry in the key, value list */
	char	*key;			/**< the name of the parameter */
	char	*value;			/**< the value of the parameter */
} epair_t;

/**
 * @sa epair_t
 */
typedef struct {
	vec3_t		origin;			/**< the origin vector of the entity */
	int			firstbrush;		/**< the index of the first brush in mapbrushes in case of a bmodel */
	int			numbrushes;		/**< the number of brushes in case of a bmodel */
	epair_t		*epairs;		/**< the entity parameters (key, value) */
	qboolean	skip;			/**< skip this entity in case the check functions have found an error and it should
								 * not get written back into the fixed map file */
} entity_t;

extern int num_entities;
extern entity_t entities[MAX_MAP_ENTITIES];

entity_t *FindTargetEntity(const char *target);
void ParseEntities(void);
const char *UnparseEntities(void);

void SetKeyValue(entity_t *ent, const char *key, const char *value);
/* will return "" if not present */
const char *ValueForKey(const entity_t *ent, const char *key);

vec_t FloatForKey(const entity_t *ent, const char *key);
void GetVectorFromString(const char *value, vec3_t vec);
void GetVectorForKey(const entity_t *ent, const char *key, vec3_t vec);
epair_t *ParseEpair(void);
byte *CompressRouting(byte *dataStart, byte *destStart, int l);

#endif /* _BSP_FILE */
