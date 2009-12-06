/**
 * @file m_parse.c
 * @todo remove all "token" param from function and use Com_UnParseLastToken
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

#include "../client.h"
#include "m_parse.h"
#include "m_main.h"
#include "m_data.h"
#include "m_internal.h"
#include "m_actions.h"
#include "m_icon.h"
#include "m_components.h"
#include "node/m_node_window.h"
#include "node/m_node_selectbox.h"
#include "node/m_node_abstractnode.h"

#include "../../shared/parse.h"

/** prototypes */
static qboolean MN_ParseProperty(void* object, const value_t *property, const char* objectName, const char **text, const char **token);
static menuAction_t *MN_ParseActionList(menuNode_t *menuNode, const char **text, const const char **token);
static menuNode_t *MN_ParseNode(menuNode_t * parent, const char **text, const char **token, const char *errhead);

/** @brief valid properties for options (selectbox, tab...) */
static const value_t optionProperties[] = {
	{"label", V_TRANSLATION_STRING, offsetof(menuOption_t, label), sizeof(char) * OPTION_MAX_VALUE_LENGTH},
	{"value", V_STRING, offsetof(menuOption_t, value), 0},
	{"icon", V_UI_ICONREF, offsetof(menuOption_t, icon), 0},
	{"disabled", V_BOOL, offsetof(menuOption_t, disabled), MEMBER_SIZEOF(menuOption_t, disabled)},
	{"invis", V_BOOL, offsetof(menuOption_t, invis), MEMBER_SIZEOF(menuOption_t, invis)},

	{NULL, V_NULL, 0, 0},
};

/** @brief valid properties for a menu model definition */
static const value_t menuModelProperties[] = {
	{"model", V_CLIENT_HUNK_STRING, offsetof(menuModel_t, model), 0},
	{"need", V_NULL, 0, 0},
	{"menutransform", V_NULL, 0, 0},
	{"anim", V_CLIENT_HUNK_STRING, offsetof(menuModel_t, anim), 0},
	{"skin", V_INT, offsetof(menuModel_t, skin), sizeof(int)},
	{"origin", V_VECTOR, offsetof(menuModel_t, origin), sizeof(vec3_t)},
	{"center", V_VECTOR, offsetof(menuModel_t, center), sizeof(vec3_t)},
	{"scale", V_VECTOR, offsetof(menuModel_t, scale), sizeof(vec3_t)},
	{"angles", V_VECTOR, offsetof(menuModel_t, angles), sizeof(vec3_t)},
	{"color", V_COLOR, offsetof(menuModel_t, color), sizeof(vec4_t)},
	{"tag", V_CLIENT_HUNK_STRING, offsetof(menuModel_t, tag), 0},
	{"parent", V_CLIENT_HUNK_STRING, offsetof(menuModel_t, parent), 0},

	{NULL, V_NULL, 0, 0},
};

/** @brief reserved token preventing calling a node with it */
static const char *reserved_tokens[] = {
	"this",
	"parent",
	"root",
	NULL
};

static qboolean MN_IsReservedToken (const char *name)
{
	const char **token = reserved_tokens;
	while (*token) {
		if (!strcmp(*token, name))
			return qtrue;
		token++;
	}
	return qfalse;
}

/**
 * @brief Find a value_t by name into a array of value_t
 * @param[in] propertyList Array of value_t, with null termination
 * @param[in] name Property name we search
 * @return A value_t with the requested name, else NULL
 */
const value_t* MN_FindPropertyByName (const value_t* propertyList, const char* name)
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
 * @brief Allocate a float into the menu memory
 * @note Its not a dynamic memory allocation. Please only use it at the loading time
 * @param[in] count number of element need to allocate
 * @todo Assert out when we are not in parsing/loading stage
 */
float* MN_AllocStaticFloat (int count)
{
	float *result;
	assert(count > 0);
	mn.curadata = ALIGN_PTR(mn.curadata, sizeof(float));
	result = (float*) mn.curadata;
	mn.curadata += sizeof(float) * count;
	if (mn.curadata - mn.adata > mn.adataize)
		Com_Error(ERR_FATAL, "MN_AllocFloat: Menu memory hunk exceeded - increase the size");
	return result;
}

/**
 * @brief Allocate a color into the menu memory
 * @note Its not a dynamic memory allocation. Please only use it at the loading time
 * @param[in] count number of element need to allocate
 * @todo Assert out when we are not in parsing/loading stage
 */
vec4_t* MN_AllocStaticColor (int count)
{
	vec4_t *result;
	assert(count > 0);
	mn.curadata = ALIGN_PTR(mn.curadata, sizeof(vec_t));
	result = (vec4_t*) mn.curadata;
	mn.curadata += sizeof(vec_t) * 4 * count;
	if (mn.curadata - mn.adata > mn.adataize)
		Com_Error(ERR_FATAL, "MN_AllocColor: Menu memory hunk exceeded - increase the size");
	return result;
}

/**
 * @brief Allocate a string into the menu memory
 * @note Its not a dynamic memory allocation. Please only use it at the loading time
 * @param[in] string Use to initialize the string
 * @param[in] size request a fixed memory size, if 0 the string size is used
 * @todo Assert out when we are not in parsing/loading stage
 */
