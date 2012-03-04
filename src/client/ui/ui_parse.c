/**
 * @file ui_parse.c
 * @todo remove all "token" param from function and use Com_UnParseLastToken
 * @todo reduce use of uiGlobal (create global functions to add/get/... entities)
 * @todo remove Com_EParseValue and use Com_ParseValue
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

#include "../client.h"
#include "ui_parse.h"
#include "ui_main.h"
#include "ui_data.h"
#include "ui_internal.h"
#include "ui_actions.h"
#include "ui_sprite.h"
#include "ui_components.h"
#include "node/ui_node_window.h"
#include "node/ui_node_selectbox.h"
#include "node/ui_node_abstractnode.h"
#include "node/ui_node_abstractoption.h"

#include "../../shared/parse.h"

/** prototypes */
static qboolean UI_ParseProperty(void* object, const value_t *property, const char* objectName, const char **text, const char **token);
static uiAction_t *UI_ParseActionList(uiNode_t *node, const char **text, const const char **token);
static uiNode_t *UI_ParseNode(uiNode_t * parent, const char **text, const char **token, const char *errhead);

/** @brief valid properties for a UI model definition */
static const value_t uiModelProperties[] = {
	{"model", V_HUNK_STRING, offsetof(uiModel_t, model), 0},
	{"need", V_NULL, 0, 0},
	{"anim", V_HUNK_STRING, offsetof(uiModel_t, anim), 0},
	{"skin", V_INT, offsetof(uiModel_t, skin), sizeof(int)},
	{"color", V_COLOR, offsetof(uiModel_t, color), sizeof(vec4_t)},
	{"tag", V_HUNK_STRING, offsetof(uiModel_t, tag), 0},
	{"parent", V_HUNK_STRING, offsetof(uiModel_t, parent), 0},

	{NULL, V_NULL, 0, 0},
};

/** @brief reserved token preventing calling a node with it
 * @todo Use dichotomic search
 */
static char const* const reservedTokens[] = {
	"this",
	"parent",
	"root",
	"null",
	"super",
	"node",
	"cvar",
	"int",
	"float",
	"string",
	"var",
	NULL
};

static qboolean UI_TokenIsReserved (const char *name)
{
	char const* const* token = reservedTokens;
	while (*token) {
		if (Q_streq(*token, name))
			return qtrue;
		token++;
	}
	return qfalse;
}

static qboolean UI_TokenIsValue (const char *name, qboolean isQuoted)
{
	assert(name);
	if (isQuoted)
		return qtrue;
	/* is it a number */
	if ((name[0] >= '0' && name[0] <= '9') || name[0] == '-' || name[0] == '.')
		return qtrue;
	/* is it a var (*cvar:...) */
	if (name[0] == '*')
		return qtrue;
	if (Q_streq(name, "true"))
		return qtrue;
	if (Q_streq(name, "false"))
		return qtrue;

	/* uppercase const name */
	if ((name[0] >= 'A' && name[0] <= 'Z') || name[0] == '_') {
		qboolean onlyUpperCase = qtrue;
		while (*name != '\0') {
			if ((name[0] >= 'A' && name[0] <= 'Z') || name[0] == '_' || (name[0] >= '0' && name[0] <= '9')) {
				/* available chars */
			} else {
				return qfalse;
			}
			name++;
		}
		return onlyUpperCase;
	}

	return qfalse;
}

static qboolean UI_TokenIsName (const char *name, qboolean isQuoted)
{
	assert(name);
	if (isQuoted)
		return qfalse;
	if ((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= 'A' && name[0] <= 'Z') || name[0] == '_') {
		qboolean onlyUpperCase = qtrue;
		while (*name != '\0') {
			if (name[0] >= 'a' && name[0] <= 'z') {
				onlyUpperCase = qfalse;
			} else if ((name[0] >= '0' && name[0] <= '9') || (name[0] >= 'A' && name[0] <= 'Z') || name[0] == '_') {
				/* available chars */
			} else {
				return qfalse;
			}
			name++;
		}
		return !onlyUpperCase;
	}
	return qfalse;
}

/**
 * @brief Find a value_t by name into a array of value_t
 * @param[in] propertyList Array of value_t, with null termination
 * @param[in] name Property name we search
 * @return A value_t with the requested name, else NULL
 */
const value_t* UI_FindPropertyByName (const value_t* propertyList, const char* name)
{
	const value_t* current = propertyList;
	while (current->string != NULL) {
		if (!Q_strcasecmp(name, current->string))
			return current;
		current++;
	}
	return NULL;
}

/**
 * @brief Allocate a float into the UI static memory
 * @note Its not a dynamic memory allocation. Please only use it at the loading time
 * @param[in] count number of element need to allocate
 * @todo Assert out when we are not in parsing/loading stage
 */
float* UI_AllocStaticFloat (int count)
{
	float *result;
	assert(count > 0);
	result = UI_AllocHunkMemory(sizeof(float) * count, sizeof(float), qfalse);
	if (result == NULL)
		Com_Error(ERR_FATAL, "UI_AllocFloat: UI memory hunk exceeded - increase the size");
	return result;
}

/**
 * @brief Allocate a color into the UI static memory
 * @note Its not a dynamic memory allocation. Please only use it at the loading time
 * @param[in] count number of element need to allocate
 * @todo Assert out when we are not in parsing/loading stage
 */
vec4_t* UI_AllocStaticColor (int count)
{
	vec4_t *result;
	assert(count > 0);
	result = UI_AllocHunkMemory(sizeof(vec_t) * 4 * count, sizeof(vec_t), qfalse);
	if (result == NULL)
		Com_Error(ERR_FATAL, "UI_AllocColor: UI memory hunk exceeded - increase the size");
	return result;
}

