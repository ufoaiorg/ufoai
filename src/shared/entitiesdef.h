/**
 * @file entitiesdef.h
 * @brief Handles definition of entities, parsing them from entities.ufo
 */

/*
All original materal Copyright (C) 2002-2009 UFO: Alien Invasion team.

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

# define	ED_DEFAULT		-1		/**< parse mode */
# define	ED_OPTIONAL		(1<<0)	/**< flag and parse mode */
# define	ED_MANDATORY	(1<<1)	/**< flag and parse mode. for entityKeyDef_t flags. entities of this type are not valid without this key. also a parse mode */
# define	ED_ABSTRACT		(1<<2)	/**< flag and parse mode, radiant use exclusively, should not be in .map file as a key */

# define	ED_CONCRETE		(ED_OPTIONAL | ED_MANDATORY) /**< flags indicating that this is a real key for use in a map file */

typedef struct entityKeyDef_s {
	char *name;
	char *desc;
	char *defaultVal;
	int flags;
} entityKeyDef_t;

typedef struct entityDef_s {
	entityKeyDef_t *keyDefs;
	int numKeyDefs;
} entityDef_t;

const entityDef_t *ED_GetEntityDef (const char *classname);
int ED_Parse(const char **data_p);
const char *ED_GetLastError(void);
void ED_Free(void);

int numEntityDefs;
entityDef_t *entityDefs;

#endif
