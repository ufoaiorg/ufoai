/**
 * @file m_actions.c
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

#include "m_main.h"
#include "m_internal.h"
#include "m_parse.h"
#include "m_input.h"
#include "m_actions.h"
#include "node/m_node_abstractnode.h"

#include "../client.h"

typedef struct {
	char* token;
	int type;
	int group;
} menuTypedActionToken_t;

/**
 * @brief List of typed token
 * @note token ordered by alphabet, because it use a dichotomic search
 */
static const menuTypedActionToken_t actionTokens[] = {

	/* 0x2x */
	{"!", EA_OPERATOR_NOT, EA_UNARYOPERATOR},
	{"!=", EA_OPERATOR_NE, EA_BINARYOPERATOR},
	{"%", EA_OPERATOR_MOD, EA_BINARYOPERATOR},
	{"&&", EA_OPERATOR_AND, EA_BINARYOPERATOR},
	{"*", EA_OPERATOR_MUL, EA_BINARYOPERATOR},
	{"+", EA_OPERATOR_ADD, EA_BINARYOPERATOR},
	{"-", EA_OPERATOR_SUB, EA_BINARYOPERATOR},
	{"/", EA_OPERATOR_DIV, EA_BINARYOPERATOR},

	/* 0x3x */
	{"<", EA_OPERATOR_LT, EA_BINARYOPERATOR},
	{"<=", EA_OPERATOR_LE, EA_BINARYOPERATOR},
	{"==", EA_OPERATOR_EQ, EA_BINARYOPERATOR},
	{">", EA_OPERATOR_GT, EA_BINARYOPERATOR},
	{">=", EA_OPERATOR_GE, EA_BINARYOPERATOR},

	/* 0x5x */
	{"^", EA_OPERATOR_XOR, EA_BINARYOPERATOR},

	/* 'a'..'z' */
	{"and", EA_OPERATOR_AND, EA_BINARYOPERATOR},
	{"call", EA_CALL, EA_ACTION},
	{"cmd", EA_CMD, EA_ACTION},
	{"delete", EA_DELETE, EA_ACTION},
	{"elif", EA_ELIF, EA_ACTION},
	{"else", EA_ELSE, EA_ACTION},
	{"eq", EA_OPERATOR_STR_EQ, EA_BINARYOPERATOR},
	{"exists", EA_OPERATOR_EXISTS, EA_UNARYOPERATOR},
	{"if", EA_IF, EA_ACTION},
	{"ne", EA_OPERATOR_STR_NE, EA_BINARYOPERATOR},
	{"not", EA_OPERATOR_NOT, EA_UNARYOPERATOR},
	{"or", EA_OPERATOR_OR, EA_BINARYOPERATOR},
	{"while", EA_WHILE, EA_ACTION},

	/* 0x7x */
	{"||", EA_OPERATOR_OR, EA_BINARYOPERATOR},
};

/**
 * @brief Check if the action token list is sorted by alphabet,
 * else dichotomic search can't work
 */
static void MN_CheckActionTokenTypeSanity (void)
{
	int i;
	for (i = 0; i < lengthof(actionTokens) - 1; i++) {
		const char *a = actionTokens[i].token;
		const char *b = actionTokens[i + 1].token;
		if (strcmp(a, b) >= 0) {
			Sys_Error("MN_CheckActionTokenTypeSanity: '%s' is before '%s'. actionList must be sorted by alphabet", a, b);
		}
	}
}

/**
 * @brief return an action type from a token, and a group
 * @param[in] token Requested token
 * @param[in] group Requested group, EA_ACTION, EA_BINARYOPERATOR, or EA_UNARYOPERATOR
 * @sa actionTokens
 * @return a action type from the requested group, else EA_NULL
 */
int MN_GetActionTokenType (const char* token, int group)
{
	unsigned char min = 0;
	unsigned char max = lengthof(actionTokens);

	while (min != max) {
		const int mid = (min + max) >> 1;
		const char diff = strcmp(actionTokens[mid].token, token);
		assert(mid < max);
		assert(mid >= min);

		if (diff == 0) {
			if (actionTokens[mid].group == group)
				return actionTokens[mid].type;
			else
				return EA_NULL;
		}

		if (diff > 0)
			max = mid;
		else
			min = mid + 1;
	}
	return EA_NULL;
}

/**
 * @brief read a property name from an input buffer to an output
 * @return last position in the input buffer if we find the property, NULL otherwise
 */