/**
 * @brief Allocate a string into the UI static memory
 * @note Its not a dynamic memory allocation. Please only use it at the loading time
 * @param[in] string Use to initialize the string
 * @param[in] size request a fixed memory size, if 0 the string size is used
 * @todo Assert out when we are not in parsing/loading stage
 */
char* UI_AllocStaticString (const char* string, int size)
{
	char* result;
	if (size == 0) {
		size = strlen(string) + 1;
	}
	result = UI_AllocHunkMemory(size, sizeof(char), qfalse);
	if (result == NULL)
		Com_Error(ERR_FATAL, "UI_AllocString: UI memory hunk exceeded - increase the size");
	Q_strncpyz(result, string, size);
	return result;
}

/**
 * @brief Allocate an action
 * @return An action
 */
uiAction_t *UI_AllocStaticAction (void)
{
	if (ui_global.numActions >= UI_MAX_ACTIONS)
		Com_Error(ERR_FATAL, "UI_AllocAction: Too many UI actions");
	return &ui_global.actions[ui_global.numActions++];
}

/**
 * Parse a string according to a property type, and allocate a raw value to the static memory
 *
 * @param action Action to initialize
 * @param node Current node we are parsing, only used for error message
 * @param property Type of the value to parse, if NULL the string is not stored as string
 * @param string String value to parse
 * @return True if the action is initialized
 * @todo remove node param and catch error where we call that function
 */
qboolean UI_InitRawActionValue (uiAction_t* action, uiNode_t *node, const value_t *property, const char *string)
{
	if (property == NULL) {
		action->type = EA_VALUE_STRING;
		action->d.terminal.d1.data = UI_AllocStaticString(string, 0);
		action->d.terminal.d2.integer = 0;
		return qtrue;
	}

	if (property->type == V_UI_SPRITEREF) {
		uiSprite_t* sprite = UI_GetSpriteByName(string);
		if (sprite == NULL) {
			Com_Printf("UI_ParseSetAction: sprite '%s' not found (%s)\n", string, UI_GetPath(node));
			return qfalse;
		}
		action->type = EA_VALUE_RAW;
		action->d.terminal.d1.data = sprite;
		action->d.terminal.d2.integer = property->type;
		return qtrue;
	} else {
		const int baseType = property->type & V_UI_MASK;
		if (baseType != 0 && baseType != V_UI_CVAR) {
			Com_Printf("UI_ParseRawValue: setter for property '%s' (type %d, 0x%X) is not supported (%s)\n", property->string, property->type, property->type, UI_GetPath(node));
			return qfalse;
		}
		ui_global.curadata = Com_AlignPtr(ui_global.curadata, property->type & V_BASETYPEMASK);
		action->type = EA_VALUE_RAW;
		action->d.terminal.d1.data = ui_global.curadata;
		action->d.terminal.d2.integer = property->type;
		/** @todo we should hide use of ui_global.curadata */
		ui_global.curadata += Com_EParseValue(ui_global.curadata, string, property->type & V_BASETYPEMASK, 0, property->size);
		return qtrue;
	}
}

/**
 * @brief Parser for setter command
 */
static qboolean UI_ParseSetAction (uiNode_t *node, uiAction_t *action, const char **text, const char **token, const char *errhead)
{
	const value_t *property;
	int type;
	uiAction_t* localAction;

	assert((*token)[0] == '*');

	Com_UnParseLastToken();
	action->d.nonTerminal.left = UI_ParseExpression(text);

	type = action->d.nonTerminal.left->type;
	if (type != EA_VALUE_CVARNAME && type != EA_VALUE_CVARNAME_WITHINJECTION
		&& type != EA_VALUE_PATHPROPERTY && type != EA_VALUE_PATHPROPERTY_WITHINJECTION) {
		Com_Printf("UI_ParseSetAction: Cvar or Node property expected. Type '%i' found\n", type);
		return qfalse;
	}

	/* must use "equal" char between name and value */
	*token = Com_EParse(text, errhead, NULL);
	if (!*text)
		return qfalse;
	if (!Q_streq(*token, "=")) {
		Com_Printf("UI_ParseSetAction: Assign sign '=' expected between variable and value. '%s' found in node %s.\n", *token, UI_GetPath(node));
		return qfalse;
	}

	/* get the value */
	if (type == EA_VALUE_CVARNAME || type == EA_VALUE_CVARNAME_WITHINJECTION) {
		action->d.nonTerminal.right = UI_ParseExpression(text);
		return qtrue;
	}

	property = (const value_t *) action->d.nonTerminal.left->d.terminal.d2.data;

	*token = Com_EParse(text, errhead, NULL);
	if (!*text)
		return qfalse;

	if (Q_streq(*token, "{")) {
		uiAction_t* actionList;

		if (property != NULL && property->type != V_UI_ACTION) {
			Com_Printf("UI_ParseSetAction: Property %s@%s do not expect code block.\n", UI_GetPath(node), property->string);
			return qfalse;
		}

		actionList = UI_ParseActionList(node, text, token);
		if (actionList == NULL)
			return qfalse;

		localAction = UI_AllocStaticAction();
		localAction->type = EA_VALUE_RAW;
		localAction->d.terminal.d1.data = actionList;
		localAction->d.terminal.d2.integer = V_UI_ACTION;
		action->d.nonTerminal.right = localAction;

		return qtrue;
	}

	if (Q_streq(*token, "(")) {
		Com_UnParseLastToken();
		action->d.nonTerminal.right = UI_ParseExpression(text);
		return qtrue;
	}

	/* @todo everything should come from UI_ParseExpression */

	if (UI_IsInjectedString(*token)) {
		localAction = UI_AllocStaticAction();
		localAction->type = EA_VALUE_STRING_WITHINJECTION;
		localAction->d.terminal.d1.data = UI_AllocStaticString(*token, 0);
		action->d.nonTerminal.right = localAction;
		return qtrue;
	}

	localAction = UI_AllocStaticAction();
	UI_InitRawActionValue(localAction, node, property, *token);
	action->d.nonTerminal.right = localAction;
	return qtrue;
}