char* MN_AllocStaticString (const char* string, int size)
{
	char* result = (char *)mn.curadata;
	mn.curadata = ALIGN_PTR(mn.curadata, sizeof(char));
	if (size != 0) {
		if (mn.curadata - mn.adata + size > mn.adataize)
			Com_Error(ERR_FATAL, "MN_AllocString: Menu memory hunk exceeded - increase the size");
		strncpy((char *)mn.curadata, string, size);
		mn.curadata += size;
	} else {
		if (mn.curadata - mn.adata + strlen(string) + 1 > mn.adataize)
			Com_Error(ERR_FATAL, "MN_AllocString: Menu memory hunk exceeded - increase the size");
		mn.curadata += sprintf((char *)mn.curadata, "%s", string) + 1;
	}
	return result;
}

/**
 * @brief Allocate an action
 * @return An action
 */
menuAction_t *MN_AllocStaticAction (void)
{
	if (mn.numActions >= MAX_MENUACTIONS)
		Com_Error(ERR_FATAL, "MN_AllocAction: Too many menu actions");
	return &mn.actions[mn.numActions++];
}

/**
 * @brief Parser for setter command
 * @todo Clean up the code after merge of setter without @
 */
static qboolean MN_ParseSetAction (menuNode_t *menuNode, menuAction_t *action, const char **text, const char **token, const char *errhead)
{
	const value_t *property;
	int type;

	assert(*token[0] == '*');

	Com_UnParseLastToken();
	action->d.nonTerminal.left = MN_ParseExpression(text, errhead, menuNode);

	type = action->d.nonTerminal.left->type;
	if (type != EA_VALUE_CVARNAME && type != EA_VALUE_CVARNAME_WITHINJECTION
		&& type != EA_VALUE_PATHPROPERTY && type != EA_VALUE_PATHPROPERTY_WITHINJECTION) {
		Com_Printf("MN_ParseSetAction: Cvar or Node property expected. Type '%i' found\n", type);
		return qfalse;
	}

	/* allow to use "equal" char between name and value */
	*token = Com_EParse(text, errhead, NULL);
	if (!*text)
		return qfalse;
	if (strcmp(*token, "=") != 0)
		Com_UnParseLastToken();

	/* get the value */
	if (type == EA_VALUE_CVARNAME || type == EA_VALUE_CVARNAME_WITHINJECTION) {
		action->d.nonTerminal.right = MN_ParseExpression(text, errhead, menuNode);
		return qtrue;
	}

	property = (const value_t *) action->d.nonTerminal.left->d.terminal.d2.data;
	if (property == NULL) {
		Com_Printf("MN_ParseSetAction: Typed node property expected. Cast the left operand.\n");
		return qfalse;
	}

	*token = Com_EParse(text, errhead, NULL);
	if (!*text)
		return qfalse;

	if (!strcmp(*token, "(")) {
		Com_UnParseLastToken();
		action->d.nonTerminal.right = MN_ParseExpression(text, errhead, menuNode);
		return qtrue;
	}

	if (property->type == V_UI_ACTION) {
		menuAction_t* actionList;
		menuAction_t*a;
		actionList = MN_ParseActionList(menuNode, text, token);
		if (actionList == NULL)
			return qfalse;

		a = MN_AllocStaticAction();
		a->type = EA_VALUE_RAW;
		a->d.terminal.d1.data = actionList;
		a->d.terminal.d2.constData = property;
		action->d.nonTerminal.right = a;
	} else if (property->type == V_UI_ICONREF) {
		menuIcon_t* icon = MN_GetIconByName(*token);
		menuAction_t*a;
		if (icon == NULL) {
			Com_Printf("MN_ParseSetAction: icon '%s' not found (%s)\n", *token, MN_GetPath(menuNode));
			return qfalse;
		}
		a = MN_AllocStaticAction();
		a->type = EA_VALUE_RAW;
		a->d.terminal.d1.data = icon;
		a->d.terminal.d2.constData = property;
		action->d.nonTerminal.right = a;
	} else {
		if (MN_IsInjectedString(*token)) {
			menuAction_t *a;
			a = MN_AllocStaticAction();
			a->type = EA_VALUE_STRING_WITHINJECTION;
			a->d.terminal.d1.data = MN_AllocStaticString(*token, 0);
			action->d.nonTerminal.right = a;
		} else {
			menuAction_t *a;
			const int baseType = property->type & V_UI_MASK;
			if (baseType != 0 && baseType != V_UI_CVAR) {
				Com_Printf("MN_ParseSetAction: setter for property '%s' (type %d, 0x%X) is not supported (%s)\n", property->string, property->type, property->type, MN_GetPath(menuNode));
				return qfalse;
			}
			mn.curadata = Com_AlignPtr(mn.curadata, property->type & V_BASETYPEMASK);
			a = MN_AllocStaticAction();
			a->type = EA_VALUE_RAW;
			a->d.terminal.d1.data = mn.curadata;
			a->d.terminal.d2.constData = property;
			action->d.nonTerminal.right = a;
			/** @todo we should hide use of mn.curadata */
			mn.curadata += Com_EParseValue(mn.curadata, *token, property->type & V_BASETYPEMASK, 0, property->size);
		}
	}
	return qtrue;
}

/**
 * @brief Parse actions and return action list
 * @return The first element from a list of action
 * @sa ea_t
 * @todo need cleanup, compute action out of the final memory; reduce number of var
 */
