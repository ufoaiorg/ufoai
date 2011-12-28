/**
 * @file ui_actions.c
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

#include "ui_main.h"
#include "ui_internal.h"
#include "ui_parse.h"
#include "ui_input.h"
#include "ui_actions.h"
#include "node/ui_node_abstractnode.h"

#include "../cl_shared.h"

typedef struct {
	const char* token;
	int type;
	int group;
} ui_typedActionToken_t;

/**
 * @brief List of typed token
 * @note token ordered by alphabet, because it use a dichotomic search
 */
static const ui_typedActionToken_t actionTokens[] = {

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

static void UI_ExecuteActions(const uiAction_t* firstAction, uiCallContext_t *context);

/**
 * @brief Check if the action token list is sorted by alphabet,
 * else dichotomic search can't work
 */
static void UI_CheckActionTokenTypeSanity (void)
{
	int i;
	for (i = 0; i < lengthof(actionTokens) - 1; i++) {
		const char *a = actionTokens[i].token;
		const char *b = actionTokens[i + 1].token;
		if (strcmp(a, b) >= 0) {
			Sys_Error("UI_CheckActionTokenTypeSanity: '%s' is before '%s'. actionList must be sorted by alphabet", a, b);
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
int UI_GetActionTokenType (const char* token, int group)
{
	unsigned char min = 0;
	unsigned char max = lengthof(actionTokens);

	while (min != max) {
		const int mid = (min + max) >> 1;
		const int diff = strcmp(actionTokens[mid].token, token);
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
static inline const char* UI_GenCommandReadProperty (const char* input, char* output, int outputSize)
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
 * Get the number of param from an execution context
 * @param[in] context The execution context
 * @return The requested param
 */
int UI_GetParamNumber (const uiCallContext_t *context)
{
	if (context->useCmdParam)
		return Cmd_Argc() - 1;
	return context->paramNumber;
}

/**
 * Get a param from an execution context
 * @param[in] context The execution context
 * @param[in] paramID The ID of the requested param (first param is integer 1)
 * @return The requested param
 */
const char* UI_GetParam (const uiCallContext_t *context, int paramID)
{
	linkedList_t *current;
	assert(paramID >= 1);

	if (context->useCmdParam)
		return Cmd_Argv(paramID);

	if (paramID > context->paramNumber) {
		Com_Printf("UI_GetParam: %i out of range\n", paramID);
		return "";
	}

	current = context->params;
	while (paramID > 1) {
		current = current->next;
		paramID--;
	}

	return (char*) current->data;
}

/**
 * @brief Replace injection identifiers (e.g. &lt;eventParam&gt;) by a value
 * @note The injection identifier can be every node value - e.g. &lt;image&gt; or &lt;width&gt;.
 * It's also possible to do something like
 * @code
 * cmd "set someCvar &lt;min&gt;/&lt;max&gt;"
 * @endcode
 */
const char* UI_GenInjectedString (const char* input, qboolean addNewLine, const uiCallContext_t *context)
{
	static char cmd[256];
	int length = sizeof(cmd) - (addNewLine ? 2 : 1);
	static char propertyName[MAX_VAR];
	const char *cin = input;
	char *cout = cmd;

	while (length && cin[0] != '\0') {
		if (cin[0] == '<') {
			/* read propertyName between '<' and '>' */
			const char *next = UI_GenCommandReadProperty(cin, propertyName, sizeof(propertyName));
			if (next) {
				/* cvar injection */
				if (Q_strstart(propertyName, "cvar:")) {
					const cvar_t *cvar = Cvar_Get(propertyName + 5, "", 0, NULL);
					const int l = snprintf(cout, length, "%s", cvar->string);
					cout += l;
					cin = next;
					length -= l;
					continue;

				} else if (Q_strstart(propertyName, "node:")) {
					const char *path = propertyName + 5;
					uiNode_t *node;
					const value_t *property;
					const char* string;
					int l;
					UI_ReadNodePath(path, context->source, &node, &property);
					if (!node) {
						Com_Printf("UI_GenInjectedString: Node '%s' wasn't found; '' returned\n", path);
#ifdef DEBUG
						Com_Printf("UI_GenInjectedString: Path relative to '%s'\n", UI_GetPath(context->source));
#endif
						string = "";
					} else if (!property) {
						Com_Printf("UI_GenInjectedString: Property '%s' wasn't found; '' returned\n", path);
						string = "";
					} else {
						string = UI_GetStringFromNodeProperty(node, property);
						if (string == NULL) {
							Com_Printf("UI_GenInjectedString: String getter for '%s' property do not exists; '' injected\n", path);
							string = "";
						}
					}

					l = snprintf(cout, length, "%s", string);
					cout += l;
					cin = next;
					length -= l;
					continue;

				/* source path injection */
				} else if (Q_strstart(propertyName, "path:")) {
					if (context->source) {
						const char *command = propertyName + 5;
						const uiNode_t *node = NULL;
						if (Q_streq(command, "root"))
							node = context->source->root;
						else if (Q_streq(command, "this"))
							node = context->source;
						else if (Q_streq(command, "parent"))
							node = context->source->parent;
						else
							Com_Printf("UI_GenCommand: Command '%s' for path injection unknown\n", command);

						if (node) {
							const int l = snprintf(cout, length, "%s", UI_GetPath(node));
							cout += l;
							cin = next;
							length -= l;
							continue;
						}
					}

				/* no prefix */
				} else {
					/* source property injection */
					if (context->source) {
						/* find property definition */
						const value_t *property = UI_GetPropertyFromBehaviour(context->source->behaviour, propertyName);
						if (property) {
							const char* value;
							int l;
							/* inject the property value */
							value = UI_GetStringFromNodeProperty(context->source, property);
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
					if (UI_GetParamNumber(context) != 0) {
						int arg;
						const int checked = sscanf(propertyName, "%d", &arg);
						if (checked == 1 && arg >= 1 && arg <= UI_GetParamNumber(context)) {
							const int l = snprintf(cout, length, "%s", UI_GetParam(context, arg));
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

	/* copy the result into a free va slot */
	return va("%s", cmd);
}

/**
 * Apply an action value to a node property. If the tuple property/value allow it, the function
 * pre compute the value and update the action value to speed up the next call.
 * @param node Node to edit
 * @param property Property of the node to edit
 * @param value Action value containing the value to set to the node property
 * @param context Call context of the script
 * @todo refactoring it to remove "context", we should only call that function when the action
 * value is a leaf (then a value, and not an expression)
 */
static void UI_NodeSetPropertyFromActionValue (uiNode_t *node, const value_t *property, const uiCallContext_t *context, uiAction_t* value)
{
	/* @todo we can use a new EA_VALUE type to flag already parsed values, we dont need to do it again and again */
	/* pre compute value if possible */
	if (value->type == EA_VALUE_STRING) {
		const char* string = value->d.terminal.d1.constString;
		if ((property->type & V_UI_MASK) == V_UI_CVAR && Q_strstart(string, "*cvar:")) {
			void *mem = ((byte *) node + property->ofs);
			*(void**) mem = value->d.terminal.d1.data;
		} else {
			/** @todo here we must catch error in a better way, and using cvar for error code to create unittest automations */
			UI_InitRawActionValue(value, node, property, string);
		}
	}

	/* decode RAW value */
	if (value->type == EA_VALUE_RAW) {
		const void *rawValue = value->d.terminal.d1.constData;
		const int rawType = value->d.terminal.d2.integer;
		UI_NodeSetPropertyFromRAW(node, property, rawValue, rawType);
	}
	/* else it is an expression */
	else {
		/** @todo we should improve if when the prop is a boolean/int/float */
		const char* string = UI_GetStringFromExpression(value, context);
		UI_NodeSetProperty(node, property, string);
	}
}

static inline void UI_ExecuteSetAction (const uiAction_t* action, const uiCallContext_t *context)
{
	const char* path;
	uiNode_t *node;
	const value_t *property;
	const uiAction_t *left;
	uiAction_t *right;

	left = action->d.nonTerminal.left;
	if (left == NULL) {
		Com_Printf("UI_ExecuteSetAction: Action without left operand skipped.\n");
		return;
	}

	right = action->d.nonTerminal.right;
	if (right == NULL) {
		Com_Printf("UI_ExecuteSetAction: Action without right operand skipped.\n");
		return;
	}

	if (left->type == EA_VALUE_CVARNAME || left->type == EA_VALUE_CVARNAME_WITHINJECTION) {
		const char *cvarName;
		const char* textValue;

		if (left->type == EA_VALUE_CVARNAME)
			cvarName = left->d.terminal.d1.constString;
		else
			cvarName = UI_GenInjectedString(left->d.terminal.d1.constString, qfalse, context);

		textValue = UI_GetStringFromExpression(right, context);

		if (textValue[0] == '_')
			textValue = _(textValue + 1);

		Cvar_ForceSet(cvarName, textValue);
		return;
	}

	/* search the node */
	if (left->type == EA_VALUE_PATHPROPERTY)
		path = left->d.terminal.d1.constString;
	else if (left->type == EA_VALUE_PATHPROPERTY_WITHINJECTION)
		path = UI_GenInjectedString(left->d.terminal.d1.constString, qfalse, context);
	else
		Com_Error(ERR_FATAL, "UI_ExecuteSetAction: Property setter with wrong type '%d'", left->type);

	UI_ReadNodePath(path, context->source, &node, &property);
	if (!node) {
		Com_Printf("UI_ExecuteSetAction: node \"%s\" doesn't exist (source: %s)\n", path, UI_GetPath(context->source));
		return;
	}
	if (!property) {
		Com_Printf("UI_ExecuteSetAction: property \"%s\" doesn't exist (source: %s)\n", path, UI_GetPath(context->source));
		return;
	}

	UI_NodeSetPropertyFromActionValue(node, property, context, right);
}

static inline void UI_ExecuteCallAction (const uiAction_t* action, const uiCallContext_t *context)
{
	uiNode_t* callNode = NULL;
	uiAction_t* param;
	uiAction_t* left = action->d.nonTerminal.left;
	uiCallContext_t newContext;
	const value_t* callProperty = NULL;
	const char* path = left->d.terminal.d1.constString;

	if (left->type == EA_VALUE_PATHPROPERTY || left->type == EA_VALUE_PATHNODE)
		path = left->d.terminal.d1.constString;
	else if (left->type == EA_VALUE_PATHPROPERTY_WITHINJECTION || left->type == EA_VALUE_PATHNODE_WITHINJECTION)
		path = UI_GenInjectedString(left->d.terminal.d1.constString, qfalse, context);
	UI_ReadNodePath(path, context->source, &callNode, &callProperty);

	if (callNode == NULL) {
		Com_Printf("UI_ExecuteCallAction: Node from path \"%s\" not found (relative to \"%s\").\n", path, UI_GetPath(context->source));
		return;
	}

	if (callProperty != NULL && callProperty->type != V_UI_ACTION && callProperty->type != V_UI_NODEMETHOD) {
		Com_Printf("UI_ExecuteCallAction: Call operand %d unsupported. (%s)\n", callProperty->type, UI_GetPath(callNode));
		return;
	}

	newContext.source = callNode;
	newContext.params = NULL;
	newContext.paramNumber = 0;
	newContext.varNumber = 0;
	newContext.varPosition = context->varPosition + context->varNumber;

	if (action->type == EA_LISTENER) {
		newContext.useCmdParam = context->useCmdParam;

		if (!newContext.useCmdParam) {
			linkedList_t *p = context->params;
			while (p) {
				const char* value = (char*) p->data;
				LIST_AddString(&newContext.params, value);
				newContext.paramNumber++;
				p = p->next;
			}
		}
	} else {
		newContext.useCmdParam = qfalse;

		param = action->d.nonTerminal.right;
		while (param) {
			const char* value;
			value = UI_GetStringFromExpression(param, context);
			LIST_AddString(&newContext.params, value);
			newContext.paramNumber++;
			param = param->next;
		}
	}

	if (callProperty == NULL || callProperty->type == V_UI_ACTION) {
		uiAction_t *actionsRef = NULL;
		if (callProperty == NULL)
			actionsRef = callNode->onClick;
		else
			actionsRef = *(uiAction_t **) ((byte *) callNode + callProperty->ofs);
		UI_ExecuteActions(actionsRef, &newContext);
	} else if (callProperty->type == V_UI_NODEMETHOD) {
		uiNodeMethod_t func = (uiNodeMethod_t) callProperty->ofs;
		func(callNode, &newContext);
	} else {
		/* unreachable, already checked few line before */
		assert(qfalse);
	}

	LIST_Delete(&newContext.params);
}

/**
 * @brief Return a variable from the context
 * @param context Call context
 * @param relativeVarId id of the variable relative to the context
 */
uiValue_t* UI_GetVariable (const uiCallContext_t *context, int relativeVarId)
{
	const int varId = context->varPosition + relativeVarId;
	assert(relativeVarId >= 0);
	assert(relativeVarId < context->varNumber);
	return &(ui_global.variableStack[varId]);
}

static void UI_ReleaseVariable (uiValue_t *variable)
{
	/** @todo most of cases here are useless, it only should be EA_VALUE_STRING */
	switch (variable->type) {
	case EA_VALUE_STRING:
		UI_FreeStringProperty(variable->value.string);
		break;
	case EA_VALUE_FLOAT:
	case EA_VALUE_NODE:
	case EA_VALUE_CVAR:
		/* nothing */
		break;
	default:
		Com_Error(ERR_FATAL, "UI_ReleaseVariable: Variable type \"%d\" unsupported", variable->type);
	}

	/* bug safe, but useless */
	OBJZERO(*variable);
}

/**
 * @brief Execute an action from a source
 * @param[in] context Context node
 * @param[in] action Action to execute
 */
static void UI_ExecuteAction (const uiAction_t* action, uiCallContext_t *context)
{
	switch (action->type) {
	case EA_NULL:
		/* do nothing */
		break;

	case EA_CMD:
		/* execute a command */
		if (action->d.terminal.d1.constString)
			Cbuf_AddText(UI_GenInjectedString(action->d.terminal.d1.constString, qtrue, context));
		break;

	case EA_CALL:
	case EA_LISTENER:
		UI_ExecuteCallAction(action, context);
		break;

	case EA_POPVARS:
		{
			int i;
			const int number = action->d.terminal.d1.integer;
			assert(number <= context->varNumber);
			for (i = 0; i < number; i++) {
				const int varId = context->varPosition + context->varNumber - i - 1;
				UI_ReleaseVariable(&(ui_global.variableStack[varId]));
			}
			context->varNumber -= number;
		}
		break;

	case EA_PUSHVARS:
#ifdef DEBUG
		/* check sanity */
		/** @todo check var slots should be empty */
#endif
		context->varNumber += action->d.terminal.d1.integer;
		if (context->varNumber >= UI_MAX_VARIABLESTACK)
			Com_Error(ERR_FATAL, "UI_ExecuteAction: Variable stack full. UI_MAX_VARIABLESTACK hit.");
		break;

	case EA_ASSIGN:
		UI_ExecuteSetAction(action, context);
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
			while (UI_GetBooleanFromExpression(action->d.nonTerminal.left, context)) {
				UI_ExecuteActions(action->d.nonTerminal.right, context);
				if (loop > 1000) {
					Com_Printf("UI_ExecuteAction: Infinite loop. Force breaking 'while'\n");
					break;
				}
				loop++;
			}
			break;
		}

	case EA_IF:
		if (UI_GetBooleanFromExpression(action->d.nonTerminal.left, context)) {
			UI_ExecuteActions(action->d.nonTerminal.right, context);
			return;
		}
		action = action->next;
		while (action && action->type == EA_ELIF) {
			if (UI_GetBooleanFromExpression(action->d.nonTerminal.left, context)) {
				UI_ExecuteActions(action->d.nonTerminal.right, context);
				return;
			}
			action = action->next;
		}
		if (action && action->type == EA_ELSE) {
			UI_ExecuteActions(action->d.nonTerminal.right, context);
		}
		break;

	/** @todo Skipping actions like that is a bad way. the function should return the next action,
	 * or we should move IF,ELSE,ELIF... in a IF block and not interleave it with default actions
	 */
	case EA_ELSE:
	case EA_ELIF:
		/* previous EA_IF execute this action */
		break;

	default:
		Com_Error(ERR_FATAL, "UI_ExecuteAction: Unknown action type %i", action->type);
	}
}

static void UI_ExecuteActions (const uiAction_t* firstAction, uiCallContext_t *context)
{
	static int callnumber = 0;
	const uiAction_t *action;
	if (callnumber++ > 20) {
		Com_Printf("UI_ExecuteActions: Break possible infinit recursion\n");
		return;
	}
	for (action = firstAction; action; action = action->next) {
		UI_ExecuteAction(action, context);
	}
	callnumber--;
}

/**
 * @brief allow to inject command param into cmd of confunc command
 */
void UI_ExecuteConFuncActions (uiNode_t* source, const uiAction_t* firstAction)
{
	uiCallContext_t context;
	OBJZERO(context);
	context.source = source;
	context.useCmdParam = qtrue;
	UI_ExecuteActions(firstAction, &context);
}

void UI_ExecuteEventActions (uiNode_t* source, const uiAction_t* firstAction)
{
	uiCallContext_t context;
	OBJZERO(context);
	context.source = source;
	context.useCmdParam = qfalse;
	UI_ExecuteActions(firstAction, &context);
}

void UI_ExecuteEventActionsEx (uiNode_t* source, const uiAction_t* firstAction, linkedList_t *params)
{
	uiCallContext_t context;
	OBJZERO(context);
	context.source = source;
	context.useCmdParam = qfalse;
	context.params = params;
	context.paramNumber = LIST_Count(params);
	UI_ExecuteActions(firstAction, &context);
}

/**
 * @brief Test if a string use an injection syntax
 * @param[in] string The string to check for injection
 * @return True if we find the following syntax in the string "<" {thing without space} ">"
 */
qboolean UI_IsInjectedString (const char *string)
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
 * @brief Free a string property if it is allocated into ui_dynStringPool
 * @param[in,out] pointer The pointer to the data that should be freed
 * @sa ui_dynStringPool
 */
void UI_FreeStringProperty (void* pointer)
{
	/* skip const string */
	if ((uintptr_t)ui_global.adata <= (uintptr_t)pointer && (uintptr_t)pointer < (uintptr_t)ui_global.adata + (uintptr_t)ui_global.adataize)
		return;

	/* skip pointer out of ui_dynStringPool */
	if (!_Mem_AllocatedInPool(ui_dynStringPool, pointer))
		return;

	Mem_Free(pointer);
}

/**
 * @brief Allocate and initialize a command action
 * @param[in] command A command for the action
 * @return An initialised action
 */
uiAction_t* UI_AllocStaticCommandAction (const char *command)
{
	uiAction_t* action = UI_AllocStaticAction();
	action->type = EA_CMD;
	action->d.terminal.d1.constString = command;
	return action;
}

/**
 * @brief Set a new action to a @c uiAction_t pointer
 * @param[in,out] action Allocated action
 * @param[in] type Only @c EA_CMD is supported
 * @param[in] data The data for this action - in case of @c EA_CMD this is the commandline
 * @note You first have to free existing node actions - only free those that are
 * not static in @c ui_global.actions array
 * @todo we should create a function to free the memory. We can use a tag in the Mem_PoolAlloc
 * calls and use use Mem_FreeTag.
 */
void UI_PoolAllocAction (uiAction_t** action, int type, const void *data)
{
	if (*action)
		Com_Error(ERR_FATAL, "There is already an action assigned");
	*action = (uiAction_t *)Mem_PoolAlloc(sizeof(**action), ui_sysPool, 0);
	(*action)->type = type;
	switch (type) {
	case EA_CMD:
		(*action)->d.terminal.d1.constString = Mem_PoolStrDup((const char *)data, ui_sysPool, 0);
		break;
	default:
		Com_Error(ERR_FATAL, "Action type %i is not yet implemented", type);
	}
}

/**
 * @brief Add a callback of a function into a node event. There can be more than on listener.
 * @param[in,out] node The node to add the listener to.
 * @param[in] property The property of the node to add the listener to.
 * @param[in] functionNode The node of the listener callback.
 */
void UI_AddListener (uiNode_t *node, const value_t *property, const uiNode_t *functionNode)
{
	uiAction_t *lastAction;
	uiAction_t *action;
	uiAction_t *value;

	if (node->dynamic) {
		Com_Printf("UI_AddListener: '%s' is a dynamic node. We can't listen it.\n", UI_GetPath(node));
		return;
	}
	if (functionNode->dynamic) {
		Com_Printf("UI_AddListener: '%s' is a dynamic node. It can't be a listener callback.\n", UI_GetPath(functionNode));
		return;
	}

	/* create the call action */
	action = (uiAction_t*) Mem_PoolAlloc(sizeof(*action), ui_sysPool, 0);
	value = (uiAction_t*) Mem_PoolAlloc(sizeof(*action), ui_sysPool, 0);
	value->d.terminal.d1.constString = Mem_PoolStrDup(UI_GetPath(functionNode), ui_sysPool, 0);
	value->next = NULL;
	action->type = EA_LISTENER;
	action->d.nonTerminal.left = value;
	/** @todo It is a hack, we should remove that */
	action->d.terminal.d2.constData = &functionNode->onClick;
	action->next = NULL;

	/* insert the action */
	lastAction = *(uiAction_t**)((char*)node + property->ofs);
	if (lastAction) {
		while (lastAction->next)
			lastAction = lastAction->next;
		lastAction->next = action;
	} else
		*(uiAction_t**)((char*)node + property->ofs) = action;
}

/**
 * @brief add a call of a function into a node event
 */
static void UI_AddListener_f (void)
{
	uiNode_t *node;
	uiNode_t *function;
	const value_t *property;

	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <pathnode@event> <pathnode>\n", Cmd_Argv(0));
		return;
	}

	UI_ReadNodePath(Cmd_Argv(1), NULL, &node, &property);
	if (node == NULL) {
		Com_Printf("UI_AddListener_f: '%s' node not found.\n", Cmd_Argv(1));
		return;
	}
	if (property == NULL || property->type != V_UI_ACTION) {
		Com_Printf("UI_AddListener_f: '%s' property not found, or is not an event.\n", Cmd_Argv(1));
		return;
	}

	function = UI_GetNodeByPath(Cmd_Argv(2));
	if (function == NULL) {
		Com_Printf("UI_AddListener_f: '%s' node not found.\n", Cmd_Argv(2));
		return;
	}

	UI_AddListener(node, property, function);
}

/**
 * @brief Remove a function callback from a node event. There can be more than on listener.
 * @param[in,out] node The node to remove the listener from.
 * @param[in] property The property of the node to remove the listener from.
 * @param[in] functionNode The node of the listener callback.
 */
void UI_RemoveListener (uiNode_t *node, const value_t *property, uiNode_t *functionNode)
{
	void *data;
	uiAction_t *lastAction;

	/* data we must remove */
	data = (void*) &functionNode->onClick;

	/* remove the action */
	lastAction = *(uiAction_t**)((char*)node + property->ofs);
	if (lastAction) {
		uiAction_t *tmp = NULL;
		if (lastAction->type == EA_LISTENER && lastAction->d.terminal.d2.constData == data) {
			tmp = lastAction;
			*(uiAction_t**)((char*)node + property->ofs) = lastAction->next;
		} else {
			while (lastAction->next) {
				if (lastAction->next->type == EA_LISTENER && lastAction->next->d.terminal.d2.constData == data)
					break;
				lastAction = lastAction->next;
			}
			if (lastAction->next) {
				tmp = lastAction->next;
				lastAction->next = lastAction->next->next;
			}
		}
		if (tmp) {
			uiAction_t* value = tmp->d.nonTerminal.left;
			Mem_Free(value->d.terminal.d1.data);
			Mem_Free(value);
			Mem_Free(tmp);
		}
		else
			Com_Printf("UI_RemoveListener_f: '%s' into '%s' not found.\n", Cmd_Argv(2), Cmd_Argv(1));
	}
}

/**
 * @brief Remove a function callback from a node event
 */
static void UI_RemoveListener_f (void)
{
	uiNode_t *node;
	uiNode_t *function;
	const value_t *property;

	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <pathnode@event> <pathnode>\n", Cmd_Argv(0));
		return;
	}

	UI_ReadNodePath(Cmd_Argv(1), NULL, &node, &property);
	if (node == NULL) {
		Com_Printf("UI_RemoveListener_f: '%s' node not found.\n", Cmd_Argv(1));
		return;
	}
	if (property == NULL || property->type != V_UI_ACTION) {
		Com_Printf("UI_RemoveListener_f: '%s' property not found, or is not an event.\n", Cmd_Argv(1));
		return;
	}

	function = UI_GetNodeByPath(Cmd_Argv(2));
	if (function == NULL) {
		Com_Printf("UI_RemoveListener_f: '%s' node not found.\n", Cmd_Argv(2));
		return;
	}

	UI_RemoveListener(node, property, function);
}

static void UI_CvarChangeListener (const char *cvarName, const char *oldValue, const char *newValue, void *data)
{
	linkedList_t *list = (linkedList_t *) data;
	LIST_Foreach(list, char const, confunc) {
		UI_ExecuteConfunc("%s %s %s", confunc, oldValue, newValue);
	}
}

static void UI_AddCvarListener_f (void)
{
	const char *cvarName;
	const char *confuncName;
	cvarChangeListener_t *l;

	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <cvar> <confunc>\n", Cmd_Argv(0));
		return;
	}

	cvarName = Cmd_Argv(1);
	confuncName = Cmd_Argv(2);
	l = Cvar_RegisterChangeListener(cvarName, UI_CvarChangeListener);
	LIST_AddString((linkedList_t**)&l->data, confuncName);
}

static void UI_RemoveCvarListener_f (void)
{
	const char *cvarName;
	const char *confuncName;
	cvar_t *var;

	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <cvar> <confunc>\n", Cmd_Argv(0));
		return;
	}

	cvarName = Cmd_Argv(1);
	confuncName = Cmd_Argv(2);

	var = Cvar_FindVar(cvarName);
	if (var == NULL)
		return;

	cvarChangeListener_t *l = var->changeListener;
	while (l) {
		if (l->exec == UI_CvarChangeListener) {
			LIST_Remove((linkedList_t**)&l->data, confuncName);
			if (LIST_IsEmpty((linkedList_t*)l->data)) {
				Cvar_UnRegisterChangeListener(cvarName, UI_CvarChangeListener);
			}
			return;
		}
		l = l->next;
	}
}

void UI_InitActions (void)
{
	UI_CheckActionTokenTypeSanity();
	Cmd_AddCommand("ui_addcvarlistener", UI_AddCvarListener_f, "Add a confunc to a cvar change event");
	Cmd_AddCommand("ui_removecvarlistener", UI_RemoveCvarListener_f, "Remove a confunc from a cvar change event");
	/** @todo rework these commands to use a script language way */
	Cmd_AddCommand("ui_addlistener", UI_AddListener_f, "Add a function into a node event");
	Cmd_AddCommand("ui_removelistener", UI_RemoveListener_f, "Remove a function from a node event");
}