/**
 * @brief Parser for c command
 */
static qboolean UI_ParseCallAction (uiNode_t *node, uiAction_t *action, const char **text, const char **token, const char *errhead)
{
	uiAction_t *expression;
	uiAction_t *lastParam = NULL;
	int paramID = 0;
	expression = UI_ParseExpression(text);
	if (expression == NULL)
		return qfalse;

	if (expression->type != EA_VALUE_PATHNODE_WITHINJECTION && expression->type != EA_VALUE_PATHNODE && expression->type != EA_VALUE_PATHPROPERTY && expression->type != EA_VALUE_PATHPROPERTY_WITHINJECTION) {
		Com_Printf("UI_ParseCallAction: \"call\" keyword only support pathnode and pathproperty (node: %s)\n", UI_GetPath(node));
		return qfalse;
	}

	action->d.nonTerminal.left = expression;

	/* check parameters */
	*token = Com_EParse(text, errhead, NULL);
	if ((*token)[0] == '\0')
		return qfalse;

	/* there is no parameters */
	if (!Q_streq(*token, "(")) {
		Com_UnParseLastToken();
		return qtrue;
	}

	/* read parameters */
	do {
		uiAction_t *param;
		paramID++;

		/* parameter */
		param = UI_ParseExpression(text);
		if (param == NULL) {
			Com_Printf("UI_ParseCallAction: problem with the %i parameter\n", paramID);
			return qfalse;
		}
		if (lastParam == NULL)
			action->d.nonTerminal.right = param;
		else
			lastParam->next = param;
		lastParam = param;

		/* separator */
		*token = Com_EParse(text, errhead, NULL);
		if (!*token)
			return qfalse;
		if (!Q_streq(*token, ",")) {
			if (Q_streq(*token, ")"))
				break;
			Com_UnParseLastToken();
			Com_Printf("UI_ParseCallAction: Invalidate end of 'call' after param %i\n", paramID);
			return qfalse;
		}
	} while(qtrue);

	return qtrue;
}

/**
 * @brief Parse actions and return action list
 * @return The first element from a list of action
 * @sa ea_t
 * @todo need cleanup, compute action out of the final memory; reduce number of var
 */
static uiAction_t *UI_ParseActionList (uiNode_t *node, const char **text, const const char **token)
{
	const char *errhead = "UI_ParseActionList: unexpected end of file (in event)";
	uiAction_t *firstAction;
	uiAction_t *lastAction;
	uiAction_t *action;
	qboolean result;

	lastAction = NULL;
	firstAction = NULL;

	/* prevent bad position */
	if ((*token)[0] != '{') {
		Com_Printf("UI_ParseActionList: token \"{\" expected, but \"%s\" found (in event) (node: %s)\n", *token, UI_GetPath(node));
		return NULL;
	}

	while (qtrue) {
		int type = EA_NULL;

		/* get new token */
		*token = Com_EParse(text, errhead, NULL);
		if (!*token)
			return NULL;

		if ((*token)[0] == '}')
			break;

		type = UI_GetActionTokenType(*token, EA_ACTION);
		/* setter form */
		if (type == EA_NULL && (*token)[0] == '*')
			type = EA_ASSIGN;

		/* unknown, we break the parsing */
		if (type == EA_NULL) {
			Com_Printf("UI_ParseActionList: unknown token \"%s\" ignored (in event) (node: %s)\n", *token, UI_GetPath(node));
			return NULL;
		}

		/* add the action */
		action = UI_AllocStaticAction();
		/** @todo better to append the action after initialization */
		if (lastAction)
			lastAction->next = action;
		if (!firstAction)
			firstAction = action;
		action->type = type;

		/* decode action */
		switch (action->type) {
		case EA_CMD:
			/* get parameter values */
			*token = Com_EParse(text, errhead, NULL);
			if (!*text)
				return NULL;

			/* get the value */
			action->d.terminal.d1.constString = UI_AllocStaticString(*token, 0);
			break;

		case EA_ASSIGN:
			result = UI_ParseSetAction(node, action, text, token, errhead);
			if (!result)
				return NULL;
			break;

		case EA_CALL:
			result = UI_ParseCallAction(node, action, text, token, errhead);
			if (!result)
				return NULL;
			break;

		case EA_DELETE:
			{
				uiAction_t *expression;
				expression = UI_ParseExpression(text);
				if (expression == NULL)
					return NULL;

				if (expression->type != EA_VALUE_CVARNAME) {
					Com_Printf("UI_ParseActionList: \"delete\" keyword only support cvarname (node: %s)\n", UI_GetPath(node));
					return NULL;
				}

				action->d.nonTerminal.left = expression;
				break;
			}

		case EA_ELIF:
			/* check previous action */
			if (!lastAction || (lastAction->type != EA_IF && lastAction->type != EA_ELIF)) {
				Com_Printf("UI_ParseActionList: 'elif' must be set after an 'if' or an 'elif' (node: %s)\n", UI_GetPath(node));
				return NULL;
			}
			/* then it execute EA_IF, no break */
		case EA_WHILE:
		case EA_IF:
			{
				uiAction_t *expression;

				/* get the condition */
				expression = UI_ParseExpression(text);
				if (expression == NULL)
					return NULL;
				action->d.nonTerminal.left = expression;

				/* get the action block */
				*token = Com_EParse(text, errhead, NULL);
				if (!*text)
					return NULL;
				action->d.nonTerminal.right = UI_ParseActionList(node, text, token);
				if (action->d.nonTerminal.right == NULL) {
					if (action->type == EA_IF)
						Com_Printf("UI_ParseActionList: block expected after \"if\" (node: %s)\n", UI_GetPath(node));
					else if (action->type == EA_ELIF)
						Com_Printf("UI_ParseActionList: block expected after \"elif\" (node: %s)\n", UI_GetPath(node));
					else
						Com_Printf("UI_ParseActionList: block expected after \"while\" (node: %s)\n", UI_GetPath(node));
					return NULL;
				}
				break;
			}

		case EA_ELSE:
			/* check previous action */
			if (!lastAction || (lastAction->type != EA_IF && lastAction->type != EA_ELIF)) {
				Com_Printf("UI_ParseActionList: 'else' must be set after an 'if' or an 'elif' (node: %s)\n", UI_GetPath(node));
				return NULL;
			}

			/* get the action block */
			*token = Com_EParse(text, errhead, NULL);
			if (!*text)
				return NULL;
			action->d.nonTerminal.left = NULL;
			action->d.nonTerminal.right = UI_ParseActionList(node, text, token);
			if (action->d.nonTerminal.right == NULL) {
				Com_Printf("UI_ParseActionList: block expected after \"else\" (node: %s)\n", UI_GetPath(node));
				return NULL;
			}
			break;

		default:
			assert(qfalse);
		}

		/* step */
		lastAction = action;
	}

	assert((*token)[0] == '}');

	/* return non NULL value */
	if (firstAction == NULL) {
		firstAction = UI_AllocStaticAction();
	}

	return firstAction;
}