static menuAction_t *MN_ParseActionList (menuNode_t *menuNode, const char **text, const const char **token)
{
	const char *errhead = "MN_ParseActionList: unexpected end of file (in event)";
	menuAction_t *firstAction;
	menuAction_t *lastAction;
	menuAction_t *action;
	qboolean result;

	lastAction = NULL;
	firstAction = NULL;

	/* prevent bad position */
	if (*token[0] != '{') {
		Com_Printf("MN_ParseActionList: token \"{\" expected, but \"%s\" found (in event) (node: %s)\n", *token, MN_GetPath(menuNode));
		return NULL;
	}

	while (qtrue) {
		int type = EA_NULL;

		/* get new token */
		*token = Com_EParse(text, errhead, NULL);
		if (!*token)
			return NULL;

		if (*token[0] == '}')
			break;


		type = MN_GetActionTokenType(*token, EA_ACTION);
		/* setter form */
		if (type == EA_NULL && *token[0] == '*')
			type = EA_ASSIGN;

		/* unknown, we break the parsing */
		if (type == EA_NULL) {
			Com_Printf("MN_ParseActionList: unknown token \"%s\" ignored (in event) (node: %s)\n", *token, MN_GetPath(menuNode));
			return NULL;
		}

		/* add the action */
		action = MN_AllocStaticAction();
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
			action->d.terminal.d1.string = MN_AllocStaticString(*token, 0);
			break;

		case EA_ASSIGN:
			result = MN_ParseSetAction(menuNode, action, text, token, errhead);
			if (!result)
				return NULL;
			break;

		case EA_CALL:
			{
				menuAction_t *expression;
				expression = MN_ParseExpression(text, errhead, menuNode);
				if (expression == NULL)
					return NULL;

				if (expression->type != EA_VALUE_PATHNODE && expression->type != EA_VALUE_PATHPROPERTY) {
					Com_Printf("MN_ParseActionList: \"call\" keyword only support pathnode and pathproperty (node: %s)\n", MN_GetPath(menuNode));
					return NULL;
				}

				action->d.nonTerminal.left = expression;
				break;
			}

		case EA_DELETE:
			{
				menuAction_t *expression;
				expression = MN_ParseExpression(text, errhead, menuNode);
				if (expression == NULL)
					return NULL;

				if (expression->type != EA_VALUE_CVARNAME) {
					Com_Printf("MN_ParseActionList: \"delete\" keyword only support cvarname (node: %s)\n", MN_GetPath(menuNode));
					return NULL;
				}

				action->d.nonTerminal.left = expression;
				break;
			}

		case EA_ELIF:
			/* check previous action */
			if (!lastAction || (lastAction->type != EA_IF && lastAction->type != EA_ELIF)) {
				Com_Printf("MN_ParseActionList: 'elif' must be set after an 'if' or an 'elif' (node: %s)\n", MN_GetPath(menuNode));
				return NULL;
			}
			/* then it execute EA_IF, no break */
		case EA_WHILE:
		case EA_IF:
			{
				menuAction_t *expression;

				/* get the condition */
				expression = MN_ParseExpression(text, errhead, menuNode);
				if (expression == NULL)
					return NULL;
				action->d.nonTerminal.left = expression;

				/* get the action block */
				*token = Com_EParse(text, errhead, NULL);
				if (!*text)
					return NULL;
				action->d.nonTerminal.right = MN_ParseActionList(menuNode, text, token);
				if (action->d.nonTerminal.right == NULL) {
					if (action->type == EA_IF)
						Com_Printf("MN_ParseActionList: block expected after \"if\" (node: %s)\n", MN_GetPath(menuNode));
					else if (action->type == EA_ELIF)
						Com_Printf("MN_ParseActionList: block expected after \"elif\" (node: %s)\n", MN_GetPath(menuNode));
					else
						Com_Printf("MN_ParseActionList: block expected after \"while\" (node: %s)\n", MN_GetPath(menuNode));
					return NULL;
				}
				break;
			}

		case EA_ELSE:
			/* check previous action */
			if (!lastAction || (lastAction->type != EA_IF && lastAction->type != EA_ELIF)) {
				Com_Printf("MN_ParseActionList: 'else' must be set after an 'if' or an 'elif' (node: %s)\n", MN_GetPath(menuNode));
				return NULL;
			}

			/* get the action block */
			*token = Com_EParse(text, errhead, NULL);
			if (!*text)
				return NULL;
			action->d.nonTerminal.left = NULL;
			action->d.nonTerminal.right = MN_ParseActionList(menuNode, text, token);
			if (action->d.nonTerminal.right == NULL) {
				Com_Printf("MN_ParseActionList: block expected after \"else\" (node: %s)\n", MN_GetPath(menuNode));
				return NULL;
			}
			break;

		default:
			assert(qfalse);
		}

		/* step */
		lastAction = action;
	}

	assert(*token[0] == '}');

	/* return non NULL value */
	if (firstAction == NULL) {
		firstAction = MN_AllocStaticAction();
	}

	return firstAction;
}

/**
 * @brief Parse an option into a node
 * the options data can be defined like this
 * @code
 * option string {
 *  value "value"
 *  action "command string"
 * }
 * @endcode
 * The strings will appear in the drop down list of the select box
 * if you select the 'string', the 'cvarname' will be set to 'value'
 * and if action is defined (which is a console/script command string)
 * this one is executed on each selection
 */
