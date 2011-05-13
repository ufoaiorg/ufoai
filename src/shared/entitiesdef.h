/**
 * @file entitiesdef.h
 * @brief Handles definition of entities, parsing them from entities.ufo
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef ENITIESDEF_H
# define ENITIESDEF_H

#ifdef __cplusplus
extern "C" {
#endif

# define	ED_MAX_DEFS		64

# define	ED_ERROR		-1
# define	ED_OK			1

# define	ED_OPTIONAL		(1<<0)	/**< flag and parse mode */
# define	ED_MANDATORY	(1<<1)	/**< flag and parse mode. for entityKeyDef_t flags. entities of this type are not valid without this key. also a parse mode */
# define	ED_ABSTRACT		(1<<2)	/**< flag and parse mode, radiant use exclusively, should not be in .map file as a key */
# define	ED_TYPE_FLOAT	(1<<3)
# define	ED_TYPE_INT		(1<<4)
# define	ED_TYPE_STRING	(1<<5)
# define	ED_DEFAULT		(1<<6)
# define	ED_MODE_TYPE	(1<<7)	/**< flag parse mode indicating that a type is being parsed */
# define	ED_RANGE		(1<<8)
# define	ED_INSIST_POSITIVE (1<<9) /**< the type insists that the values are unsigned */

# define	ED_CONCRETE		(ED_OPTIONAL | ED_MANDATORY) /**< flags indicating that this is a real key for use in a map file */
# define	ED_KEY_TYPE		(ED_TYPE_FLOAT | ED_TYPE_INT | ED_TYPE_STRING)

typedef union int_float_u {
	int i;
	float f;
} int_float_tu;

typedef struct entityKeyRange_s {
	char	*str;		/**< the range string is stored here until the whole ent def is parsed */
	int		*iArr;		/**< this is used if the key is V_INT */
	float	*fArr;		/**< this is used if the key is V_FLOAT */
	int		numElements;/**< number of discrete options for the value, or two for continuous ranges */
	int		continuous;	/**< boolean. two elements in the array, Arr[0] <= val <= Arr[1] */
} entityKeyRange_t;

typedef struct entityKeyDef_s {
	char	*name;		/**< the name of the key (eg classname) */
	char	*desc;		/**< a description or value for the key (eg worldspawn) */
	char	*defaultVal;/**< a defualt value that may be provided by ufo2map -fix */
	int		flags;		/**< optional, mandatory, etc, see @sa ED_OPTIONAL, ED_MANDATORY, ED_ABSTRACT */
	int		vLen;		/**< for numeric types that may be vectors, the number of elements */
	entityKeyRange_t **ranges; /**< store allowed values for this key. may be a different range for each element */
	int		numRanges;	/**< may be 0, 1 or equal to the vLen. only for numeric types */
} entityKeyDef_t;

typedef struct entityDef_s {
	char 			*classname;
	entityKeyDef_t 	*keyDefs;
	int 			numKeyDefs;
} entityDef_t;

int ED_GetDefaultFloat(float *defaultBuf, const int n, const entityKeyDef_t *kd);
int ED_GetDefaultInt(int *defaultBuf, const int n, const entityKeyDef_t *kd);
int ED_GetDefaultString(char *defaultBuf, const size_t n, const entityKeyDef_t *kd);

int ED_GetIntVector(const entityKeyDef_t *kd, int v[], const int n);
int ED_Check(const char *classname, const char *key, const char *value);
int ED_CheckKey(const entityKeyDef_t *kd, const char *value);
const entityDef_t *ED_GetEntityDef(const char *classname);
const entityKeyDef_t *ED_GetKeyDef(const char *classname, const char *keyname, const int abstract);
const entityKeyDef_t *ED_GetKeyDefEntity(const entityDef_t *ed, const char *keyname, const int abstract);
int ED_Parse(const char *data_p);
const char *ED_GetLastError(void);
void ED_Free(void);

extern int numEntityDefs;
extern entityDef_t entityDefs[ED_MAX_DEFS + 1];

#ifdef __cplusplus
}
#endif

#endif
