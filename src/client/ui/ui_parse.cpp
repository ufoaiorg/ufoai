/**
 * @file
 * @todo remove all "token" param from function and use Com_UnParseLastToken
 * @todo reduce use of uiGlobal (create global functions to add/get/... entities)
 * @todo remove Com_EParseValue and use Com_ParseValue
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "ui_node.h"
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
#include "../cl_language.h"

/** prototypes */
static bool UI_ParseProperty(void* object, const value_t* property, const char* objectName, const char** text, const char** token);
static uiAction_t* UI_ParseActionList(uiNode_t* node, const char** text, const char** token);
static uiNode_t* UI_ParseNode(uiNode_t* parent, const char** text, const char** token, const char* errhead);

/** @brief valid properties for a UI model definition */
static const value_t uiModelProperties[] = {
	{"model", V_HUNK_STRING, offsetof(uiModel_t, model), 0},
	{"need", V_NULL, 0, 0},
	{"anim", V_HUNK_STRING, offsetof(uiModel_t, anim), 0},
	{"skin", V_INT, offsetof(uiModel_t, skin), sizeof(int)},
	{"color", V_COLOR, offsetof(uiModel_t, color), sizeof(vec4_t)},
	{"tag", V_HUNK_STRING, offsetof(uiModel_t, tag), 0},
	{"parent", V_HUNK_STRING, offsetof(uiModel_t, parent), 0},

	{nullptr, V_NULL, 0, 0},
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
	"child",
	nullptr
};

static bool UI_TokenIsReserved (const char* name)
{
	char const* const* token = reservedTokens;
	while (*token) {
		if (Q_streq(*token, name))
			return true;
		token++;
	}
	return false;
}

static bool UI_TokenIsValue (const char* name, bool isQuoted)
{
	assert(name);
	if (isQuoted)
		return true;
	/* is it a number */
	if ((name[0] >= '0' && name[0] <= '9') || name[0] == '-' || name[0] == '.')
		return true;
	/* is it a var (*cvar:...) */
	if (name[0] == '*')
		return true;
	if (Q_streq(name, "true"))
		return true;
	if (Q_streq(name, "false"))
		return true;

	/* uppercase const name */
	if ((name[0] >= 'A' && name[0] <= 'Z') || name[0] == '_') {
		bool onlyUpperCase = true;
		while (*name != '\0') {
			if ((name[0] >= 'A' && name[0] <= 'Z') || name[0] == '_' || (name[0] >= '0' && name[0] <= '9')) {
				/* available chars */
			} else {
				return false;
			}
			name++;
		}
		return onlyUpperCase;
	}

	return false;
}

static bool UI_TokenIsName (const char* name, bool isQuoted)
{
	assert(name);
	if (isQuoted)
		return false;
	if ((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= 'A' && name[0] <= 'Z') || name[0] == '_') {
		bool onlyUpperCase = true;
		while (*name != '\0') {
			if (name[0] >= 'a' && name[0] <= 'z') {
				onlyUpperCase = false;
			} else if ((name[0] >= '0' && name[0] <= '9') || (name[0] >= 'A' && name[0] <= 'Z') || name[0] == '_') {
				/* available chars */
			} else {
				return false;
			}
			name++;
		}
		return !onlyUpperCase;
	}
	return false;
}

/**
 * @brief Find a value_t by name into a array of value_t
 * @param[in] propertyList Array of value_t, with null termination
 * @param[in] name Property name we search
 * @return A value_t with the requested name, else nullptr
 */
const value_t* UI_FindPropertyByName (const value_t* propertyList, const char* name)
{
	const value_t* current = propertyList;
	while (current->string != nullptr) {
		if (!Q_strcasecmp(name, current->string))
			return current;
		current++;
	}
	return nullptr;
}

/**
 * @brief Allocate a float into the UI static memory
 * @note Its not a dynamic memory allocation. Please only use it at the loading time
 * @param[in] count number of element need to allocate
 * @todo Assert out when we are not in parsing/loading stage
 */
float* UI_AllocStaticFloat (int count)
{
	float* result;
	assert(count > 0);
	result = (float*) UI_AllocHunkMemory(sizeof(float) * count, sizeof(float), false);
	if (result == nullptr)
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
	vec4_t* result;
	assert(count > 0);
	result = (vec4_t*) UI_AllocHunkMemory(sizeof(vec_t) * 4 * count, sizeof(vec_t), false);
	if (result == nullptr)
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
	result = (char*) UI_AllocHunkMemory(size, sizeof(char), false);
	if (result == nullptr)
		Com_Error(ERR_FATAL, "UI_AllocString: UI memory hunk exceeded - increase the size");
	Q_strncpyz(result, string, size);
	return result;
}

/**
 * @brief Allocate an action
 * @return An action
 */
uiAction_t* UI_AllocStaticAction (void)
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
 * @param property Type of the value to parse, if nullptr the string is not stored as string
 * @param string String value to parse
 * @return True if the action is initialized
 * @todo remove node param and catch error where we call that function
 */
