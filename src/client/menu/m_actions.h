/**
 * @file m_actions.h
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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

	EA_BINARYOPERATOR,
	EA_UNARYOPERATOR,

	/* masks */
	EA_HIGHT_MASK = 0xFF00,

	/* actions */
	EA_ACTION = 0x0100,
	EA_CMD = EA_ACTION + 1,
	EA_CALL = EA_ACTION + 2,
	EA_ASSIGN = EA_ACTION + 3,
	EA_IF = EA_ACTION + 4,
	EA_ELSE = EA_ACTION + 5,
	EA_ELIF = EA_ACTION + 6,
	EA_WHILE = EA_ACTION + 7,
	EA_DELETE = EA_ACTION + 8,

	/* boolean to boolean operators */
	EA_OPERATOR_BOOLEAN2BOOLEAN = 0x0300,
	EA_OPERATOR_AND = EA_OPERATOR_BOOLEAN2BOOLEAN + 1,
	EA_OPERATOR_OR = EA_OPERATOR_BOOLEAN2BOOLEAN + 2,
	EA_OPERATOR_XOR = EA_OPERATOR_BOOLEAN2BOOLEAN + 3,
	EA_OPERATOR_NOT = EA_OPERATOR_BOOLEAN2BOOLEAN + 4,

	/* float to boolean operators */
	EA_OPERATOR_FLOAT2BOOLEAN = 0x0400,
	EA_OPERATOR_EQ = EA_OPERATOR_FLOAT2BOOLEAN + 1, /**< == */
	EA_OPERATOR_LE = EA_OPERATOR_FLOAT2BOOLEAN + 2, /**< <= */
	EA_OPERATOR_GE = EA_OPERATOR_FLOAT2BOOLEAN + 3, /**< >= */
	EA_OPERATOR_GT = EA_OPERATOR_FLOAT2BOOLEAN + 4, /**< > */
	EA_OPERATOR_LT = EA_OPERATOR_FLOAT2BOOLEAN + 5, /**< < */
	EA_OPERATOR_NE = EA_OPERATOR_FLOAT2BOOLEAN + 6, /**< != */

	/* float to float operators */
	EA_OPERATOR_FLOAT2FLOAT = 0x0500,
	EA_OPERATOR_ADD = EA_OPERATOR_FLOAT2FLOAT + 1,
	EA_OPERATOR_SUB = EA_OPERATOR_FLOAT2FLOAT + 2,
	EA_OPERATOR_MUL = EA_OPERATOR_FLOAT2FLOAT + 3,
	EA_OPERATOR_DIV = EA_OPERATOR_FLOAT2FLOAT + 4,
	EA_OPERATOR_MOD = EA_OPERATOR_FLOAT2FLOAT + 5,

	/* string operators */
	EA_OPERATOR_STRING2BOOLEAN = 0x0600,
	EA_OPERATOR_STR_EQ = EA_OPERATOR_STRING2BOOLEAN + 1,	/**< eq */
	EA_OPERATOR_STR_NE = EA_OPERATOR_STRING2BOOLEAN + 2,	/**< ne */

	/* unary operators */
	EA_OPERATOR_EXISTS = 0x0700, /**< only cvar given - check for existence */

	/* terminal values */
	EA_VALUE = 0x0A00,
	EA_VALUE_STRING = EA_VALUE + 1,						/**< reference to a string */
	EA_VALUE_STRING_WITHINJECTION = EA_VALUE + 2,		/**< reference to a injected string */
	EA_VALUE_FLOAT = EA_VALUE + 3,						/**< embedded float */
	EA_VALUE_RAW = EA_VALUE + 4,						/**< reference to a binary value */
	EA_VALUE_CVARNAME = EA_VALUE + 5,					/**< reference to a cvarname */
	EA_VALUE_CVARNAME_WITHINJECTION = EA_VALUE + 6,		/**< should be into an extra action type */
	EA_VALUE_PATHPROPERTY = EA_VALUE + 7,				/**< reference to a path, and a property (when it is possible) */
	EA_VALUE_PATHPROPERTY_WITHINJECTION = EA_VALUE + 8,	/**< should be into an extra action type */
	EA_VALUE_NODEPROPERTY = EA_VALUE + 9				/**< reference to a node, and a property (not a string) */
} ea_t;

#define EA_CALL_NORMAL			0x0
#define EA_CALL_NODEMETHOD		0x1

typedef union {
	int integer;
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

/* prototype */
struct menuNode_s;

void MN_ExecuteEventActions(const struct menuNode_s* source, const menuAction_t* firstAction);
void MN_ExecuteConFuncActions(const struct menuNode_s* source, const menuAction_t* firstAction);
qboolean MN_IsInjectedString(const char *string);
void MN_FreeStringProperty(void* pointer);
const char* MN_GenInjectedString(const struct menuNode_s* source, qboolean useCmdParam, const char* input, qboolean addNewLine);
int MN_GetActionTokenType(const char* token, int group);

void MN_PoolAllocAction(menuAction_t** action, int type, const void *data);
menuAction_t* MN_AllocStaticCommandAction(char *command);
void MN_InitActions(void);

#endif
