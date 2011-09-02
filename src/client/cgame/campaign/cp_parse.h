/**
 * @file cp_parse.h
 * @brief Campaign parsing header
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef CP_PARSE_H
#define CP_PARSE_H

#define MAX_COMP	32
/** Component item count scaled to the disassembled item/UFO's codition */
#define COMP_ITEMCOUNT_SCALED -32768

/**
 * @brief The definition of a "components" entry (i.e. an assembly of several items) parsed from a ufo-file.
 * @sa CL_ParseComponents
 */
typedef struct components_s {
	char assemblyId[MAX_VAR];	/**< The name of the assembly (i.e. the UFO) */
	const objDef_t *assemblyItem;	/**< object (that is an assembly)*/

	int time;	/**< The time (in hours) until the disassembly is finished. */

	int numItemtypes;				/**< Number of item-types listed below. (max is MAX_COMP) */
	const objDef_t *items[MAX_COMP];		/**< List of parts (item-types). */
	int itemAmount[MAX_COMP];		/**< How many items of this type are in this assembly. */
	int itemAmount2[MAX_COMP];		/**< How many items of this type are in this assembly when it crashed (max-value?). */
} components_t;

void CP_ScriptSanityCheck(void);
components_t *CP_GetComponentsByItem(const objDef_t *item);
components_t *CP_GetComponentsByID(const char *id);

#endif