static qboolean UI_ParseExcludeRect (uiNode_t * node, const char **text, const char **token, const char *errhead)
{
	uiExcludeRect_t rect;
	uiExcludeRect_t *newRect;

	/* get parameters */
	*token = Com_EParse(text, errhead, node->name);
	if (!*text)
		return qfalse;
	if ((*token)[0] != '{') {
		Com_Printf("UI_ParseExcludeRect: node with bad excluderect ignored (node \"%s\")\n", UI_GetPath(node));
		return qtrue;
	}

	do {
		*token = Com_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;
		/** @todo move it into a property array */
		if (Q_streq(*token, "pos")) {
			*token = Com_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;
			Com_EParseValue(&rect, *token, V_POS, offsetof(uiExcludeRect_t, pos), sizeof(vec2_t));
		} else if (Q_streq(*token, "size")) {
			*token = Com_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;
			Com_EParseValue(&rect, *token, V_POS, offsetof(uiExcludeRect_t, size), sizeof(vec2_t));
		}
	} while ((*token)[0] != '}');

	newRect = (uiExcludeRect_t*) UI_AllocHunkMemory(sizeof(*newRect), STRUCT_MEMORY_ALIGN, qfalse);
	if (newRect == NULL) {
		Com_Printf("UI_ParseExcludeRect: ui hunk memory exceeded.");
		return qfalse;
	}

	/* move data to final memory and link to node */
	*newRect = rect;
	newRect->next = node->firstExcludeRect;
	node->firstExcludeRect = newRect;
	return qtrue;
}

static qboolean UI_ParseEventProperty (uiNode_t * node, const value_t *event, const char **text, const char **token, const char *errhead)
{
	uiAction_t **action;

	/* add new actions to end of list */
	action = (uiAction_t **) ((byte *) node + event->ofs);
	for (; *action; action = &(*action)->next) {}

	/* get the action body */
	*token = Com_EParse(text, errhead, node->name);
	if (!*text)
		return qfalse;

	if ((*token)[0] != '{') {
		Com_Printf("UI_ParseEventProperty: Event '%s' without body (%s)\n", event->string, UI_GetPath(node));
		return qfalse;
	}

	Com_EnableFunctionScriptToken(qtrue);

	*action = UI_ParseActionList(node, text, token);
	if (*action == NULL)
		return qfalse;

	Com_EnableFunctionScriptToken(qfalse);

	/* block terminal already read */
	assert((*token)[0] == '}');

	return qtrue;
}

/**
 * @brief Parse a property value
 * @todo don't read the next token (need to change the script language)
 */
