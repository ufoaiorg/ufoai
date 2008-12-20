/**
 * @file m_parse.h
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#ifndef CLIENT_MENU_M_PARSE_H
#define CLIENT_MENU_M_PARSE_H

#include "node/m_node_window.h"

void MN_ParseMenu(const char *name, const char **text);
void MN_ParseIcon(const char *name, const char **text);
void MN_ParseMenuModel(const char *name, const char **text);
float MN_GetReferenceFloat(const menu_t* const menu, void *ref);
const char *MN_GetReferenceString(const menu_t* const menu, const char *ref);
struct value_s;
const value_t* MN_FindPropertyByName(const struct value_s* propertyList, const char* name);
char* MN_AllocString(const char* string, int size);


qboolean MN_ScriptSanityCheck(void);

#define V_MENU_COPY 0x100 /**< flag to request a memory copy into the menu structure */

#define	V_SPECIAL				0x8000
#define	V_SPECIAL_ACTION		(V_SPECIAL + 0)	/**< Identify an action type into the value_t structure */
#define V_SPECIAL_EXCLUDERECT	(V_SPECIAL + 1)	/**< Identify a special attribute, use special parse function */
#define V_SPECIAL_OPTIONNODE	(V_SPECIAL + 2) /**< Identify a special attribute, use special parse function */
#define V_SPECIAL_ICONREF		(V_SPECIAL + 3) /**< Identify a special attribute, use special parse function */

#endif