static qboolean MN_ParseOption (menuNode_t * node, const char **text, const char **token, const char *errhead)
{
	menuOption_t *option;

	/* get parameters */
	*token = Com_EParse(text, errhead, node->name);
	if (!*text)
		return qfalse;

	option = MN_AllocStaticOption(1);
	if (option == NULL) {
		Com_Printf("MN_ParseOption: Too many option entries for node %s\n", MN_GetPath(node));

		*token = Com_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;
		if (*token[0] != '{') {
			Com_Printf("MN_ParseOption: node with bad option definition ignored (node \"%s\")\n", MN_GetPath(node));
			return qfalse;
		}
		FS_SkipBlock(text);
		return qfalse;
	}

	Q_strncpyz(option->id, *token, sizeof(option->id));
	Com_DPrintf(DEBUG_CLIENT, "...found selectbox: '%s'\n", *token);

	*token = Com_EParse(text, errhead, node->name);
	if (!*text)
		return qfalse;
	if (*token[0] != '{') {
		Com_Printf("MN_ParseOption: node with bad option definition ignored (node \"%s\")\n", MN_GetPath(node));
		return qfalse;
	}

	do {
		const value_t *property;
		int result;

		*token = Com_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;

		if (*token[0] == '}')
			break;

		property = MN_FindPropertyByName(optionProperties, *token);
		if (!property) {
			Com_Printf("MN_ParseOption: unknown options property: '%s'\n", *token);
			return qfalse;
		}

		/* get parameter values */
		result = MN_ParseProperty(option, property, option->id, text, token);
		if (!result) {
			Com_Printf("MN_ParseOption: Parsing for option '%s'. See upper\n", option->id);
			return qfalse;
		}

	} while (*token[0] != '}');
	MN_NodeAppendOption(node, option);
	return qtrue;
}

static qboolean MN_ParseExcludeRect (menuNode_t * node, const char **text, const char **token, const char *errhead)
{
	excludeRect_t rect;

	/* get parameters */
	*token = Com_EParse(text, errhead, node->name);
	if (!*text)
		return qfalse;
	if (*token[0] != '{') {
		Com_Printf("MN_ParseExcludeRect: node with bad excluderect ignored (node \"%s\")\n", MN_GetPath(node));
		return qtrue;
	}

	do {
		*token = Com_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;
		/** @todo move it into a property array */
		if (!strcmp(*token, "pos")) {
			*token = Com_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;
			Com_EParseValue(&rect, *token, V_POS, offsetof(excludeRect_t, pos), sizeof(vec2_t));
		} else if (!strcmp(*token, "size")) {
			*token = Com_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;
			Com_EParseValue(&rect, *token, V_POS, offsetof(excludeRect_t, size), sizeof(vec2_t));
		}
	} while (*token[0] != '}');


	if (mn.numExcludeRect >= MAX_EXLUDERECTS) {
		Com_Printf("MN_ParseExcludeRect: exluderect limit exceeded (max: %i)\n", MAX_EXLUDERECTS);
		return qfalse;
	}

	/* copy the rect into the global structure */
	mn.excludeRect[mn.numExcludeRect] = rect;

	/* link only the first element */
	if (node->excludeRect == NULL) {
		node->excludeRect = &mn.excludeRect[mn.numExcludeRect];
	}

	mn.numExcludeRect++;
	node->excludeRectNum++;

	return qtrue;
}

static qboolean MN_ParseEventProperty (menuNode_t * node, const value_t *event, const char **text, const char **token, const char *errhead)
{
	menuAction_t **action;

	/* add new actions to end of list */
	action = (menuAction_t **) ((byte *) node + event->ofs);
	for (; *action; action = &(*action)->next) {}

	/* get the action body */
	*token = Com_EParse(text, errhead, node->name);
	if (!*text)
		return qfalse;

	if (*token[0] != '{') {
		Com_Printf("MN_ParseEventProperty: Event '%s' without body (%s)\n", event->string, MN_GetPath(node));
		return qfalse;
	}

	*action = MN_ParseActionList(node, text, token);
	if (*action == NULL)
		return qfalse;

	/* block terminal already read */
	assert(*token[0] == '}');

	return qtrue;
}

/**
 * @brief Parse a property value
 * @todo don't read the next token (need to change the script language)
 */
