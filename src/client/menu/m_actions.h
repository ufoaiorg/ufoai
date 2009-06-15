/**
 * @file m_actions.h
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

#ifndef CLIENT_MENU_M_ACTIONS_H
#define CLIENT_MENU_M_ACTIONS_H

#include "../../common/common.h"

/** @brief EventActions */
typedef enum ea_s {
	EA_NULL = 0,
	EA_CMD,
	EA_CALL,
	EA_ASSIGN,
	EA_IF,
	EA_ELSE,
	EA_ELIF,

	/* invalid operator */
	EA_OPERATOR_INVALID,

	/* unary operators */
	EA_OPERATOR_EXISTS, /**< only cvar given - check for existence */

	/* float operators */
	EA_OPERATOR_EQ, /**< == */
	EA_OPERATOR_LE, /**< <= */
	EA_OPERATOR_GE, /**< >= */
	EA_OPERATOR_GT, /**< > */
	EA_OPERATOR_LT, /**< < */
	EA_OPERATOR_NE, /**< != */

	/* string operators */
	EA_OPERATOR_STR_EQ,	/**< eq */
	EA_OPERATOR_STR_NE,	/**< ne */

	/* terminal values */
	EA_VALUE_STRING,						/**< reference to a string */
	EA_VALUE_FLOAT,							/**< embedded float */
	EA_VALUE_RAW,							/**< reference to a binary value */
	EA_VALUE_CVARNAME,						/**< reference to a cvarname */
	EA_VALUE_CVARNAME_WITHINJECTION,		/**< should be into an extra action type */
	EA_VALUE_PATHPROPERTY,					/**< reference to a path, and a property (when it is possible) */
	EA_VALUE_PATHPROPERTY_WITHINJECTION,	/**< should be into an extra action type */
	EA_VALUE_NODEPROPERTY					/**< reference to a node, and a property (not a string) */
} ea_t;

typedef union {
	float number;
	char* string;
	const char* constString;
	void* data;
	const void* constData;
} menuTerminalActionData_t;

typedef struct menuAction_s {
	short type;
	short subType;

	union {
		struct {
			struct menuAction_s *left;
			struct menuAction_s *right;
		} nonTerminal;
		struct {
			menuTerminalActionData_t d1;
			menuTerminalActionData_t d2;
		} terminal;
	} d;

	struct menuAction_s *next;
} menuAction_t;

void MN_ExecuteConfunc(const char *fmt, ...) __attribute__((format(__printf__, 1, 2)));

/* prototype */
struct menuNode_s;

void MN_ExecuteEventActions(const struct menuNode_s* source, const menuAction_t* firstAction);
void MN_ExecuteConFuncActions(const struct menuNode_s* source, const menuAction_t* firstAction);
qboolean MN_IsInjectedString(const char *string);
void MN_FreeStringProperty(void* pointer);

void MN_PoolAllocAction(menuAction_t** action, int type, const void *data);
menuAction_t* MN_AllocCommandAction(char *command);
void MN_InitActions(void);

#endif