static inline const char* MN_GenCommandReadProperty (const char* input, char* output, int outputSize)
{
	assert(input[0] == '<');
	outputSize--;
	input++;

	while (outputSize && *input != '\0' && *input != ' ' && *input != '>') {
		*output++ = *input++;
		outputSize--;
	}

	if (input[0] != '>')
		return NULL;

	output[0] = '\0';
	return ++input;
}

/**
 * @brief Replace injection identifiers (e.g. &lt;eventParam&gt;) by a value
 * @note The injection identifier can be every node value - e.g. &lt;image&gt; or &lt;width&gt;.
 * It's also possible to do something like
 * @code
 * cmd "set someCvar &lt;min&gt;/&lt;max&gt;"
 * @endcode
 */
const char* MN_GenInjectedString (const menuNode_t* source, qboolean useCmdParam, const char* input, qboolean addNewLine)
{
	static char cmd[256];
	int length = sizeof(cmd) - (addNewLine ? 2 : 1);
	static char propertyName[MAX_VAR];
	const char *cin = input;
	char *cout = cmd;

	while (length && cin[0] != '\0') {
		if (cin[0] == '<') {
			/* read propertyName between '<' and '>' */
			const char *next = MN_GenCommandReadProperty(cin, propertyName, sizeof(propertyName));
			if (next) {
				/* cvar injection */
				if (!strncmp(propertyName, "cvar:", 5)) {
					const cvar_t *cvar = Cvar_Get(propertyName + 5, "", 0, NULL);
					const int l = snprintf(cout, length, "%s", cvar->string);
					cout += l;
					cin = next;
					length -= l;
					continue;

				} else if (!strncmp(propertyName, "node:", 5)) {
					const char *path = propertyName + 5;
					menuNode_t *node;
					const value_t *property;
					const char* string;
					int l;
					MN_ReadNodePath(path, source, &node, &property);
					if (!node) {
						Com_Printf("MN_GenInjectedString: Node '%s' wasn't found; '' returned\n", path);
						string = "";
					} else if (!property) {
						Com_Printf("MN_GenInjectedString: Property '%s' wasn't found; '' returned\n", path);
						string = "";
					} else {
						string = MN_GetStringFromNodeProperty(node, property);
						if (string == NULL) {
							Com_Printf("MN_GenInjectedString: String getter for '%s' property do not exists; '' injected\n", path);
							string = "";
						}
					}

					l = snprintf(cout, length, "%s", string);
					cout += l;
					cin = next;
					length -= l;
					continue;

				/* source path injection */
				} else if (!strncmp(propertyName, "path:", 5)) {
					if (source) {
						const char *command = propertyName + 5;
						const menuNode_t *node = NULL;
						if (!strcmp(command, "root"))
							node = source->root;
						else if (!strcmp(command, "this"))
							node = source;
						else if (!strcmp(command, "parent"))
							node = source->parent;
						else
							Com_Printf("MN_GenCommand: Command '%s' for path injection unknown\n", command);

						if (node) {
							const int l = snprintf(cout, length, "%s", MN_GetPath(node));
							cout += l;
							cin = next;
							length -= l;
							continue;
						}
					}

				/* no prefix */
				} else {
					/* source property injection */
					if (source) {
						/* find property definition */
						const value_t *property = MN_GetPropertyFromBehaviour(source->behaviour, propertyName);
						if (property) {
							const char* value;
							int l;
							/* inject the property value */
							value = MN_GetStringFromNodeProperty(source, property);
							if (value == NULL)
								value = "";
							l = snprintf(cout, length, "%s", value);
							cout += l;
							cin = next;
							length -= l;
							continue;
						}
					}

					/* param injection */
					if (useCmdParam) {
						int arg;
						const int checked = sscanf(propertyName, "%d", &arg);
						if (checked == 1 && Cmd_Argc() >= arg) {
							const int l = snprintf(cout, length, "%s", Cmd_Argv(arg));
							cout += l;
							cin = next;
							length -= l;
							continue;
						}
					}
				}
			}
		}
		*cout++ = *cin++;
		length--;
	}

	/* is buffer too small? */
	assert(cin[0] == '\0');

	if (addNewLine)
		*cout++ = '\n';

	*cout++ = '\0';

	return cmd;
}