static qboolean MN_ParseProperty (void* object, const value_t *property, const char* objectName, const char **text, const char **token)
{
	const char *errhead = "MN_ParseProperty: unexpected end of file (object";
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
				Com_Printf("MN_ParseProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
				return qfalse;
			}
		}
		break;

	case V_UI_REF:
		*token = Com_EParse(text, errhead, objectName);
		if (!*text)
			return qfalse;

		/* a reference to data is handled like this */
		mn.curadata = Com_AlignPtr(mn.curadata, property->type & V_BASETYPEMASK);
		*(byte **) ((byte *) object + property->ofs) = mn.curadata;

		/** @todo check for the moment its not a cvar */
		assert (*token[0] != '*');

		/* sanity check */
		if ((property->type & V_BASETYPEMASK) == V_STRING && strlen(*token) > MAX_VAR - 1) {
			Com_Printf("MN_ParseProperty: Value '%s' is too long (key %s)\n", *token, property->string);
			return qfalse;
		}

		result = Com_ParseValue(mn.curadata, *token, property->type & V_BASETYPEMASK, 0, property->size, &bytes);
		if (result != RESULT_OK) {
			Com_Printf("MN_ParseProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
			return qfalse;
		}
		mn.curadata += bytes;

		break;

	case V_UI_CVAR:	/* common type */
		*token = Com_EParse(text, errhead, objectName);
		if (!*text)
			return qfalse;

		/* references are parsed as string */
		if (*token[0] == '*') {
			/* a reference to data */
			mn.curadata = Com_AlignPtr(mn.curadata, V_STRING);
			*(byte **) valuePtr = mn.curadata;

			/* sanity check */
			if (strlen(*token) > MAX_VAR - 1) {
				Com_Printf("MN_ParseProperty: Value '%s' is too long (key %s)\n", *token, property->string);
				return qfalse;
			}

			result = Com_ParseValue(mn.curadata, *token, V_STRING, 0, 0, &bytes);
			if (result != RESULT_OK) {
				Com_Printf("MN_ParseProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
				return qfalse;
			}
			mn.curadata += bytes;
		} else {
			/* a reference to data */
			mn.curadata = Com_AlignPtr(mn.curadata, property->type & V_BASETYPEMASK);
			*(byte **) valuePtr = mn.curadata;

			/* sanity check */
			if ((property->type & V_BASETYPEMASK) == V_STRING && strlen(*token) > MAX_VAR - 1) {
				Com_Printf("MN_ParseProperty: Value '%s' is too long (key %s)\n", *token, property->string);
				return qfalse;
			}

			result = Com_ParseValue(mn.curadata, *token, property->type & V_BASETYPEMASK, 0, property->size, &bytes);
			if (result != RESULT_OK) {
				Com_Printf("MN_ParseProperty: Invalid value for property '%s': %s\n", property->string, Com_GetLastParseError());
				return qfalse;
			}
			mn.curadata += bytes;
		}
		break;

	case V_UI:

		switch (property->type) {
		case V_UI_ACTION:
			result = MN_ParseEventProperty(object, property, text, token, errhead);
			if (!result)
				return qfalse;
			break;

		case V_UI_EXCLUDERECT:
			result = MN_ParseExcludeRect(object, text, token, errhead);
			if (!result)
				return qfalse;
			break;

		case V_UI_OPTIONNODE:
			result = MN_ParseOption(object, text, token, errhead);
			if (!result)
				return qfalse;
			break;

		case V_UI_ICONREF:
			{
				menuIcon_t** icon = (menuIcon_t**) valuePtr;
				*token = Com_EParse(text, errhead, objectName);
				if (!*text)
					return qfalse;
				*icon = MN_GetIconByName(*token);
				if (*icon == NULL) {
					Com_Printf("MN_ParseProperty: icon '%s' not found (object %s)\n", *token, objectName);
				}
			}
			break;

		case V_UI_IF:
			{
				menuAction_t **expression = (menuAction_t **) valuePtr;

				*token = Com_EParse(text, errhead, objectName);
				if (!*text)
					return qfalse;

				*expression = MN_AllocStaticStringCondition(*token);
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

				*dataId = MN_GetDataIDByName(*token);
				if (*dataId < 0) {
					Com_Printf("MN_ParseProperty: Could not find menu dataId '%s' (%s@%s)\n",
							*token, objectName, property->string);
					return qfalse;
				}
			}
			break;

		default:
			Com_Printf("MN_ParseProperty: unknown property type '%d' (0x%X) (%s@%s)\n",
					property->type, property->type, objectName, property->string);
			return qfalse;
		}
		break;

	default:
		Com_Printf("MN_ParseProperties: unknown property type '%d' (0x%X) (%s@%s)\n",
				property->type, property->type, objectName, property->string);
		return qfalse;
	}

	return qtrue;
}

static qboolean MN_ParseFunction (menuNode_t * node, const char **text, const char **token)
{
	menuAction_t **action;
	assert (node->behaviour->isFunction);

	/* add new actions to end of list */
	/** @todo [begin] this code look stange */
	action = &node->onClick;
	for (; *action; action = &(*action)->next) {}

	if (mn.numActions >= MAX_MENUACTIONS)
		Com_Error(ERR_FATAL, "MN_ParseFunction: MAX_MENUACTIONS exceeded (%i)", mn.numActions);
	*action = &mn.actions[mn.numActions++];
	memset(*action, 0, sizeof(**action));
	/** @todo [end] this code look strange */

	*action = MN_ParseActionList(node, text, token);
	if (*action == NULL)
		return qfalse;

	return *token[0] == '}';
}

