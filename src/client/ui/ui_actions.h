/**
 * @file ui_actions.h
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

#ifndef CLIENT_UI_UI_ACTIONS_H
#define CLIENT_UI_UI_ACTIONS_H

#include "../../common/common.h"

/** @brief Type for uiAction_t
 * It also contain type about type (for example EA_BINARYOPERATOR)
 * @sa uiAction_t
 */
typedef enum uiActionType_s {
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
	EA_LISTENER = EA_ACTION + 9,
	EA_PUSHVARS = EA_ACTION + 10,
	EA_POPVARS = EA_ACTION + 11,

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
	EA_OPERATOR_UNARY = 0x0700,
	EA_OPERATOR_EXISTS = EA_OPERATOR_UNARY + 1,				/**< only cvar given - check for existence */
	EA_OPERATOR_PATHFROM = EA_OPERATOR_UNARY + 2,			/**< apply a relative path to a node an return a node */
	EA_OPERATOR_PATHPROPERTYFROM = EA_OPERATOR_UNARY + 3,	/**< apply a relative path to a node an return a property */

	/* terminal values (leafs) */
	EA_VALUE = 0x0A00,
	EA_VALUE_STRING = EA_VALUE + 1,						/**< reference to a string */
	EA_VALUE_STRING_WITHINJECTION = EA_VALUE + 2,		/**< reference to a injected string */
	EA_VALUE_FLOAT = EA_VALUE + 3,						/**< embedded float */
	EA_VALUE_RAW = EA_VALUE + 4,						/**< reference to a binary value */
	EA_VALUE_CVARNAME = EA_VALUE + 5,					/**< reference to a cvarname */
	EA_VALUE_CVARNAME_WITHINJECTION = EA_VALUE + 6,		/**< should be into an extra action type */
	EA_VALUE_PATHNODE = EA_VALUE + 7,					/**< reference to a path, without property */
	EA_VALUE_PATHNODE_WITHINJECTION = EA_VALUE + 8,		/**< should be into an extra action type */
	EA_VALUE_PATHPROPERTY = EA_VALUE + 9,				/**< reference to a path, and a property */
	EA_VALUE_PATHPROPERTY_WITHINJECTION = EA_VALUE + 10,/**< should be into an extra action type */
	EA_VALUE_NODEPROPERTY = EA_VALUE + 11,				/**< reference to a node, and a property (not a string) */
	EA_VALUE_VAR = EA_VALUE + 12,						/**< reference to a var */
	EA_VALUE_CVAR = EA_VALUE + 13,						/**< reference to a cvar */
	EA_VALUE_NODE = EA_VALUE + 14,						/**< reference to a node */
	EA_VALUE_PARAM = EA_VALUE + 15,						/**< reference to a param */
	EA_VALUE_PARAMCOUNT = EA_VALUE + 16,				/**< reference to the number of params */
	EA_VALUE_THIS = EA_VALUE + 17,						/**< reference to the current node */
	EA_VALUE_WINDOW = EA_VALUE + 18,					/**< reference to the window node */
	EA_VALUE_PARENT = EA_VALUE + 19						/**< reference to the parent node */
} uiActionType_t;



/**
 * @brief Defines the data of a @c uiAction_t leaf.
 * It allows different kind of data without cast
 * @sa uiAction_t
 */
typedef union {
	int integer;
	float number;
	const char* constString;
	void* data;
	const void* constData;
} uiTerminalActionData_t;

/**
 * @brief Atomic element to store UI scripts
 * The parser use this atom to translate script action into many trees of actions.
 * One function is one tree, and when we call this function, the tree is executed.
 *
 * An atom can be a command, an operator, or a value:
 * <ul> <li>Each command (EA_ACTION like EA_CALL, EA_CMD...) uses its own action structure. It can sometimes use child actions, or can be a leaf.</li>
 * <li> Operators (EA_OPERATOR_*) use binary tree structure (left and right operands), else are unary.</li>
 * <li> A value (EA_VALUE_*) is a terminal action (a leaf).</li></ul>
 * @todo FIXME Merge terminal and nonTerminal, this way is finally stupid. Left can be terminal and right can be non terminal,
 * then the structure make non sens and can create hidden bugs.
 */
typedef struct uiAction_s {
	/**
	 * @brief Define the type of the element, it can be a command, an operator, or a value
	 * @sa ea_t
	 */
	short type;

	/**
	 * @brief Some operators/commands/values can use it to store info about the content
	 */
	short subType;

	/**
	 * @brief Stores data about the action
	 */
	union {
		/**
		 * @brief Stores a none terminal action (a command or an operator)
		 * @note The action type must be a command or an operator
		 */
		struct {
			struct uiAction_s *left;
			struct uiAction_s *right;
		} nonTerminal;

		/**
		 * @brief Stores a terminal action (a value, which must be a leaf in the tree)
		 * @note The action type must be value (or sometimes a command)
		 * @todo Define the "sometimes"
		 */
		struct {
			uiTerminalActionData_t d1;
			uiTerminalActionData_t d2;
		} terminal;
	} d;

	/**
	 * @brief Next element in the action list
	 */
	struct uiAction_s *next;
} uiAction_t;

/* prototype */
struct uiNode_s;
struct cvar_s;

/** @brief Type for uiAction_t
 * It also contain type about type (for example EA_BINARYOPERATOR)
 * @sa uiAction_t
 */
typedef struct uiValue_s {
	uiActionType_t type;	/** Subset of action type to identify the value */
	union {
		int integer;
		float number;
		char* string;
		struct cvar_s *cvar;
		struct uiNode_s *node;
	} value;
} uiValue_t;

/**
 * @brief Contain the context of the calling of a function
 */
typedef struct uiCallContext_s {
	/** node owning the action */
	struct uiNode_s* source;
	/** is the function can use param from command line */
	qboolean useCmdParam;
	linkedList_t *params;
	int paramNumber;
	int varPosition;
	int varNumber;
} uiCallContext_t;

void UI_ExecuteEventActions(struct uiNode_s* source, const uiAction_t* firstAction);
void UI_ExecuteConFuncActions(struct uiNode_s* source, const uiAction_t* firstAction);
void UI_ExecuteEventActionsEx (struct uiNode_s* source, const uiAction_t* firstAction, linkedList_t *params);
qboolean UI_IsInjectedString(const char *string);
void UI_FreeStringProperty(void* pointer);
const char* UI_GenInjectedString(const char* input, qboolean addNewLine, const uiCallContext_t *context);
int UI_GetActionTokenType(const char* token, int group);
uiValue_t* UI_GetVariable (const uiCallContext_t *context, int relativeVarId);

void UI_PoolAllocAction(uiAction_t** action, int type, const void *data);
uiAction_t* UI_AllocStaticCommandAction(const char *command);
void UI_InitActions(void);
void UI_AddListener(struct uiNode_s *node, const value_t *property, const struct uiNode_s *functionNode);
void UI_RemoveListener(struct uiNode_s *node, const value_t *property, struct uiNode_s *functionNode);

const char* UI_GetParam(const uiCallContext_t *context, int paramID);
int UI_GetParamNumber(const uiCallContext_t *context);

#endif