static inline void MN_ExecuteSetAction (const menuNode_t* source, qboolean useCmdParam, const menuAction_t* action)
{
	const char* path;
	menuNode_t *node;
	const value_t *property;
	const menuAction_t *left;
	menuAction_t *right;

	left = action->d.nonTerminal.left;
	if (left == NULL) {
		Com_Printf("MN_ExecuteSetAction: Action without left operand skipped.\n");
		return;
	}

	right = action->d.nonTerminal.right;
	if (right == NULL) {
		Com_Printf("MN_ExecuteSetAction: Action without right operand skipped.\n");
		return;
	}

	if (left->type == EA_VALUE_CVARNAME || left->type == EA_VALUE_CVARNAME_WITHINJECTION) {
		const char *cvarName;
		const char* textValue;

		if (left->type == EA_VALUE_CVARNAME)
			cvarName = left->d.terminal.d1.string;
		else
			cvarName = MN_GenInjectedString(source, useCmdParam, left->d.terminal.d1.string, qfalse);

		textValue = MN_GetStringFromExpression(right, source);

		if (textValue[0] == '_')
			textValue = _(textValue + 1);

		Cvar_ForceSet(cvarName, textValue);
		return;
	}

	/* search the node */
	if (left->type == EA_VALUE_PATHPROPERTY)
		path = left->d.terminal.d1.string;
	else if (left->type == EA_VALUE_PATHPROPERTY_WITHINJECTION)
		path = MN_GenInjectedString(source, useCmdParam, left->d.terminal.d1.string, qfalse);
	else
		Com_Error(ERR_FATAL, "MN_ExecuteSetAction: Property setter with wrong type '%d'", left->type);

	MN_ReadNodePath(path, source, &node, &property);
	if (!node) {
		Com_Printf("MN_ExecuteSetAction: node \"%s\" doesn't exist (source: %s)\n", path, MN_GetPath(source));
		return;
	}
	if (!property) {
		Com_Printf("MN_ExecuteSetAction: property \"%s\" doesn't exist (source: %s)\n", path, MN_GetPath(source));
		return;
	}

	/* decode RAW value */
	if (right->type == EA_VALUE_RAW) {
		void *rawValue = right->d.terminal.d1.data;
		const value_t *rawType = right->d.terminal.d2.constData;
		MN_NodeSetPropertyFromRAW(node, property, rawValue, rawType);
	}
	/* else it is an expression */
	else {
		/** @todo we should improve if when the prop is a boolean/int/float */
		const char* value = MN_GetStringFromExpression(right, source);
		MN_NodeSetProperty(node, property, value);
	}
}

static void MN_ExecuteInjectedActions(const menuNode_t* source, qboolean useCmdParam, const menuAction_t* firstAction);

/**
 * @brief Execute an action from a source
 * @param[in] source Context node
 * @param[in] useCmdParam If true, inject every where its possible command line param
 * @param[in] action Action to execute
 */
static void MN_ExecuteInjectedAction (const menuNode_t* source, qboolean useCmdParam, const menuAction_t* action)
{
	switch (action->type) {
	case EA_NULL:
		/* do nothing */
		break;

	case EA_CMD:
		/* execute a command */
		if (action->d.terminal.d1.string)
			Cbuf_AddText(MN_GenInjectedString(source, useCmdParam, action->d.terminal.d1.string, qtrue));
		break;

	case EA_CALL:
	case EA_LISTENER:
		{
			menuNode_t* callNode = NULL;
			const value_t* callProperty = NULL;
			const char* path = action->d.nonTerminal.left->d.terminal.d1.constString;
			MN_ReadNodePath(path, source, &callNode, &callProperty);

			if (callProperty == NULL || callProperty->type == V_UI_ACTION) {
				menuAction_t *actionsRef = NULL;
				if (callProperty == NULL)
					actionsRef = callNode->onClick;
				else
					actionsRef = *(menuAction_t **) ((byte *) callNode + callProperty->ofs);
				MN_ExecuteInjectedActions(callNode, useCmdParam, actionsRef);
			} else if (callProperty->type == V_UI_NODEMETHOD) {
				menuNodeMethod_t func = (menuNodeMethod_t) callProperty->ofs;
				func(callNode);
			} else {
				Com_Printf("MN_ExecuteInjectedAction: Call operand %d unsupported. (%s)\n", callProperty->type, MN_GetPath(callNode));
				return;
			}
		}
		break;

	case EA_ASSIGN:
		MN_ExecuteSetAction(source, useCmdParam, action);
		break;

	case EA_DELETE:
		{
			const char* cvarname = action->d.nonTerminal.left->d.terminal.d1.constString;
			Cvar_Delete(cvarname);
			break;
		}

	case EA_WHILE:
		{
			int loop = 0;
			while (MN_GetBooleanFromExpression(action->d.nonTerminal.left, source)) {
				MN_ExecuteInjectedActions(source, useCmdParam, action->d.nonTerminal.right);
				if (loop > 1000) {
					Com_Printf("MN_ExecuteInjectedAction: Infinite loop. Force breaking 'while'\n");
					break;
				}
				loop++;
			}
			break;
		}

	case EA_IF:
		if (MN_GetBooleanFromExpression(action->d.nonTerminal.left, source)) {
			MN_ExecuteInjectedActions(source, useCmdParam, action->d.nonTerminal.right);
			return;
		}
		action = action->next;
		while (action && action->type == EA_ELIF) {
			if (MN_GetBooleanFromExpression(action->d.nonTerminal.left, source)) {
				MN_ExecuteInjectedActions(source, useCmdParam, action->d.nonTerminal.right);
				return;
			}
			action = action->next;
		}
		if (action && action->type == EA_ELSE) {
			MN_ExecuteInjectedActions(source, useCmdParam, action->d.nonTerminal.right);
		}
		break;

	case EA_ELSE:
	case EA_ELIF:
		/* previous EA_IF execute this action */
		break;

	default:
		Com_Error(ERR_FATAL, "MN_ExecuteInjectedAction: Unknown action type %i", action->type);
	}
}