/**
 * @sa MN_ParseNodeProperties
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
static qboolean MN_ParseNodeProperties (menuNode_t * node, const char **text, const char **token)
{
	const char *errhead = "MN_ParseNodeProperties: unexpected end of file (node";
	qboolean nextTokenAlreadyRead = qfalse;

	if (*token[0] != '{') {
		nextTokenAlreadyRead = qtrue;
	}

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
		if (*token[0] == '}') {
			break;
		}

		/* find the property */

		val = MN_GetPropertyFromBehaviour(node->behaviour, *token);
		if (!val) {
			/* unknown token, print message and continue */
			Com_Printf("MN_ParseNodeProperties: unknown property \"%s\", node ignored (node %s)\n",
					*token, MN_GetPath(node));
			return qfalse;
		}

		/* get parameter values */
		result = MN_ParseProperty(node, val, node->name, text, token);
		if (!result) {
			Com_Printf("MN_ParseNodeProperties: Problem with parsing of node property '%s@%s'. See upper\n",
					MN_GetPath(node), val->string);
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
static qboolean MN_ParseNodeBody (menuNode_t * node, const char **text, const char **token, const char *errhead)
{
	qboolean result = qtrue;

	if (*token[0] != '{') {
		/* read the body block start */
		*token = Com_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;
		if (*token[0] != '{') {
			Com_Printf("MN_ParseNodeBody: node doesn't have body, token '%s' read (node \"%s\")\n", *token, MN_GetPath(node));
			mn.numNodes--;
			return qfalse;
		}
	}

	/* functions are a special case */
	if (node->behaviour->isFunction) {
		result = MN_ParseFunction(node, text, token);
	} else {

		/* check the content */
		*token = Com_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;

		if (*token[0] == '{') {
			/* we have a special block for properties */
			result = MN_ParseNodeProperties(node, text, token);
			if (!result)
				return qfalse;

			/* move token over the next node behaviour */
			*token = Com_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;

			/* and then read all nodes */
			while (*token[0] != '}') {
				menuNode_t *new = MN_ParseNode(node, text, token, errhead);
				if (!new)
					return qfalse;

				*token = Com_EParse(text, errhead, node->name);
				if (*text == NULL)
					return qfalse;
			}
		} else if (MN_GetPropertyFromBehaviour(node->behaviour, *token)) {
			/* we should have a block with properties only */
			result = MN_ParseNodeProperties(node, text, token);
		} else {
			/* we should have a block with nodes only */
			while (*token[0] != '}') {
				menuNode_t *new = MN_ParseNode(node, text, token, errhead);
				if (!new)
					return qfalse;

				*token = Com_EParse(text, errhead, node->name);
				if (*text == NULL)
					return qfalse;
			}
		}
	}
	if (!result) {
		Com_Printf("MN_ParseNodeBody: node with bad body ignored (node \"%s\")\n", MN_GetPath(node));
		mn.numNodes--;
		return qfalse;
	}

	/* already check on MN_ParseNodeProperties */
	assert(*token[0] == '}');
	return qtrue;
}

/**
 * @brief parse a node and complet the menu with it
 * @sa MN_ParseNodeProperties
 * @todo we can think about merging MN_ParseNodeProperties here
 * @note first token already read
 * @note dont read more than the need token (last right token is '}' of end of node)
 */
static menuNode_t *MN_ParseNode (menuNode_t * parent, const char **text, const char **token, const char *errhead)
{
	menuNode_t *node = NULL;
	nodeBehaviour_t *behaviour;
	menuNode_t *component = NULL;
	qboolean result;

	/* allow to begin with the identifier "node" before the behaviour name */
	if (!Q_strcasecmp(*token, "node")) {
		*token = Com_EParse(text, errhead, "");
		if (!*text)
			return NULL;
	}

	/* get the behaviour */
	behaviour = MN_GetNodeBehaviour(*token);
	if (!behaviour) {
		component = MN_GetComponent(*token);
	}
	if (behaviour == NULL && component == NULL) {
		Com_Printf("MN_ParseNode: node behaviour/component '%s' doesn't exists\n", *token);
		return NULL;
	}

	/* get the name */
	*token = Com_EParse(text, errhead, "");
	if (!*text)
		return NULL;
	if (MN_IsReservedToken(*token)) {
		Com_Printf("MN_ParseNode: \"%s\" is a reserved token, we can't call a node with it\n", *token);
		return NULL;
	}

	/* test if node already exists */
	/** Already existing node should only come from inherited node,we should not have 2 definitions of the same node into the same window. */
	if (parent)
		node = MN_GetNode(parent, *token);
	if (node) {
		if (node->behaviour != behaviour) {
			Com_Printf("MN_ParseNode: we can't change node type (node \"%s\")\n", MN_GetPath(node));
			return NULL;
		}
		Com_DPrintf(DEBUG_CLIENT, "... over-riding node %s\n", MN_GetPath(node));
		/* reset action list of node */
		/* maybe it mean "reset the code when it is an inherited function" */
		node->onClick = NULL;	/**< @todo understand why this strange hack exists (there is a lot of over actions) */

#if 0
		/* reordering the node to the end */
		/* finally not a good idea... */
		MN_RemoveNode(node->parent, node);
		MN_AppendNode(node->parent, node);
#endif

	/* else initialize node */
	} else if (component) {
		node = MN_CloneNode(component, NULL, qtrue, *token);
		if (parent) {
			if (parent->root)
				MN_UpdateRoot(node, parent->root);
			MN_AppendNode(parent, node);
		}
	} else {
		node = MN_AllocStaticNode(behaviour->name);
		node->parent = parent;
		if (parent)
			node->root = parent->root;
		Q_strncpyz(node->name, *token, sizeof(node->name));
		if (strlen(node->name) != strlen(*token))
			Com_Printf("MN_ParseNode: Node name \"%s\" truncated. New name is \"%s\"\n", *token, node->name);
		/** @todo move it into caller */
		if (parent)
			MN_AppendNode(parent, node);
	}

	/* throw to the node, we begin to read attributes */
	if (node->behaviour->loading)
		node->behaviour->loading(node);

	/* get body */
	result = MN_ParseNodeBody(node, text, token, errhead);
	if (!result)
		return NULL;

	/* initialize the node according to its behaviour */
	if (node->behaviour->loaded) {
		node->behaviour->loaded(node);
	}
	return node;
}

/**
 * @brief Parse menu transform
 */
static qboolean MN_ParseMenuTransform (menuModel_t *menuModel, const char **text, const char **token, const char *errhead)
{
	*token = Com_EParse(text, errhead, menuModel->id);
	if (!*text)
		return qfalse;
	if (*token[0] != '{') {
		Com_Printf("Error in menumodel '%s' menutransform definition\n", menuModel->id);
		return qfalse;
	}
	do {
		*token = Com_EParse(text, errhead, menuModel->id);
		if (!*text)
			return qfalse;
		if (*token[0] == '}')
			break;
		menuModel->menuTransform[menuModel->menuTransformCnt].menuID = Mem_PoolStrDup(*token, mn_sysPool, 0);

		*token = Com_EParse(text, errhead, menuModel->id);
		if (!*text)
			return qfalse;
		if (*token[0] == '}') {
			Com_Printf("Error in menumodel '%s' menutransform definition - missing scale float value\n", menuModel->id);
			break;
		}
		if (*token[0] == '#') {
			menuModel->menuTransform[menuModel->menuTransformCnt].useScale = qfalse;
		} else {
			Com_EParseValue(&menuModel->menuTransform[menuModel->menuTransformCnt].scale, *token, V_VECTOR, 0, sizeof(vec3_t));
			menuModel->menuTransform[menuModel->menuTransformCnt].useScale = qtrue;
		}

		*token = Com_EParse(text, errhead, menuModel->id);
		if (!*text)
			return qfalse;
		if (*token[0] == '}') {
			Com_Printf("Error in menumodel '%s' menutransform definition - missing angles float value\n", menuModel->id);
			break;
		}
		if (*token[0] == '#') {
			menuModel->menuTransform[menuModel->menuTransformCnt].useAngles = qfalse;
		} else {
			Com_EParseValue(&menuModel->menuTransform[menuModel->menuTransformCnt].angles, *token, V_VECTOR, 0, sizeof(vec3_t));
			menuModel->menuTransform[menuModel->menuTransformCnt].useAngles = qtrue;
		}

		*token = Com_EParse(text, errhead, menuModel->id);
		if (!*text)
			return qfalse;
		if (*token[0] == '}') {
			Com_Printf("Error in menumodel '%s' menutransform definition - missing origin float value\n", menuModel->id);
			break;
		}
		if (*token[0] == '#') {
			menuModel->menuTransform[menuModel->menuTransformCnt].useOrigin = qfalse;
		} else {
			Com_EParseValue(&menuModel->menuTransform[menuModel->menuTransformCnt].origin, *token, V_VECTOR, 0, sizeof(vec3_t));
			menuModel->menuTransform[menuModel->menuTransformCnt].useOrigin = qtrue;
		}

		menuModel->menuTransformCnt++;
	} while (*token[0] != '}'); /* dummy condition - break is earlier here */

	return qtrue;
}

/**
 * @brief parses the models.ufo and all files where menu_models are defined
 * @sa CL_ParseClientData
 */
void MN_ParseMenuModel (const char *name, const char **text)
{
	menuModel_t *menuModel;
	const char *token;
	int i;
	const value_t *v = NULL;
	const char *errhead = "MN_ParseMenuModel: unexpected end of file (names ";

	/* search for menumodels with same name */
	for (i = 0; i < mn.numMenuModels; i++)
		if (!strcmp(mn.menuModels[i].id, name)) {
			Com_Printf("MN_ParseMenuModel: menu_model \"%s\" with same name found, second ignored\n", name);
			return;
		}

	if (mn.numMenuModels >= MAX_MENUMODELS) {
		Com_Printf("MN_ParseMenuModel: Max menu models reached\n");
		return;
	}

	/* initialize the menu */
	menuModel = &mn.menuModels[mn.numMenuModels];
	memset(menuModel, 0, sizeof(*menuModel));

	Vector4Set(menuModel->color, 1, 1, 1, 1);

	menuModel->id = Mem_PoolStrDup(name, mn_sysPool, 0);
	Com_DPrintf(DEBUG_CLIENT, "Found menu model %s (%i)\n", menuModel->id, mn.numMenuModels);

	/* get it's body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("MN_ParseMenuModel: menu \"%s\" without body ignored\n", menuModel->id);
		return;
	}

	mn.numMenuModels++;

	do {
		/* get the name type */
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		v = MN_FindPropertyByName(menuModelProperties, token);
		if (!v)
			Com_Printf("MN_ParseMenuModel: unknown token \"%s\" ignored (menu_model %s)\n", token, name);

		if (v->type == V_NULL) {
			if (!strcmp(v->string, "need")) {
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;
				menuModel->next = MN_GetMenuModel(token);
				if (!menuModel->next)
					Com_Printf("Could not find menumodel %s", token);
				menuModel->need = Mem_PoolStrDup(token, mn_sysPool, 0);
			} else if (!strcmp(v->string, "menutransform")) {
				qboolean result;
				result = MN_ParseMenuTransform (menuModel, text, &token, errhead);
				if (!result)
					return;
			}
		} else {
			token = Com_EParse(text, errhead, name);
			if (!*text)
				return;
			switch (v->type) {
			case V_CLIENT_HUNK_STRING:
				Mem_PoolStrDupTo(token, (char**) ((char*)menuModel + (int)v->ofs), mn_sysPool, 0);
				break;
			default:
				Com_EParseValue(menuModel, token, v->type, v->ofs, v->size);
			}
		}
	} while (*text);
}