bool UI_InitRawActionValue (uiAction_t* action, uiNode_t* node, const value_t* property, const char* string)
{
	if (property == nullptr) {
		action->type = EA_VALUE_STRING;
		action->d.terminal.d1.data = UI_AllocStaticString(string, 0);
		action->d.terminal.d2.integer = 0;
		return true;
	}

	if (property->type == V_UI_SPRITEREF) {
		uiSprite_t* sprite = UI_GetSpriteByName(string);
		if (sprite == nullptr) {
			Com_Printf("UI_ParseSetAction: sprite '%s' not found (%s)\n", string, UI_GetPath(node));
			return false;
		}
		action->type = EA_VALUE_RAW;
		action->d.terminal.d1.data = sprite;
		action->d.terminal.d2.integer = property->type;
		return true;
	} else {
		const int baseType = property->type & V_UI_MASK;
		if (baseType != 0 && baseType != V_UI_CVAR) {
			Com_Printf("UI_ParseRawValue: setter for property '%s' (type %d, 0x%X) is not supported (%s)\n", property->string, property->type, property->type, UI_GetPath(node));
			return false;
		}
		ui_global.curadata = (byte*) Com_AlignPtr(ui_global.curadata, (valueTypes_t) (property->type & V_BASETYPEMASK));
		action->type = EA_VALUE_RAW;
		action->d.terminal.d1.data = ui_global.curadata;
		action->d.terminal.d2.integer = property->type;
		/** @todo we should hide use of ui_global.curadata */
		ui_global.curadata += Com_EParseValue(ui_global.curadata, string, (valueTypes_t) (property->type & V_BASETYPEMASK), 0, property->size);
		return true;
	}
}

/**
 * @brief Parser for setter command
 */
static bool UI_ParseSetAction (uiNode_t* node, uiAction_t* action, const char** text, const char** token, const char* errhead)
{
	const value_t* property;
	int type;
	uiAction_t* localAction;

	assert((*token)[0] == '*');

	Com_UnParseLastToken();
	action->d.nonTerminal.left = UI_ParseExpression(text);

	type = action->d.nonTerminal.left->type;
	if (type != EA_VALUE_CVARNAME && type != EA_VALUE_CVARNAME_WITHINJECTION
		&& type != EA_VALUE_PATHPROPERTY && type != EA_VALUE_PATHPROPERTY_WITHINJECTION) {
		Com_Printf("UI_ParseSetAction: Cvar or Node property expected. Type '%i' found\n", type);
		return false;
	}

	/* must use "equal" char between name and value */
	*token = Com_EParse(text, errhead, nullptr);
	if (!*text)
		return false;
	if (!Q_streq(*token, "=")) {
		Com_Printf("UI_ParseSetAction: Assign sign '=' expected between variable and value. '%s' found in node %s.\n", *token, UI_GetPath(node));
		return false;
	}

	/* get the value */
	if (type == EA_VALUE_CVARNAME || type == EA_VALUE_CVARNAME_WITHINJECTION) {
		action->d.nonTerminal.right = UI_ParseExpression(text);
		return true;
	}

	property = (const value_t*) action->d.nonTerminal.left->d.terminal.d2.data;

	*token = Com_EParse(text, errhead, nullptr);
	if (!*text)
		return false;

	if (Q_streq(*token, "{")) {
		uiAction_t* actionList;

		if (property != nullptr && property->type != V_UI_ACTION) {
			Com_Printf("UI_ParseSetAction: Property %s@%s do not expect code block.\n", UI_GetPath(node), property->string);
			return false;
		}

		actionList = UI_ParseActionList(node, text, token);
		if (actionList == nullptr)
			return false;

		localAction = UI_AllocStaticAction();
		localAction->type = EA_VALUE_RAW;
		localAction->d.terminal.d1.data = actionList;
		localAction->d.terminal.d2.integer = V_UI_ACTION;
		action->d.nonTerminal.right = localAction;

		return true;
	}

	if (Q_streq(*token, "(")) {
		Com_UnParseLastToken();
		action->d.nonTerminal.right = UI_ParseExpression(text);
		return true;
	}

	/* @todo everything should come from UI_ParseExpression */

	if (UI_IsInjectedString(*token)) {
		localAction = UI_AllocStaticAction();
		localAction->type = EA_VALUE_STRING_WITHINJECTION;
		localAction->d.terminal.d1.data = UI_AllocStaticString(*token, 0);
		action->d.nonTerminal.right = localAction;
		return true;
	}

	localAction = UI_AllocStaticAction();
	UI_InitRawActionValue(localAction, node, property, *token);
	action->d.nonTerminal.right = localAction;
	return true;
}

/**
 * @brief Parser for call command
 */