static qboolean UI_ParseProperty (void* object, const value_t *property, const char* objectName, const char **text, const char **token)
{
	const char *errhead = "UI_ParseProperty: unexpected end of file (object";
	static const char *notWellFormedValue = "UI_ParseProperty: \"%s\" is not a well formed node name (it must be quoted, uppercase const, a number, or prefixed with '*')\n";
	size_t bytes;
	void *valuePtr = (void*) ((uintptr_t)object + property->ofs);
	int result;
	const int specialType = property->type & V_UI_MASK;

	if (property->type == V_NULL) {
		return qfalse;
	}

	switch (specialType) {
	case V_NOT_UI:	/* common type */

		*token = Com_EParse(text, errhead, objectName);
		if (!*text)
			return qfalse;
		if (!UI_TokenIsValue(*token, Com_ParsedTokenIsQuoted())) {
			Com_Printf(notWellFormedValue, *token);
			return qfalse;
		}

		if (property->type == V_TRANSLATION_STRING) {
			/* selectbox values are static arrays */
			char *target = (char*) valuePtr;
			const char *translatableToken = *token;
			assert(property->size);
			if (translatableToken[0] == '_')
				translatableToken++;
			Q_strncpyz(target, translatableToken, property->size);
		} else {
			result = Com_ParseValue(object, *token, property->type, property->ofs, property->size, &bytes);
			if (result != RESULT_OK) {
				Com_Printf("UI_ParseProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
				return qfalse;
			}
		}
		break;

	case V_UI_REF:
		*token = Com_EParse(text, errhead, objectName);
		if (!*text)
			return qfalse;
		if (!UI_TokenIsValue(*token, Com_ParsedTokenIsQuoted())) {
			Com_Printf(notWellFormedValue, *token);
			return qfalse;
		}

		/* a reference to data is handled like this */
		ui_global.curadata = Com_AlignPtr(ui_global.curadata, property->type & V_BASETYPEMASK);
		*(byte **) ((byte *) object + property->ofs) = ui_global.curadata;

		/** @todo check for the moment its not a cvar */
		assert((*token)[0] != '*');

		/* sanity check */
		if ((property->type & V_BASETYPEMASK) == V_STRING && strlen(*token) > MAX_VAR - 1) {
			Com_Printf("UI_ParseProperty: Value '%s' is too long (key %s)\n", *token, property->string);
			return qfalse;
		}

		result = Com_ParseValue(ui_global.curadata, *token, property->type & V_BASETYPEMASK, 0, property->size, &bytes);
		if (result != RESULT_OK) {
			Com_Printf("UI_ParseProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
			return qfalse;
		}
		ui_global.curadata += bytes;

		break;

	case V_UI_CVAR:	/* common type */
		*token = Com_EParse(text, errhead, objectName);
		if (!*text)
			return qfalse;
		if (!UI_TokenIsValue(*token, Com_ParsedTokenIsQuoted())) {
			Com_Printf(notWellFormedValue, *token);
			return qfalse;
		}

		/* references are parsed as string */
		if ((*token)[0] == '*') {
			/* a reference to data */
			ui_global.curadata = Com_AlignPtr(ui_global.curadata, V_STRING);
			*(byte **) valuePtr = ui_global.curadata;

			/* sanity check */
			if (strlen(*token) > MAX_VAR - 1) {
				Com_Printf("UI_ParseProperty: Value '%s' is too long (key %s)\n", *token, property->string);
				return qfalse;
			}

			result = Com_ParseValue(ui_global.curadata, *token, V_STRING, 0, 0, &bytes);
			if (result != RESULT_OK) {
				Com_Printf("UI_ParseProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
				return qfalse;
			}
			ui_global.curadata += bytes;
		} else {
			/* a reference to data */
			ui_global.curadata = Com_AlignPtr(ui_global.curadata, property->type & V_BASETYPEMASK);
			*(byte **) valuePtr = ui_global.curadata;

			/* sanity check */
			if ((property->type & V_BASETYPEMASK) == V_STRING && strlen(*token) > MAX_VAR - 1) {
				Com_Printf("UI_ParseProperty: Value '%s' is too long (key %s)\n", *token, property->string);
				return qfalse;
			}

			result = Com_ParseValue(ui_global.curadata, *token, property->type & V_BASETYPEMASK, 0, property->size, &bytes);
			if (result != RESULT_OK) {
				Com_Printf("UI_ParseProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
				return qfalse;
			}
			ui_global.curadata += bytes;
		}
		break;

	case V_UI:

		switch ((int)property->type) {
		case V_UI_ACTION:
			result = UI_ParseEventProperty((uiNode_t *)object, property, text, token, errhead);
			if (!result)
				return qfalse;
			break;

		case V_UI_EXCLUDERECT:
			result = UI_ParseExcludeRect((uiNode_t *)object, text, token, errhead);
			if (!result)
				return qfalse;
			break;

		case V_UI_SPRITEREF:
			{
				uiSprite_t** sprite = (uiSprite_t**) valuePtr;
				*token = Com_EParse(text, errhead, objectName);
				if (!*text)
					return qfalse;

				*sprite = UI_GetSpriteByName(*token);
				if (*sprite == NULL) {
					Com_Printf("UI_ParseProperty: sprite '%s' not found (object %s)\n", *token, objectName);
				}
			}
			break;

		case V_UI_IF:
			{
				uiAction_t **expression = (uiAction_t **) valuePtr;

				*token = Com_EParse(text, errhead, objectName);
				if (!*text)
					return qfalse;

				*expression = UI_AllocStaticStringCondition(*token);
				if (*expression == NULL)
					return qfalse;
			}
			break;

		case V_UI_DATAID:
			{
				int *dataId = (int*) valuePtr;
				*token = Com_EParse(text, errhead, objectName);
				if (!*text)
					return qfalse;

				*dataId = UI_GetDataIDByName(*token);
				if (*dataId < 0) {
					Com_Printf("UI_ParseProperty: Could not find shared data ID '%s' (%s@%s)\n",
							*token, objectName, property->string);
					return qfalse;
				}
			}
			break;

		default:
			Com_Printf("UI_ParseProperty: unknown property type '%d' (0x%X) (%s@%s)\n",
					property->type, property->type, objectName, property->string);
			return qfalse;
		}
		break;

	default:
		Com_Printf("UI_ParseProperties: unknown property type '%d' (0x%X) (%s@%s)\n",
				property->type, property->type, objectName, property->string);
		return qfalse;
	}

	return qtrue;
}

static qboolean UI_ParseFunction (uiNode_t * node, const char **text, const char **token)
{
	uiAction_t **action;
	assert(node->behaviour->isFunction);

	Com_EnableFunctionScriptToken(qtrue);

	action = &node->onClick;
	*action = UI_ParseActionList(node, text, token);
	if (*action == NULL)
		return qfalse;

	Com_EnableFunctionScriptToken(qfalse);

	return (*token)[0] == '}';
}

/**
 * @sa UI_ParseNodeProperties
 * @brief parse all sequencial properties into a block
 * @note allow to use an extra block
 * @code
 * foobehaviour foonode {
 *   { properties }
 *   // the function stop reading here
 *   nodes
 * }
 * foobehaviour foonode {
 *   properties
 *   // the function stop reading here
 *   nodes
 * }
 * @endcode
 */
static qboolean UI_ParseNodeProperties (uiNode_t * node, const char **text, const char **token)
{
	const char *errhead = "UI_ParseNodeProperties: unexpected end of file (node";
	qboolean nextTokenAlreadyRead = qfalse;

	if ((*token)[0] != '{')
		nextTokenAlreadyRead = qtrue;

	do {
		const value_t *val;
		int result;

		/* get new token */
		if (!nextTokenAlreadyRead) {
			*token = Com_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;
		} else {
			nextTokenAlreadyRead = qfalse;
		}

		/* is finished */
		if ((*token)[0] == '}')
			break;

		/* find the property */
		val = UI_GetPropertyFromBehaviour(node->behaviour, *token);
		if (!val) {
			/* unknown token, print message and continue */
			Com_Printf("UI_ParseNodeProperties: unknown property \"%s\", node ignored (node %s)\n",
					*token, UI_GetPath(node));
			return qfalse;
		}

		/* get parameter values */
		result = UI_ParseProperty(node, val, node->name, text, token);
		if (!result) {
			Com_Printf("UI_ParseNodeProperties: Problem with parsing of node property '%s@%s'. See upper\n",
					UI_GetPath(node), val->string);
			return qfalse;
		}

	} while (*text);

	return qtrue;
}

/**
 * @brief Read a node body
 * @note Node header already read, we are over the node name, or '{'
 * @code
 * Allowed syntax
 * { properties }
 * OR
 * { nodes }
 * OR
 * { { properties } nodes }
 * @endcode
 */
static qboolean UI_ParseNodeBody (uiNode_t * node, const char **text, const char **token, const char *errhead)
{
	qboolean result = qtrue;

	if ((*token)[0] != '{') {
		/* read the body block start */
		*token = Com_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;
		if ((*token)[0] != '{') {
			Com_Printf("UI_ParseNodeBody: node doesn't have body, token '%s' read (node \"%s\")\n", *token, UI_GetPath(node));
			ui_global.numNodes--;
			return qfalse;
		}
	}

	/* functions are a special case */
	if (node->behaviour->isFunction) {
		result = UI_ParseFunction(node, text, token);
	} else {

		/* check the content */
		*token = Com_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;

		if ((*token)[0] == '{') {
			/* we have a special block for properties */
			result = UI_ParseNodeProperties(node, text, token);
			if (!result)
				return qfalse;

			/* move token over the next node behaviour */
			*token = Com_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;

			/* and then read all nodes */
			while ((*token)[0] != '}') {
				uiNode_t *newNode = UI_ParseNode(node, text, token, errhead);
				if (!newNode)
					return qfalse;

				*token = Com_EParse(text, errhead, node->name);
				if (*text == NULL)
					return qfalse;
			}
		} else if (UI_GetPropertyFromBehaviour(node->behaviour, *token)) {
			/* we should have a block with properties only */
			result = UI_ParseNodeProperties(node, text, token);
		} else {
			/* we should have a block with nodes only */
			while ((*token)[0] != '}') {
				uiNode_t *newNode = UI_ParseNode(node, text, token, errhead);
				if (!newNode)
					return qfalse;

				*token = Com_EParse(text, errhead, node->name);
				if (*text == NULL)
					return qfalse;
			}
		}
	}
	if (!result) {
		Com_Printf("UI_ParseNodeBody: node with bad body ignored (node \"%s\")\n", UI_GetPath(node));
		ui_global.numNodes--;
		return qfalse;
	}

	/* already check on UI_ParseNodeProperties */
	assert((*token)[0] == '}');
	return qtrue;
}

/**
 * @brief parse a node
 * @sa UI_ParseNodeProperties
 * @todo we can think about merging UI_ParseNodeProperties here
 * @note first token already read
 * @note dont read more than the need token (last right token is '}' of end of node)
 */
static uiNode_t *UI_ParseNode (uiNode_t * parent, const char **text, const char **token, const char *errhead)
{
	uiNode_t *node = NULL;
	uiBehaviour_t *behaviour;
	uiNode_t *component = NULL;
	qboolean result;

	/* allow to begin with the identifier "node" before the behaviour name */
	if (!Q_strcasecmp(*token, "node")) {
		*token = Com_EParse(text, errhead, "");
		if (!*text)
			return NULL;
	}

	/* get the behaviour */
	behaviour = UI_GetNodeBehaviour(*token);
	if (!behaviour) {
		component = UI_GetComponent(*token);
	}
	if (behaviour == NULL && component == NULL) {
		Com_Printf("UI_ParseNode: node behaviour/component '%s' doesn't exists\n", *token);
		return NULL;
	}

	/* get the name */
	*token = Com_EParse(text, errhead, "");
	if (!*text)
		return NULL;
	if (!UI_TokenIsName(*token, Com_ParsedTokenIsQuoted())) {
		Com_Printf("UI_ParseNode: \"%s\" is not a well formed node name ([a-zA-Z_][a-zA-Z0-9_]*)\n", *token);
		return NULL;
	}
	if (UI_TokenIsReserved(*token)) {
		Com_Printf("UI_ParseNode: \"%s\" is a reserved token, we can't call a node with it\n", *token);
		return NULL;
	}

	/* test if node already exists */
	/* Already existing node should only come from inherited node,we should not have 2 definitions of the same node into the same window. */
	if (parent)
		node = UI_GetNode(parent, *token);

	/* reuse a node */
	if (node) {
		if (node->behaviour != behaviour) {
			Com_Printf("UI_ParseNode: we can't change node type (node \"%s\")\n", UI_GetPath(node));
			return NULL;
		}
		Com_DPrintf(DEBUG_CLIENT, "... over-riding node %s\n", UI_GetPath(node));

	/* else initialize a component */
	} else if (component) {
		node = UI_CloneNode(component, NULL, qtrue, *token, qfalse);
		if (parent) {
			if (parent->root)
				UI_UpdateRoot(node, parent->root);
			UI_AppendNode(parent, node);
		}

	/* else initialize a new node */
	} else {
		node = UI_AllocNode(*token, behaviour->name, qfalse);
		node->parent = parent;
		if (parent)
			node->root = parent->root;
		/** @todo move it into caller */
		if (parent)
			UI_AppendNode(parent, node);
	}

	/* get body */
	result = UI_ParseNodeBody(node, text, token, errhead);
	if (!result)
		return NULL;

	/* validate properties */
	if (node->behaviour->loaded)
		node->behaviour->loaded(node);

	return node;
}

/**
 * @brief parses the models.ufo and all files where UI models (menu_model) are defined
 * @sa CL_ParseClientData
 */
qboolean UI_ParseUIModel (const char *name, const char **text)
{
	uiModel_t *model;
	const char *token;
	int i;
	const value_t *v = NULL;
	const char *errhead = "UI_ParseUIModel: unexpected end of file (names ";

	/* search for a UI models with same name */
	for (i = 0; i < ui_global.numModels; i++)
		if (Q_streq(ui_global.models[i].id, name)) {
			Com_Printf("UI_ParseUIModel: menu_model \"%s\" with same name found, second ignored\n", name);
			return qfalse;
		}

	if (ui_global.numModels >= UI_MAX_MODELS) {
		Com_Printf("UI_ParseUIModel: Max UI models reached\n");
		return qfalse;
	}

	/* initialize the model */
	model = &ui_global.models[ui_global.numModels];
	OBJZERO(*model);

	Vector4Set(model->color, 1, 1, 1, 1);

	model->id = Mem_PoolStrDup(name, ui_sysPool, 0);
	Com_DPrintf(DEBUG_CLIENT, "Found UI model %s (%i)\n", model->id, ui_global.numModels);

	/* get it's body */
	token = Com_Parse(text);

	if (!*text || token[0] != '{') {
		Com_Printf("UI_ParseUIModel: Model \"%s\" without body ignored\n", model->id);
		return qfalse;
	}

	ui_global.numModels++;

	do {
		/* get the name type */
		token = Com_EParse(text, errhead, name);
		if (!*text)
			return qfalse;
		if (token[0] == '}')
			break;

		v = UI_FindPropertyByName(uiModelProperties, token);
		if (!v)
			Com_Printf("UI_ParseUIModel: unknown token \"%s\" ignored (UI model %s)\n", token, name);

		if (v->type == V_NULL) {
			if (Q_streq(v->string, "need")) {
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return qfalse;
				if (model->next != NULL)
					Sys_Error("UI_ParseUIModel: second 'need' token found in model %s", name);
				model->next = UI_GetUIModel(token);
				 if (!model->next)
					Com_Printf("Could not find UI model %s", token);
			}
		} else {
			token = Com_EParse(text, errhead, name);
			if (!*text)
				return qfalse;
			switch (v->type) {
			case V_HUNK_STRING:
				Mem_PoolStrDupTo(token, (char**) ((char*)model + (int)v->ofs), ui_sysPool, 0);
				break;
			default:
				Com_EParseValue(model, token, v->type, v->ofs, v->size);
				break;
			}
		}
	} while (*text);

	return qtrue;
}

qboolean UI_ParseSprite (const char *name, const char **text)
{
	uiSprite_t *icon;
	const char *token;

	/* search for icons with same name */
	icon = UI_AllocStaticSprite(name);

	/* get it's body */
	token = Com_Parse(text);
	assert(token[0] == '{');

	/* read properties */
	while (qtrue) {
		const value_t *property;
		qboolean result;

		token = Com_Parse(text);
		if (*text == NULL)
			return qfalse;

		if (token[0] == '}')
			break;

		property = UI_FindPropertyByName(ui_spriteProperties, token);
		if (!property) {
			Com_Printf("UI_ParseIcon: unknown options property: '%s' - ignore it\n", token);
			return qfalse;
		}

		/* get parameter values */
		result = UI_ParseProperty(icon, property, icon->name, text, &token);
		if (!result) {
			Com_Printf("UI_ParseIcon: Parsing for sprite '%s'. See upper\n", icon->name);
			return qfalse;
		}
	}

	return qtrue;
}

/**
 * @brief Parse a component
 * @sa CL_ParseClientData
 * @code
 * component panel componentName {
 * }
 * @endcode
 */
qboolean UI_ParseComponent (const char *type, const char **text)
{
	const char *errhead = "UI_ParseComponent: unexpected end of file (component";
	uiNode_t *component;
	const char *token;

	if (!Q_streq(type, "component")) {
		Com_Error(ERR_FATAL, "UI_ParseComponent: \"component\" expected but \"%s\" found.\n", type);
		return qfalse;	/* never reached */
	}

	/* CL_ParseClientData read the real type as name */
	Com_UnParseLastToken();
	token = Com_Parse(text);

	component = UI_ParseNode(NULL, text, &token, errhead);
	if (!component)
		return qfalse;

	UI_InsertComponent(component);
	return qtrue;
}


/**
 * @brief Parse a window
 * @sa CL_ParseClientData
 * @code
 * window windowName {
 * }
 * @endcode
 */
qboolean UI_ParseWindow (const char *type, const char *name, const char **text)
{
	const char *errhead = "UI_ParseWindow: unexpected end of file (window";
	uiNode_t *window;
	const char *token;
	qboolean result;
	int i;

	if (!Q_streq(type, "window")) {
		Com_Error(ERR_FATAL, "UI_ParseWindow: '%s %s' is not a window node\n", type, name);
		return qfalse;	/* never reached */
	}

	if (!UI_TokenIsName(name, Com_ParsedTokenIsQuoted())) {
		Com_Printf("UI_ParseWindow: \"%s\" is not a well formed node name ([a-zA-Z_][a-zA-Z0-9_]*)\n", name);
		return qfalse;
	}
	if (UI_TokenIsReserved(name)) {
		Com_Printf("UI_ParseWindow: \"%s\" is a reserved token, we can't call a node with it (node \"%s\")\n", name, name);
		return qfalse;
	}

	/* search for windows with same name */
	for (i = 0; i < ui_global.numWindows; i++)
		if (!strncmp(name, ui_global.windows[i]->name, sizeof(ui_global.windows[i]->name)))
			break;

	if (i < ui_global.numWindows) {
		Com_Printf("UI_ParseWindow: %s \"%s\" with same name found, second ignored\n", type, name);
	}

	if (ui_global.numWindows >= UI_MAX_WINDOWS) {
		Com_Error(ERR_FATAL, "UI_ParseWindow: max windows exceeded (%i) - ignore '%s'\n", UI_MAX_WINDOWS, name);
		return qfalse;	/* never reached */
	}

	/* get window body */
	token = Com_Parse(text);

	/* does this window inherit data from another window? */
	if (Q_streq(token, "extends")) {
		uiNode_t *superWindow;
		token = Com_Parse(text);
		superWindow = UI_GetWindow(token);
		if (superWindow == NULL)
			Sys_Error("Could not get the super window \"%s\"", token);
		window = UI_CloneNode(superWindow, NULL, qtrue, name, qfalse);
		token = Com_Parse(text);
	} else {
		window = UI_AllocNode(name, type, qfalse);
		window->root = window;
	}

	UI_InsertWindow(window);

	/* parse it's body */
	result = UI_ParseNodeBody(window, text, &token, errhead);
	if (!result) {
#ifdef COMPILE_UNITTESTS
		Com_Printf("UI_ParseWindow: window \"%s\" has a bad body\n", window->name);
		return qfalse;
#else
		Com_Error(ERR_FATAL, "UI_ParseWindow: window \"%s\" has a bad body\n", window->name);
#endif
	}

	window->behaviour->loaded(window);
	return qtrue;
}

/**
 * @sa Com_MacroExpandString
 * @todo we should review this code, '*' doesn't work very well for all the needed things
 */
const char *UI_GetReferenceString (const uiNode_t* const node, const char *ref)
{
	if (!ref)
		return NULL;

	/* its a cvar */
	if (ref[0] == '*') {
		const char *token;

		/* get the reference and the name */
		token = Com_MacroExpandString(ref);
		if (token) {
			if (token[0] == '_') {
				token++;
				return _(token);
			}
			return token;
		}

		/* skip the star */
		token = ref + 1;
		if (token[0] == '\0')
			return NULL;

		if (Q_strstart(token, "binding:")) {
			/* skip prefix */
			token = token + 8;
			return Key_GetBinding(token, (cls.state != ca_active ? KEYSPACE_UI : KEYSPACE_GAME));
		} else {
			Sys_Error("UI_GetReferenceString: unknown reference");	/**< maybe this code is never used */
#if 0	/** @todo need a full rework */
			uiNode_t *refNode;
			const value_t *val;

			token = Com_Parse(&text);
			if (!text)
				return NULL;

			/* draw a reference to a node property */
			refNode = UI_GetNode(node->root, ident);
			if (!refNode)
				return NULL;

			/* get the property */
			val = UI_GetPropertyFromBehaviour(refNode->behaviour, token);
			if (!val)
				return NULL;

			/* get the string */
			return Com_ValueToStr(refNode, val->type & V_BASETYPEMASK, val->ofs);
#endif
		}

	/* translatable string */
	} else if (ref[0] == '_') {
		ref++;
		return _(ref);

	/* just a string */
	} else {
		return ref;
	}
}

float UI_GetReferenceFloat (const uiNode_t* const node, const void *ref)
{
	if (!ref)
		return 0.0;
	if (((const char *) ref)[0] == '*') {
		const char *token;
		token = (const char *) ref + 1;

		if (token[0] == '\0')
			return 0.0;

		if (Q_strstart(token, "cvar:")) {
			/* get the cvar value */
			return Cvar_GetValue(token + 5);
		} else {
			/** @todo maybe this code is never used */
			Sys_Error("UI_GetReferenceFloat: unknown reference '%s' from node '%s'",
					token, node->name);
#if 0	/** @todo need a full rework */
			uiNode_t *refNode;
			const value_t *val;

			/* draw a reference to a node property */
			refNode = UI_GetNode(node->root, ident);
			if (!refNode)
				return 0.0;

			/* get the property */
			val = UI_GetPropertyFromBehaviour(refNode->behaviour, token);
			if (!val || val->type != V_FLOAT)
				return 0.0;

			/* get the string */
			return *(float *) ((byte *) refNode + val->ofs);
#endif
		}
	} else {
		/* just get the data */
		return *(const float *) ref;
	}
}