void MN_ParseIcon (const char *name, const char **text)
{
	menuIcon_t *icon;
	const char *token;

	/* search for menus with same name */
	icon = MN_AllocStaticIcon(name);

	/* get it's body */
	token = Com_Parse(text);
	assert(strcmp(token, "{") == 0);

	/* read properties */
	while (qtrue) {
		const value_t *property;
		qboolean result;

		token = Com_Parse(text);
		if (*text == NULL)
			return;

		if (token[0] == '}')
			break;

		property = MN_FindPropertyByName(mn_iconProperties, token);
		if (!property) {
			Com_Printf("MN_ParseIcon: unknown options property: '%s' - ignore it\n", token);
			return;
		}

		/* get parameter values */
		result = MN_ParseProperty(icon, property, icon->name, text, &token);
		if (!result) {
			Com_Printf("MN_ParseIcon: Parsing for icon '%s'. See upper\n", icon->name);
			return;
		}
	}

	return;
}

/**
 * @brief Parse a component
 * @sa CL_ParseClientData
 * @code
 * component panel componentName {
 * }
 * @endcode
 */
void MN_ParseComponent (const char *type, const char **text)
{
	const char *errhead = "MN_ParseComponent: unexpected end of file (component";
	menuNode_t *component;
	const char *token;

	if (strcmp(type, "component") != 0) {
		Com_Error(ERR_FATAL, "MN_ParseComponent: \"component\" expected but \"%s\" found.\n", type);
		return;	/* never reached */
	}

	/* CL_ParseClientData read the real type as name */
	Com_UnParseLastToken();
	token = Com_Parse(text);

	component = MN_ParseNode(NULL, text, &token, errhead);
	if (component)
		MN_InsertComponent(component);
}


