/**
 * @file ui_parse.h
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

#ifndef CLIENT_UI_UI_PARSE_H
#define CLIENT_UI_UI_PARSE_H

#include "../../common/scripts.h"

struct uiNode_s;
struct uiAction_s;
struct value_s;

qboolean UI_ParseWindow(const char *type, const char *name, const char **text);
qboolean UI_ParseComponent(const char *type, const char **text);
qboolean UI_ParseSprite(const char *name, const char **text);
qboolean UI_ParseUIModel(const char *name, const char **text);
float UI_GetReferenceFloat(const struct uiNode_s* const node, const void *ref);
const char *UI_GetReferenceString(const struct uiNode_s* const node, const char *ref) __attribute__ ((warn_unused_result));
const value_t* UI_FindPropertyByName(const value_t* propertyList, const char* name) __attribute__ ((warn_unused_result));
char* UI_AllocStaticString(const char* string, int size) __attribute__ ((warn_unused_result));
float* UI_AllocStaticFloat(int count) __attribute__ ((warn_unused_result));
vec4_t* UI_AllocStaticColor(int count) __attribute__ ((warn_unused_result));
struct uiAction_s *UI_AllocStaticAction(void) __attribute__ ((warn_unused_result));
qboolean UI_InitRawActionValue(struct uiAction_s* action, struct uiNode_s *node, const struct value_s *property, const char *string);

/* main special type */
/** @todo we should split/flag parse type (type need only 1 lex; and other) */
#define	V_UI_MASK			0x8F00			/**< Mask for all UI bits */
#define	V_UI				0x8000			/**< bit identity an UI type */
#define V_NOT_UI			0
#define	V_UI_ACTION			(V_UI + 0)		/**< Identify an action type into the value_t structure */
#define V_UI_EXCLUDERECT	(V_UI + 1)		/**< Identify a special attribute, use special parse function */
#define V_UI_SPRITEREF		(V_UI + 3)		/**< Identify a special attribute, use special parse function */
#define V_UI_IF				(V_UI + 4)		/**< Identify a special attribute, use special parse function */
#define V_UI_DATAID			(V_UI + 5)
#define V_UI_CVAR			(V_UI + 0x0100) /**< Property is a CVAR string (mix this flag with base type, see bellow) */
#define V_UI_REF			(V_UI + 0x0200) /**< Property is a ref into a value (mix this flag with base type, see bellow) */
#define V_UI_NODEMETHOD		(V_UI + 0x0400) /**< Property is a function */

/* alias */
#define V_UI_ALIGN			V_INT

/* composite special type */
#define V_CVAR_OR_FLOAT			(V_UI_CVAR + V_FLOAT)
#define V_CVAR_OR_STRING		(V_UI_CVAR + V_STRING)
#define V_CVAR_OR_LONGSTRING	(V_UI_CVAR + V_LONGSTRING)
#define V_REF_OF_STRING			(V_UI_REF + V_STRING)

#endif