static void MN_ExecuteInjectedActions (const menuNode_t* source, qboolean useCmdParam, const menuAction_t* firstAction)
{
	static int callnumber = 0;
	const menuAction_t *action;
	if (callnumber++ > 20) {
		Com_Printf("MN_ExecuteInjectedActions: Possible recursion\n");
		return;
	}
	for (action = firstAction; action; action = action->next) {
		MN_ExecuteInjectedAction(source, useCmdParam, action);
	}
	callnumber--;
}

/**
 * @brief allow to inject command param into cmd of confunc command
 */
void MN_ExecuteConFuncActions (const menuNode_t* source, const menuAction_t* firstAction)
{
	MN_ExecuteInjectedActions(source, qtrue, firstAction);
}

void MN_ExecuteEventActions (const menuNode_t* source, const menuAction_t* firstAction)
{
	MN_ExecuteInjectedActions(source, qfalse, firstAction);
}

/**
 * @brief Test if a string use an injection syntax
 * @return True if we find the following syntax in the string "<" {thing without space} ">"
 */
qboolean MN_IsInjectedString (const char *string)
{
	const char *c = string;
	assert(string);
	while (*c != '\0') {
		if (*c == '<') {
			const char *d = c + 1;
			if (*d != '>') {
				while (*d) {
					if (*d == '>')
						return qtrue;
					if (*d == ' ' || *d == '\t' || *d == '\n' || *d == '\r')
						break;
					d++;
				}
			}
		}
		c++;
	}
	return qfalse;
}

/**
 * @brief Free a string property if it is allocated into mn_dynStringPool
 * @sa mn_dynStringPool
 */
void MN_FreeStringProperty (void* pointer)
{
	/* skip const string */
	if ((uintptr_t)mn.adata <= (uintptr_t)pointer && (uintptr_t)pointer < (uintptr_t)mn.adata + (uintptr_t)mn.adataize)
		return;

	/* skip pointer out of mn_dynStringPool */
	if (!_Mem_AllocatedInPool(mn_dynStringPool, pointer))
		return;

	Mem_Free(pointer);
}

/**
 * @brief Allocate and initialize a command action
 * @param[in] command A command for the action
 * @return An initialised action
 */
menuAction_t* MN_AllocStaticCommandAction (char *command)
{
	menuAction_t* action = MN_AllocStaticAction();
	action->type = EA_CMD;
	action->d.terminal.d1.string = command;
	return action;
}

/**
 * @brief Set a new action to a @c menuAction_t pointer
 * @param[in,out] action Allocated action
 * @param[in] type Only @c EA_CMD is supported
 * @param[in] data The data for this action - in case of @c EA_CMD this is the commandline
 * @note You first have to free existing node actions - only free those that are
 * not static in @c mn.menuActions array
 * @todo we should create a function to free the memory. We can use a tag in the Mem_PoolAlloc
 * calls and use use Mem_FreeTag.
 */
void MN_PoolAllocAction (menuAction_t** action, int type, const void *data)
{
	if (*action)
		Com_Error(ERR_FATAL, "There is already an action assigned");
	*action = (menuAction_t *)Mem_PoolAlloc(sizeof(**action), mn_sysPool, 0);
	(*action)->type = type;
	switch (type) {
	case EA_CMD:
		(*action)->d.terminal.d1.string = Mem_PoolStrDup((const char *)data, mn_sysPool, 0);
		break;
	default:
		Com_Error(ERR_FATAL, "Action type %i is not yet implemented", type);
	}
}