/**
 * @brief Parse a window
 * @sa CL_ParseClientData
 * @code
 * window windowName {
 * }
 * @endcode
 */
void MN_ParseWindow (const char *type, const char *name, const char **text)
{
	const char *errhead = "MN_ParseWindow: unexpected end of file (menu";
	menuNode_t *menu;
	const char *token;
	qboolean result;
	int i;

	if (strcmp(type, "window") != 0) {
		Com_Error(ERR_FATAL, "MN_ParseWindow: '%s %s' is not a window node\n", type, name);
		return;	/* never reached */
	}

	if (MN_IsReservedToken(name)) {
		Com_Printf("MN_ParseWindow: \"%s\" is a reserved token, we can't call a node with it (node \"%s\")\n", name, name);
		return;
	}

	/* search for menus with same name */
	for (i = 0; i < mn.numWindows; i++)
		if (!strncmp(name, mn.windows[i]->name, sizeof(mn.windows[i]->name)))
			break;

	if (i < mn.numWindows) {
		Com_Printf("MN_ParseWindow: %s \"%s\" with same name found, second ignored\n", type, name);
	}

	if (mn.numWindows >= MAX_MENUS) {
		Com_Error(ERR_FATAL, "MN_ParseWindow: max menus exceeded (%i) - ignore '%s'\n", MAX_MENUS, name);
		return;	/* never reached */
	}

	/* get menu body */
	token = Com_Parse(text);

	/* does this menu inherit data from another menu? */
	if (!strcmp(token, "extends")) {
		menuNode_t *superMenu;
		token = Com_Parse(text);
		superMenu = MN_GetMenu(token);
		if (superMenu == NULL)
			Sys_Error("Could not get the super menu %s", token);
		menu = MN_CloneNode(superMenu, NULL, qtrue, name);
		token = Com_Parse(text);
	} else {
		menu = MN_AllocStaticNode(type);
		menu->root = menu;
		menu->behaviour->loading(menu);
		Q_strncpyz(menu->name, name, sizeof(menu->name));
		if (strlen(menu->name) != strlen(name))
			Com_Printf("MN_ParseWindow: Menu name \"%s\" truncated. New name is \"%s\"\n", name, menu->name);
	}

	MN_InsertMenu(menu);

	/* parse it's body */
	result = MN_ParseNodeBody(menu, text, &token, errhead);
	if (!result) {
		Com_Error(ERR_FATAL, "MN_ParseWindow: menu \"%s\" has a bad body\n", menu->name);
		return;	/* never reached */
	}

	menu->behaviour->loaded(menu);
}

/**
 * @sa Com_MacroExpandString
 * @todo should review this code, '*' dont check very well every things
 */
const char *MN_GetReferenceString (const menuNode_t* const node, const char *ref)
{
	if (!ref)
		return NULL;

	/* its a cvar */
	if (ref[0] == '*') {
		const char *token;

		/* get the reference and the name */
		token = Com_MacroExpandString(ref);
		if (token)
			return token;

		/* skip the star */
		token = ref + 1;
		if (token[0] == '\0')
			return NULL;

		if (!strncmp(token, "binding:", 8)) {
			/* skip prefix */
			token = token + 8;
			return Key_GetBinding(token, (cls.state != ca_active ? KEYSPACE_MENU : KEYSPACE_GAME));
		} else {
			Sys_Error("MN_GetReferenceString: unknown reference");	/**< maybe this code is never used */
#if 0	/** @todo need a full rework */
			menuNode_t *refNode;
			const value_t *val;

			token = Com_Parse(&text);
			if (!text)
				return NULL;

			/* draw a reference to a node property */
			refNode = MN_GetNode(node->root, ident);
			if (!refNode)
				return NULL;

			/* get the property */
			val = MN_GetPropertyFromBehaviour(refNode->behaviour, token);
			if (!val)
				return NULL;

			/* get the string */
			return Com_ValueToStr(refNode, val->type & V_BASETYPEMASK, val->ofs);
#endif
		}

	/* traslatable string */
	} else if (ref[0] == '_') {
		ref++;
		return _(ref);

	/* just a string */
	} else {
		return ref;
	}
}

float MN_GetReferenceFloat (const menuNode_t* const node, const void *ref)
{
	if (!ref)
		return 0.0;
	if (((const char *) ref)[0] == '*') {
		const char *token;
		token = (const char *) ref + 1;

		if (token[0] == '\0')
			return 0.0;

		if (!strncmp(token, "cvar:", 5)) {
			/* get the cvar value */
			return Cvar_GetValue(token + 5);
		} else {
			Sys_Error("MN_GetReferenceFloat: unknown reference");	/**< maybe this code is never used */
#if 0	/** @todo need a full rework */
			menuNode_t *refNode;
			const value_t *val;

			/* draw a reference to a node property */
			refNode = MN_GetNode(node->root, ident);
			if (!refNode)
				return 0.0;

			/* get the property */
			val = MN_GetPropertyFromBehaviour(refNode->behaviour, token);
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