static bool UI_ParseCallAction (uiNode_t* node, uiAction_t* action, const char** text, const char** token, const char* errhead)
{
	uiAction_t* expression;
	uiAction_t* lastParam = nullptr;
	int paramID = 0;
	expression = UI_ParseExpression(text);
	if (expression == nullptr)
		return false;

	if (expression->type != EA_VALUE_PATHNODE_WITHINJECTION && expression->type != EA_VALUE_PATHNODE && expression->type != EA_VALUE_PATHPROPERTY && expression->type != EA_VALUE_PATHPROPERTY_WITHINJECTION) {
		Com_Printf("UI_ParseCallAction: \"call\" keyword only support pathnode and pathproperty (node: %s)\n", UI_GetPath(node));
		return false;
	}

	action->d.nonTerminal.left = expression;

	/* check parameters */
	*token = Com_EParse(text, errhead, nullptr);
	if ((*token)[0] == '\0')
		return false;

	/* there is no parameters */
	if (!Q_streq(*token, "(")) {
		Com_UnParseLastToken();
		return true;
	}

	/* read parameters */
	do {
		uiAction_t* param;
		paramID++;

		/* parameter */
		param = UI_ParseExpression(text);
		if (param == nullptr) {
			Com_Printf("UI_ParseCallAction: problem with the %i parameter\n", paramID);
			return false;
		}
		if (lastParam == nullptr)
			action->d.nonTerminal.right = param;
		else
			lastParam->next = param;
		lastParam = param;

		/* separator */
		*token = Com_EParse(text, errhead, nullptr);
		if (!*token)
			return false;
		if (!Q_streq(*token, ",")) {
			if (Q_streq(*token, ")"))
				break;
			Com_UnParseLastToken();
			Com_Printf("UI_ParseCallAction: Invalidate end of 'call' after param %i\n", paramID);
			return false;
		}
	} while(true);

	return true;
}

/**
 * @brief Parse actions and return action list
 * @return The first element from a list of action
 * @sa ea_t
 * @todo need cleanup, compute action out of the final memory; reduce number of var
 */
static uiAction_t* UI_ParseActionList (uiNode_t* node, const char** text, const char** token)
{
	const char* errhead = "UI_ParseActionList: unexpected end of file (in event)";
	uiAction_t* firstAction;
	uiAction_t* lastAction;
	uiAction_t* action;

	lastAction = nullptr;
	firstAction = nullptr;

	/* prevent bad position */
	if ((*token)[0] != '{') {
		Com_Printf("UI_ParseActionList: token \"{\" expected, but \"%s\" found (in event) (node: %s)\n", *token, UI_GetPath(node));
		return nullptr;
	}

	while (true) {
		bool result;
		int type = EA_NULL;

		/* get new token */
		*token = Com_EParse(text, errhead, nullptr);
		if (!*token)
			return nullptr;

		if ((*token)[0] == '}')
			break;

		type = UI_GetActionTokenType(*token, EA_ACTION);
		/* setter form */
		if (type == EA_NULL && (*token)[0] == '*')
			type = EA_ASSIGN;

		/* unknown, we break the parsing */
		if (type == EA_NULL) {
			Com_Printf("UI_ParseActionList: unknown token \"%s\" ignored (in event) (node: %s)\n", *token, UI_GetPath(node));
			return nullptr;
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
			*token = Com_EParse(text, errhead, nullptr);
			if (!*text)
				return nullptr;

			/* get the value */
			action->d.terminal.d1.constString = UI_AllocStaticString(*token, 0);
			break;

		case EA_ASSIGN:
			result = UI_ParseSetAction(node, action, text, token, errhead);
			if (!result)
				return nullptr;
			break;

		case EA_CALL:
			result = UI_ParseCallAction(node, action, text, token, errhead);
			if (!result)
				return nullptr;
			break;

		case EA_DELETE:
			{
				uiAction_t* expression;
				expression = UI_ParseExpression(text);
				if (expression == nullptr)
					return nullptr;

				if (expression->type != EA_VALUE_CVARNAME) {
					Com_Printf("UI_ParseActionList: \"delete\" keyword only support cvarname (node: %s)\n", UI_GetPath(node));
					return nullptr;
				}

				action->d.nonTerminal.left = expression;
				break;
			}

		case EA_ELIF:
			/* check previous action */
			if (!lastAction || (lastAction->type != EA_IF && lastAction->type != EA_ELIF)) {
				Com_Printf("UI_ParseActionList: 'elif' must be set after an 'if' or an 'elif' (node: %s)\n", UI_GetPath(node));
				return nullptr;
			}
			/* then it execute EA_IF, no break */
		case EA_WHILE:
		case EA_IF:
			{
				uiAction_t* expression;

				/* get the condition */
				expression = UI_ParseExpression(text);
				if (expression == nullptr)
					return nullptr;
				action->d.nonTerminal.left = expression;

				/* get the action block */
				*token = Com_EParse(text, errhead, nullptr);
				if (!*text)
					return nullptr;
				action->d.nonTerminal.right = UI_ParseActionList(node, text, token);
				if (action->d.nonTerminal.right == nullptr) {
					switch (action->type) {
					case EA_IF:
						Com_Printf("UI_ParseActionList: block expected after \"if\" (node: %s)\n", UI_GetPath(node));
						break;
					case EA_ELIF:
						Com_Printf("UI_ParseActionList: block expected after \"elif\" (node: %s)\n", UI_GetPath(node));
						break;
					case EA_WHILE:
						Com_Printf("UI_ParseActionList: block expected after \"while\" (node: %s)\n", UI_GetPath(node));
						break;
					default:
						Com_Printf("UI_ParseActionList: cannot determine statement type (node: %s)\n", UI_GetPath(node));
					}
					return nullptr;
				}
				break;
			}

		case EA_FORCHILDIN:
			{
				uiAction_t* expression;

				/* get the node */
				expression = UI_ParseExpression(text);
				if (expression == nullptr) {
					return nullptr;
				}
				action->d.nonTerminal.left = expression;

				/* check the type */
				type = action->d.nonTerminal.left->type;
				if (type != EA_VALUE_PATHNODE && type != EA_VALUE_PATHNODE_WITHINJECTION) {
					Com_Printf("UI_ParseActionList: Node property expected. Type '%x' found\n", type);
					return nullptr;
				}

				/* get the action block */
				*token = Com_EParse(text, errhead, nullptr);
				if (!*text)
					return nullptr;
				action->d.nonTerminal.right = UI_ParseActionList(node, text, token);
				if (action->d.nonTerminal.right == nullptr) {
					Com_Printf("UI_ParseActionList: block expected after \"forchildin\" (node: %s)\n", UI_GetPath(node));
					return nullptr;
				}
				break;
			}

		case EA_ELSE:
			/* check previous action */
			if (!lastAction || (lastAction->type != EA_IF && lastAction->type != EA_ELIF)) {
				Com_Printf("UI_ParseActionList: 'else' must be set after an 'if' or an 'elif' (node: %s)\n", UI_GetPath(node));
				return nullptr;
			}

			/* get the action block */
			*token = Com_EParse(text, errhead, nullptr);
			if (!*text)
				return nullptr;
			action->d.nonTerminal.left = nullptr;
			action->d.nonTerminal.right = UI_ParseActionList(node, text, token);
			if (action->d.nonTerminal.right == nullptr) {
				Com_Printf("UI_ParseActionList: block expected after \"else\" (node: %s)\n", UI_GetPath(node));
				return nullptr;
			}
			break;

		default:
			assert(false);
		}

		/* step */
		lastAction = action;
	}

	assert((*token)[0] == '}');

	/* return non nullptr value */
	if (firstAction == nullptr) {
		firstAction = UI_AllocStaticAction();
	}

	return firstAction;
}