/**
 * @brief add a call of a function into a nodfe event
 */
static void MN_AddListener_f (void)
{
	menuNode_t *node;
	menuNode_t *function;
	const value_t *property;
	menuAction_t *action;
	menuAction_t *value;
	menuAction_t *lastAction;

	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <pathnode@event> <pathnode>\n", Cmd_Argv(0));
		return;
	}

	MN_ReadNodePath(Cmd_Argv(1), NULL, &node, &property);
	if (node == NULL) {
		Com_Printf("MN_AddListener_f: '%s' node not found.\n", Cmd_Argv(1));
		return;
	}
	if (property == NULL || property->type != V_UI_ACTION) {
		Com_Printf("MN_AddListener_f: '%s' property not found, or is not an event.\n", Cmd_Argv(1));
		return;
	}

	function = MN_GetNodeByPath(Cmd_Argv(2));
	if (function == NULL) {
		Com_Printf("MN_AddListener_f: '%s' node not found.\n", Cmd_Argv(2));
		return;
	}

	/* create the call action */
	/** @todo merge that into a function */
	action = (menuAction_t*) Mem_PoolAlloc(sizeof(*action), mn_sysPool, 0);
	value = (menuAction_t*) Mem_PoolAlloc(sizeof(*action), mn_sysPool, 0);
	value->d.terminal.d1.constString = Mem_PoolStrDup(Cmd_Argv(2), mn_sysPool, 0);
	value->next = NULL;
	action->type = EA_LISTENER;
	action->d.nonTerminal.left = value;
	/** @todo It is a hack, we should remove that */
	action->d.terminal.d2.data = &function->onClick;
	action->next = NULL;

	/* insert the action */
	lastAction = *(menuAction_t**)((char*)node + property->ofs);
	if (lastAction) {
		while (lastAction->next)
			lastAction = lastAction->next;
		lastAction->next = action;
	} else
		*(menuAction_t**)((char*)node + property->ofs) = action;
}

/**
 * @brief add a call of a function from a node event
 */
static void MN_RemoveListener_f (void)
{
	menuNode_t *node;
	menuNode_t *function;
	const value_t *property;
	void *data;
	menuAction_t *lastAction;

	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <pathnode@event> <pathnode>\n", Cmd_Argv(0));
		return;
	}

	MN_ReadNodePath(Cmd_Argv(1), NULL, &node, &property);
	if (node == NULL) {
		Com_Printf("MN_RemoveListener_f: '%s' node not found.\n", Cmd_Argv(1));
		return;
	}
	if (property == NULL || property->type != V_UI_ACTION) {
		Com_Printf("MN_RemoveListener_f: '%s' property not found, or is not an event.\n", Cmd_Argv(1));
		return;
	}

	function = MN_GetNodeByPath(Cmd_Argv(2));
	if (function == NULL) {
		Com_Printf("MN_RemoveListener_f: '%s' node not found.\n", Cmd_Argv(2));
		return;
	}

	/* data we must remove */
	data = (void*) &function->onClick;

	/* remove the action */
	lastAction = *(menuAction_t**)((char*)node + property->ofs);
	if (lastAction) {
		menuAction_t *tmp = NULL;
		if (lastAction->type == EA_LISTENER && lastAction->d.terminal.d2.data == data) {
			tmp = lastAction;
			*(menuAction_t**)((char*)node + property->ofs) = lastAction->next;
		} else {
			while (lastAction->next) {
				if (lastAction->next->type == EA_LISTENER && lastAction->next->d.terminal.d2.data == data)
					break;
				lastAction = lastAction->next;
			}
			if (lastAction->next) {
				tmp = lastAction->next;
				lastAction->next = lastAction->next->next;
			}
		}
		if (tmp) {
			/** @todo merge that into a function */
			menuAction_t* value = tmp->d.nonTerminal.left;
			Mem_Free(value->d.terminal.d1.data);
			Mem_Free(value);
			Mem_Free(tmp);
		}
		else
			Com_Printf("MN_RemoveListener_f: '%s' into '%s' not found.\n", Cmd_Argv(2), Cmd_Argv(1));
	}
}

void MN_InitActions (void)
{
	MN_CheckActionTokenTypeSanity();
	Cmd_AddCommand("mn_addlistener", MN_AddListener_f, "Add a function into a node event");
	Cmd_AddCommand("mn_removelistener", MN_RemoveListener_f, "Remove a function from a node event");
}