static bool UI_ParseExcludeRect (uiNode_t* node, const char** text, const char** token, const char* errhead)
{
	uiExcludeRect_t rect;
	uiExcludeRect_t* newRect;

	/* get parameters */
	*token = Com_EParse(text, errhead, node->name);
	if (!*text)
		return false;
	if ((*token)[0] != '{') {
		Com_Printf("UI_ParseExcludeRect: node with bad excluderect ignored (node \"%s\")\n", UI_GetPath(node));
		return true;
	}

	do {
		*token = Com_EParse(text, errhead, node->name);
		if (!*text)
			return false;
		/** @todo move it into a property array */
		if (Q_streq(*token, "pos")) {
			*token = Com_EParse(text, errhead, node->name);
			if (!*text)
				return false;
			Com_EParseValue(&rect, *token, V_POS, offsetof(uiExcludeRect_t, pos), sizeof(vec2_t));
		} else if (Q_streq(*token, "size")) {
			*token = Com_EParse(text, errhead, node->name);
			if (!*text)
				return false;
			Com_EParseValue(&rect, *token, V_POS, offsetof(uiExcludeRect_t, size), sizeof(vec2_t));
		}
	} while ((*token)[0] != '}');

	newRect = (uiExcludeRect_t*) UI_AllocHunkMemory(sizeof(*newRect), STRUCT_MEMORY_ALIGN, false);
	if (newRect == nullptr) {
		Com_Printf("UI_ParseExcludeRect: ui hunk memory exceeded.");
		return false;
	}

	/* move data to final memory and link to node */
	*newRect = rect;
	newRect->next = node->firstExcludeRect;
	node->firstExcludeRect = newRect;
	return true;
}

static bool UI_ParseEventProperty (uiNode_t* node, const value_t* event, const char** text, const char** token, const char* errhead)
{
	/* add new actions to end of list */
	uiAction_t** action = &Com_GetValue<uiAction_t*>(node, event);
	for (; *action; action = &(*action)->next) {}

	/* get the action body */
	*token = Com_EParse(text, errhead, node->name);
	if (!*text)
		return false;

	if ((*token)[0] != '{') {
		Com_Printf("UI_ParseEventProperty: Event '%s' without body (%s)\n", event->string, UI_GetPath(node));
		return false;
	}

	*action = UI_ParseActionList(node, text, token);
	if (*action == nullptr)
		return false;

	/* block terminal already read */
	assert((*token)[0] == '}');

	return true;
}

/**
 * @brief Parse a property value
 * @todo don't read the next token (need to change the script language)
 */
static bool UI_ParseProperty (void* object, const value_t* property, const char* objectName, const char** text, const char** token)
{
	const char* errhead = "UI_ParseProperty: unexpected end of file (object";
	static const char* notWellFormedValue = "UI_ParseProperty: \"%s\" is not a well formed node name (it must be quoted, uppercase const, a number, or prefixed with '*')\n";
	size_t bytes;
	int result;
	const int specialType = property->type & V_UI_MASK;

	if (property->type == V_NULL) {
		return false;
	}

	switch (specialType) {
	case V_NOT_UI:	/* common type */

		*token = Com_EParse(text, errhead, objectName);
		if (!*text)
			return false;
		if (!UI_TokenIsValue(*token, Com_GetType(text) == TT_QUOTED_WORD)) {
			Com_Printf(notWellFormedValue, *token);
			return false;
		}

		if (property->type == V_TRANSLATION_STRING) {
			/* selectbox values are static arrays */
			char* const target = Com_GetValue<char[]>(object, property);
			const char* translatableToken = *token;
			assert(property->size);
			if (translatableToken[0] == '_')
				translatableToken++;
			Q_strncpyz(target, translatableToken, property->size);
		} else {
			result = Com_ParseValue(object, *token, property->type, property->ofs, property->size, &bytes);
			if (result != RESULT_OK) {
				Com_Printf("UI_ParseProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
				return false;
			}
		}
		break;

	case V_UI_REF:
		*token = Com_EParse(text, errhead, objectName);
		if (!*text)
			return false;
		if (!UI_TokenIsValue(*token, Com_GetType(text) == TT_QUOTED_WORD)) {
			Com_Printf(notWellFormedValue, *token);
			return false;
		}

		/* a reference to data is handled like this */
		ui_global.curadata = (byte*) Com_AlignPtr(ui_global.curadata, (valueTypes_t) (property->type & V_BASETYPEMASK));
		Com_GetValue<byte*>(object, property) = ui_global.curadata;

		/** @todo check for the moment its not a cvar */
		assert((*token)[0] != '*');

		/* sanity check */
		if ((property->type & V_BASETYPEMASK) == V_STRING && strlen(*token) > MAX_VAR - 1) {
			Com_Printf("UI_ParseProperty: Value '%s' is too long (key %s)\n", *token, property->string);
			return false;
		}

		result = Com_ParseValue(ui_global.curadata, *token, (valueTypes_t) (property->type & V_BASETYPEMASK), 0, property->size, &bytes);
		if (result != RESULT_OK) {
			Com_Printf("UI_ParseProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
			return false;
		}
		ui_global.curadata += bytes;

		break;

	case V_UI_CVAR:	/* common type */
		*token = Com_EParse(text, errhead, objectName);
		if (!*text)
			return false;
		if (!UI_TokenIsValue(*token, Com_GetType(text) == TT_QUOTED_WORD)) {
			Com_Printf(notWellFormedValue, *token);
			return false;
		}

		/* references are parsed as string */
		if ((*token)[0] == '*') {
			/* a reference to data */
			ui_global.curadata = (byte*) Com_AlignPtr(ui_global.curadata, V_STRING);
			Com_GetValue<byte*>(object, property) = ui_global.curadata;

			/* sanity check */
			if (strlen(*token) > MAX_VAR - 1) {
				Com_Printf("UI_ParseProperty: Value '%s' is too long (key %s)\n", *token, property->string);
				return false;
			}

			result = Com_ParseValue(ui_global.curadata, *token, V_STRING, 0, 0, &bytes);
			if (result != RESULT_OK) {
				Com_Printf("UI_ParseProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
				return false;
			}
			ui_global.curadata += bytes;
		} else {
			/* a reference to data */
			ui_global.curadata = (byte*) Com_AlignPtr(ui_global.curadata, (valueTypes_t)(property->type & V_BASETYPEMASK));
			Com_GetValue<byte*>(object, property) = ui_global.curadata;

			/* sanity check */
			if ((property->type & V_BASETYPEMASK) == V_STRING && strlen(*token) > MAX_VAR - 1) {
				Com_Printf("UI_ParseProperty: Value '%s' is too long (key %s)\n", *token, property->string);
				return false;
			}

			result = Com_ParseValue(ui_global.curadata, *token, (valueTypes_t)(property->type & V_BASETYPEMASK), 0, property->size, &bytes);
			if (result != RESULT_OK) {
				Com_Printf("UI_ParseProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
				return false;
			}
			ui_global.curadata += bytes;
		}
		break;

	case V_UI:

		switch ((int)property->type) {
		case V_UI_ACTION:
			result = UI_ParseEventProperty(static_cast<uiNode_t*>(object), property, text, token, errhead);
			if (!result)
				return false;
			break;

		case V_UI_EXCLUDERECT:
			result = UI_ParseExcludeRect(static_cast<uiNode_t*>(object), text, token, errhead);
			if (!result)
				return false;
			break;

		case V_UI_SPRITEREF:
			{
				*token = Com_EParse(text, errhead, objectName);
				if (!*text)
					return false;

				uiSprite_t const*& sprite = Com_GetValue<uiSprite_t const*>(object, property);
				sprite = UI_GetSpriteByName(*token);
				if (!sprite) {
					Com_Printf("UI_ParseProperty: sprite '%s' not found (object %s)\n", *token, objectName);
				}
			}
			break;

		case V_UI_IF:
			{
				*token = Com_EParse(text, errhead, objectName);
				if (!*text)
					return false;

				uiAction_t*& expression = Com_GetValue<uiAction_t*>(object, property);
				expression = UI_AllocStaticStringCondition(*token);
				if (!expression)
					return false;
			}
			break;

		case V_UI_DATAID:
			{
				*token = Com_EParse(text, errhead, objectName);
				if (!*text)
					return false;

				int& dataId = Com_GetValue<int>(object, property);
				dataId = UI_GetDataIDByName(*token);
				if (dataId < 0) {
					Com_Printf("UI_ParseProperty: Could not find shared data ID '%s' (%s@%s)\n",
							*token, objectName, property->string);
					return false;
				}
			}
			break;

		default:
			Com_Printf("UI_ParseProperty: unknown property type '%d' (0x%X) (%s@%s)\n",
					property->type, property->type, objectName, property->string);
			return false;
		}
		break;

	default:
		Com_Printf("UI_ParseProperties: unknown property type '%d' (0x%X) (%s@%s)\n",
				property->type, property->type, objectName, property->string);
		return false;
	}

	return true;
}

static bool UI_ParseFunction (uiNode_t* node, const char** text, const char** token)
{
	uiAction_t** action;
	assert(UI_Node_IsFunction(node));

	action = &node->onClick;
	*action = UI_ParseActionList(node, text, token);
	if (*action == nullptr)
		return false;

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
static bool UI_ParseNodeProperties (uiNode_t* node, const char** text, const char** token)
{
	const char* errhead = "UI_ParseNodeProperties: unexpected end of file (node";
	bool nextTokenAlreadyRead = false;

	if ((*token)[0] != '{')
		nextTokenAlreadyRead = true;

	do {
		const value_t* val;
		int result;

		/* get new token */
		if (!nextTokenAlreadyRead) {
			*token = Com_EParse(text, errhead, node->name);
			if (!*text)
				return false;
		} else {
			nextTokenAlreadyRead = false;
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
			return false;
		}

		/* get parameter values */
		result = UI_ParseProperty(node, val, node->name, text, token);
		if (!result) {
			Com_Printf("UI_ParseNodeProperties: Problem with parsing of node property '%s@%s'. See upper\n",
					UI_GetPath(node), val->string);
			return false;
		}
	} while (*text);

	return true;
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
static bool UI_ParseNodeBody (uiNode_t* node, const char** text, const char** token, const char* errhead)
{
	bool result = true;

	if ((*token)[0] != '{') {
		/* read the body block start */
		*token = Com_EParse(text, errhead, node->name);
		if (!*text)
			return false;
		if ((*token)[0] != '{') {
			Com_Printf("UI_ParseNodeBody: node doesn't have body, token '%s' read (node \"%s\")\n", *token, UI_GetPath(node));
			ui_global.numNodes--;
			return false;
		}
	}

	/* functions are a special case */
	if (UI_Node_IsFunction(node)) {
		result = UI_ParseFunction(node, text, token);
	} else {

		/* check the content */
		*token = Com_EParse(text, errhead, node->name);
		if (!*text)
			return false;

		if ((*token)[0] == '{') {
			/* we have a special block for properties */
			result = UI_ParseNodeProperties(node, text, token);
			if (!result)
				return false;

			/* move token over the next node behaviour */
			*token = Com_EParse(text, errhead, node->name);
			if (!*text)
				return false;

			/* and then read all nodes */
			while ((*token)[0] != '}') {
				uiNode_t* newNode = UI_ParseNode(node, text, token, errhead);
				if (!newNode)
					return false;

				*token = Com_EParse(text, errhead, node->name);
				if (*text == nullptr)
					return false;
			}
		} else if (UI_GetPropertyFromBehaviour(node->behaviour, *token)) {
			/* we should have a block with properties only */
			result = UI_ParseNodeProperties(node, text, token);
		} else {
			/* we should have a block with nodes only */
			while ((*token)[0] != '}') {
				uiNode_t* newNode = UI_ParseNode(node, text, token, errhead);
				if (!newNode)
					return false;

				*token = Com_EParse(text, errhead, node->name);
				if (*text == nullptr)
					return false;
			}
		}
	}
	if (!result) {
		Com_Printf("UI_ParseNodeBody: node with bad body ignored (node \"%s\")\n", UI_GetPath(node));
		ui_global.numNodes--;
		return false;
	}

	/* already check on UI_ParseNodeProperties */
	assert((*token)[0] == '}');
	return true;
}

/**
 * @brief parse a node
 * @sa UI_ParseNodeProperties
 * @todo we can think about merging UI_ParseNodeProperties here
 * @note first token already read
 * @note dont read more than the need token (last right token is '}' of end of node)
 */
static uiNode_t* UI_ParseNode (uiNode_t* parent, const char** text, const char** token, const char* errhead)
{
	uiNode_t* node = nullptr;
	uiBehaviour_t* behaviour;
	uiNode_t* component = nullptr;

	/* get the behaviour */
	behaviour = UI_GetNodeBehaviour(*token);
	if (!behaviour) {
		component = UI_GetComponent(*token);
	}
	if (behaviour == nullptr && component == nullptr) {
		Com_Printf("UI_ParseNode: node behaviour/component '%s' doesn't exist (%s)\n", *token, UI_GetPath(parent));
		return nullptr;
	}

	/* get the name */
	*token = Com_EParse(text, errhead, "");
	if (!*text)
		return nullptr;
	if (!UI_TokenIsName(*token, Com_GetType(text) == TT_QUOTED_WORD)) {
		Com_Printf("UI_ParseNode: \"%s\" is not a well formed node name ([a-zA-Z_][a-zA-Z0-9_]*)\n", *token);
		return nullptr;
	}
	if (UI_TokenIsReserved(*token)) {
		Com_Printf("UI_ParseNode: \"%s\" is a reserved token, we can't call a node with it\n", *token);
		return nullptr;
	}

	/* test if node already exists */
	/* Already existing node should only come from inherited node, we should not have 2 definitions of the same node into the same window. */
	if (parent)
		node = UI_GetNode(parent, *token);

	/* reuse a node */
	if (node) {
		const uiBehaviour_t* test = (behaviour != nullptr) ? behaviour : (component != nullptr) ? component->behaviour : nullptr;
		if (node->behaviour != test) {
			Com_Printf("UI_ParseNode: we can't change node type (node \"%s\")\n", UI_GetPath(node));
			return nullptr;
		}
		Com_DPrintf(DEBUG_CLIENT, "... over-riding node %s\n", UI_GetPath(node));

	/* else initialize a component */
	} else if (component) {
		node = UI_CloneNode(component, nullptr, true, *token, false);
		if (parent) {
			if (parent->root)
				UI_UpdateRoot(node, parent->root);
			UI_AppendNode(parent, node);
		}

	/* else initialize a new node */
	} else {
		node = UI_AllocNode(*token, behaviour->name, false);
		node->parent = parent;
		if (parent)
			node->root = parent->root;
		/** @todo move it into caller */
		if (parent)
			UI_AppendNode(parent, node);
	}

	/* get body */
	const bool result = UI_ParseNodeBody(node, text, token, errhead);
	if (!result)
		return nullptr;

	/* validate properties */
	UI_Node_Loaded(node);

	return node;
}

/**
 * @brief parses the models.ufo and all files where UI models (menu_model) are defined
 * @sa CL_ParseClientData
 */
bool UI_ParseUIModel (const char* name, const char** text)
{
	const char* errhead = "UI_ParseUIModel: unexpected end of file (names ";

	/* search for a UI models with same name */
	for (int i = 0; i < ui_global.numModels; i++)
		if (Q_streq(ui_global.models[i].id, name)) {
			Com_Printf("UI_ParseUIModel: menu_model \"%s\" with same name found, second ignored\n", name);
			return false;
		}

	if (ui_global.numModels >= UI_MAX_MODELS) {
		Com_Printf("UI_ParseUIModel: Max UI models reached\n");
		return false;
	}

	/* initialize the model */
	uiModel_t* model = &ui_global.models[ui_global.numModels];
	OBJZERO(*model);

	Vector4Set(model->color, 1, 1, 1, 1);

	model->id = Mem_PoolStrDup(name, ui_sysPool, 0);
	Com_DPrintf(DEBUG_CLIENT, "Found UI model %s (%i)\n", model->id, ui_global.numModels);

	/* get it's body */
	const char* token = Com_Parse(text);

	if (!*text || token[0] != '{') {
		Com_Printf("UI_ParseUIModel: Model \"%s\" without body ignored\n", model->id);
		return false;
	}

	ui_global.numModels++;

	do {
		const value_t* v = nullptr;
		/* get the name type */
		token = Com_EParse(text, errhead, name);
		if (!*text)
			return false;
		if (token[0] == '}')
			break;

		v = UI_FindPropertyByName(uiModelProperties, token);
		if (!v) {
			Com_Printf("UI_ParseUIModel: unknown token \"%s\" ignored (UI model %s)\n", token, name);
			return false;
		}

		if (v->type == V_NULL) {
			if (Q_streq(v->string, "need")) {
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return false;
				if (model->next != nullptr)
					Sys_Error("UI_ParseUIModel: second 'need' token found in model %s", name);
				model->next = UI_GetUIModel(token);
				 if (!model->next)
					Com_Printf("Could not find UI model %s", token);
			}
		} else {
			token = Com_EParse(text, errhead, name);
			if (!*text)
				return false;
			switch (v->type) {
			case V_HUNK_STRING:
				Mem_PoolStrDupTo(token, &Com_GetValue<char*>(model, v), ui_sysPool, 0);
				break;
			default:
				Com_EParseValue(model, token, v->type, v->ofs, v->size);
				break;
			}
		}
	} while (*text);

	return true;
}

bool UI_ParseSprite (const char* name, const char** text)
{
	uiSprite_t* icon;
	const char* token;

	/* search for icons with same name */
	icon = UI_AllocStaticSprite(name);

	/* get it's body */
	token = Com_Parse(text);
	assert(token[0] == '{');

	/* read properties */
	while (true) {
		const value_t* property;

		token = Com_Parse(text);
		if (*text == nullptr)
			return false;

		if (token[0] == '}')
			break;

		property = UI_FindPropertyByName(ui_spriteProperties, token);
		if (!property) {
			Com_Printf("UI_ParseIcon: unknown options property: '%s' - ignore it\n", token);
			return false;
		}

		/* get parameter values */
		const bool result = UI_ParseProperty(icon, property, icon->name, text, &token);
		if (!result) {
			Com_Printf("UI_ParseIcon: Parsing for sprite '%s'. See upper\n", icon->name);
			return false;
		}
	}

	return true;
}

/**
 * @brief Parse a component
 * @sa CL_ParseClientData
 * @code
 * component panel componentName {
 * }
 * @endcode
 */
bool UI_ParseComponent (const char* type, const char* name, const char** text)
{
	const char* errhead = "UI_ParseComponent: unexpected end of file (component";
	const char* token;

	if (!Q_streq(type, "component")) {
		Com_Error(ERR_FATAL, "UI_ParseComponent: \"component\" expected but \"%s\" found.\n", type);
		return false;	/* never reached */
	}

	/* check the name */
	if (!UI_TokenIsName(name, false)) {
		Com_Printf("UI_ParseNode: \"%s\" is not a well formed node name ([a-zA-Z_][a-zA-Z0-9_]*)\n", name);
		return false;
	}
	if (UI_TokenIsReserved(name)) {
		Com_Printf("UI_ParseNode: \"%s\" is a reserved token, we can't call a node with it\n", name);
		return false;
	}

	token = Com_EParse(text, errhead, "");
	if (text == nullptr)
		return false;

	/* get keyword */
	if (!Q_streq(token, "extends")) {
		Com_Printf("UI_ParseComponent: \"extends\" expected but \"%s\" found (component %s)\n", token, name);
		return false;
	}
	token = Com_EParse(text, errhead, "");
	if (text == nullptr)
		return false;

	/* initialize component */
	uiNode_t* component = nullptr;
	const uiBehaviour_t* behaviour = UI_GetNodeBehaviour(token);
	if (behaviour) {
		/* initialize a new node from behaviour */
		component = UI_AllocNode(name, behaviour->name, false);
	} else {
		const uiNode_t* inheritedComponent = UI_GetComponent(token);
		if (inheritedComponent) {
			/* initialize from a component */
			component = UI_CloneNode(inheritedComponent, nullptr, true, name, false);
		} else {
			Com_Printf("UI_ParseComponent: node behaviour/component '%s' doesn't exists (component %s)\n", token, name);
			return false;
		}
	}

	/* get body */
	token = Com_EParse(text, errhead, "");
	if (!*text)
		return false;
	bool result = UI_ParseNodeBody(component, text, &token, errhead);
	if (!result)
		return false;

	/* validate properties */
	UI_Node_Loaded(component);

	UI_InsertComponent(component);
	return true;
}


/**
 * @brief Parse a window
 * @sa CL_ParseClientData
 * @code
 * window windowName {
 * }
 * @endcode
 */
bool UI_ParseWindow (const char* type, const char* name, const char** text)
{
	const char* errhead = "UI_ParseWindow: unexpected end of file (window";
	uiNode_t* window;
	const char* token;
	int i;

	if (!Q_streq(type, "window")) {
		Com_Error(ERR_FATAL, "UI_ParseWindow: '%s %s' is not a window node\n", type, name);
		return false;	/* never reached */
	}

	if (!UI_TokenIsName(name, Com_GetType(text) == TT_QUOTED_WORD)) {
		Com_Printf("UI_ParseWindow: \"%s\" is not a well formed node name ([a-zA-Z_][a-zA-Z0-9_]*)\n", name);
		return false;
	}
	if (UI_TokenIsReserved(name)) {
		Com_Printf("UI_ParseWindow: \"%s\" is a reserved token, we can't call a node with it (node \"%s\")\n", name, name);
		return false;
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
		return false;	/* never reached */
	}

	/* get window body */
	token = Com_Parse(text);

	/* does this window inherit data from another window? */
	if (Q_streq(token, "extends")) {
		uiNode_t* superWindow;
		token = Com_Parse(text);
		superWindow = UI_GetWindow(token);
		if (superWindow == nullptr)
			Sys_Error("Could not get the super window \"%s\"", token);
		window = UI_CloneNode(superWindow, nullptr, true, name, false);
		token = Com_Parse(text);
	} else {
		window = UI_AllocNode(name, type, false);
		window->root = window;
	}

	UI_InsertWindow(window);

	/* parse it's body */
	bool result = UI_ParseNodeBody(window, text, &token, errhead);
	if (!result) {
		Com_Error(ERR_FATAL, "UI_ParseWindow: window \"%s\" has a bad body\n", window->name);
	}

	UI_Node_Loaded(window);
	return true;
}

/**
 * @sa Com_MacroExpandString
 * @todo we should review this code, '*' doesn't work very well for all the needed things
 */
const char* UI_GetReferenceString (const uiNode_t* const node, const char* ref)
{
	if (!ref)
		return nullptr;

	/* its a cvar */
	if (ref[0] != '*')
		return CL_Translate(ref);

	/* get the reference and the name */
	const char* token = Com_MacroExpandString(ref);
	if (token)
		return CL_Translate(token);

	/* skip the star */
	token = ref + 1;
	if (token[0] == '\0')
		return nullptr;

	Sys_Error("UI_GetReferenceString: unknown reference %s", token);
}

float UI_GetReferenceFloat (const uiNode_t* const node, const void* ref)
{
	if (!ref)
		return 0.0;
	if (char const* const token = Q_strstart((char const*)ref, "*")) {
		if (token[0] == '\0')
			return 0.0;

		if (char const* const cvar = Q_strstart(token, "cvar:")) {
			return Cvar_GetValue(cvar);
		}

		Sys_Error("UI_GetReferenceFloat: unknown reference '%s' from node '%s'", token, node->name);
	}

	/* just get the data */
	return *(const float*) ref;
}
