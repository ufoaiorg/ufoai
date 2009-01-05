/**
 * @file m_parse.c
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

#include "../client.h"
#include "m_parse.h"
#include "m_main.h"
#include "m_actions.h"
#include "m_icon.h"
#include "node/m_node_window.h"
#include "node/m_node_selectbox.h"

static qboolean MN_ParseProperty (void* object, const value_t *property, const char* objectName, const char **text, const char **token);

/** @brief valid properties for options of the selectbox and tab */
static const value_t selectBoxValues[] = {
	{"label", V_TRANSLATION_MANUAL_STRING, offsetof(selectBoxOptions_t, label), sizeof(char) * SELECTBOX_MAX_VALUE_LENGTH},
	{"action", V_STRING, offsetof(selectBoxOptions_t, action), 0},
	{"value", V_STRING, offsetof(selectBoxOptions_t, value), 0},
	{"icon", V_SPECIAL_ICONREF, offsetof(selectBoxOptions_t, icon), 0},

	{NULL, V_NULL, 0, 0},
};

/** @brief valid properties for a menu model definition */
static const value_t menuModelValues[] = {
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

/** @brief valid node event actions */
static const char *ea_strings[EA_NUM_EVENTACTION] = {
	"",
	"cmd",
	"call",
	"set",
	"if"
};

#define EA_SPECIAL_NUM_EVENTACTION 1
#define EA_SPECIAL_TIMEOUT (EA_NUM_EVENTACTION + 1)

static const char *ea_special_strings[EA_SPECIAL_NUM_EVENTACTION] = {
	"timeout",
};

/* =========================================================== */

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
 * @brief Allocate a string into the menu memory
 * @note Its not a dynamic memory allocation. Please only use it at the loading time
 * @param[in] string Use to initialize the string
 * @param[in] size request a fixed memory size, if 0 the string size is used
 * @todo use it every where its possible (search mn.curadata)
 * @todo Assert out when we are not in parsing/loading stage
 */
char* MN_AllocString (const char* string, int size)
{
	char* result = (char *)mn.curadata;
	if (size != 0) {
		assert(mn.curadata - mn.adata + size <= mn.adataize);
		strncpy((char *)mn.curadata, string, size);
		mn.curadata += size;
	} else {
		assert(mn.curadata - mn.adata + strlen(string) + 1 <= mn.adataize);
		mn.curadata += sprintf((char *)mn.curadata, "%s", string) + 1;
	}
	return result;
}

/* prototype */
static menuAction_t *MN_ParseAction(menuNode_t *menuNode, const char **text, const const char **token);

static inline qboolean MN_ParseSetAction (menuNode_t *menuNode, menuAction_t *action, const char **text, const const char **token, const char *errhead)
{
	char cast[32] = "abstractnode";
	char path[MAX_VAR];
	const char *nodeName = *token + 1;
	nodeBehaviour_t *castedBehaviour;
	const value_t *val;

	path[0] = '\0';

	/* cvar setter */
	if (Q_strncmp(nodeName, "cvar:", 5) == 0) {
		action->data = MN_AllocString(nodeName + 5, 0);
		action->type.param1 = EA_CVARNAME;
		action->type.param2 = EA_VALUE;

		/* get the value */
		*token = COM_EParse(text, errhead, NULL);
		if (!*text)
			return qfalse;
		MN_AllocString(*token, 0);
		return qtrue;
	}

	/* copy the menu name, and move to the node name */
	if (Q_strncmp(nodeName, "menu:", 5) == 0) {
		const char *menuName = nodeName + 5;
		nodeName = strchr(nodeName, '.');
		assert(nodeName);
		nodeName++;
		assert(nodeName - menuName < sizeof(path));
		Q_strncpyz(path, menuName, nodeName - menuName + 1);
		action->type.param1 = EA_PATHPROPERTY;
	} else {
		action->type.param1 = EA_THISMENUNODENAMEPROPERTY;
	}

	/* check cast */
	if (nodeName[0] == '(') {
		char *end = strchr(nodeName, ')');
		assert(end != NULL);
		assert(end - nodeName < sizeof(cast));

		/* copy the cast and update the node name */
		if (end != NULL) {
			Q_strncpyz(cast, nodeName + 1, end - nodeName);
			nodeName = end + 1;
		}
	}

	/* copy the node path */
	if (action->type.param1 == EA_PATHPROPERTY) {
		Q_strcat(path, nodeName, sizeof(path));
		action->data = (byte*) MN_AllocString(path, 0);
	} else {
		action->data = (byte*) MN_AllocString(nodeName, 0);
	}

	/* get the node property */
	*token = COM_EParse(text, errhead, NULL);
	if (!*text)
		return qfalse;

	castedBehaviour = MN_GetNodeBehaviour(cast);
	val = MN_GetPropertyFromBehaviour(castedBehaviour, *token);
	if (!val && action->type.param1 != EA_PATHPROPERTY) {
		/* do we ALREADY know this node? and his type */
		const menuNode_t *node = MN_GetNode(menuNode->menu, action->data);
		if (node) {
			val = MN_NodeGetPropertyDefinition(node, *token);
		} else {
			Com_Printf("MN_ParseSetAction: node \"%s\" not already know (in event), you can cast it\n", *token);
		}
	}

	action->scriptValues = val;

	if (!val || !val->type) {
		Com_Printf("MN_ParseSetAction: token \"%s\" isn't a node property (in event)\n", *token);
		action->type.op = EA_NULL;
		return qtrue;
	}

	action->type.param2 = EA_RAWVALUE;

	/* get the value */
	*token = COM_EParse(text, errhead, NULL);
	if (!*text)
		return qfalse;

	if (val->type == V_SPECIAL_ACTION) {
		void *mem = mn.curadata;
		mn.curadata += sizeof(menuAction_t*);
		*(menuAction_t**)mem = MN_ParseAction(menuNode, text, token);
	} else {
		if (MN_IsInjectedString(*token)) {
			action->type.param2 = EA_VALUE;
			MN_AllocString(*token, 0);
		} else {
			const int baseType = val->type & V_SPECIAL_TYPE;
			if (baseType != 0 && baseType != V_SPECIAL_CVAR) {
				Com_Printf("MN_ParseSetAction: setter for property '%s' (type %d, 0x%X) is not supported (%s.%s)\n", val->string, val->type, val->type, menuNode->menu->name, menuNode->name);
				return qfalse;
			}
			mn.curadata += Com_EParseValue(mn.curadata, *token, val->type & V_BASETYPEMASK, 0, val->size);
		}
	}
	return qtrue;
}

/**
 * @brief Prse actions and return action list
 * @return The first element from a list of action
 * @sa ea_t
 * @todo need cleanup, compute action out of the final memory; reduce number of var
 */
static menuAction_t *MN_ParseAction (menuNode_t *menuNode, const char **text, const const char **token)
{
	const char *errhead = "MN_ParseAction: unexpected end of file (in event)";
	menuAction_t *firstAction;
	menuAction_t *lastAction;
	menuAction_t *action;
	qboolean result;
	int i;

	lastAction = NULL;
	firstAction = NULL;

	/* prevent bad position */
	assert(*token[0] == '{');

	while (qtrue) {
		int type = EA_NULL;

		/* get new token */
		*token = COM_EParse(text, errhead, NULL);
		if (!*token)
			return NULL;

		if (*token[0] == '}')
			break;

		/* decode action type */
		for (i = 0; i < EA_NUM_EVENTACTION; i++) {
			if (Q_strcasecmp(*token, ea_strings[i]) == 0) {
				type = i;
				break;
			}
		}
		/* short setter form */
		if (type == EA_NULL && *token[0] == '*') {
			type = EA_SET;
		}
		if (type == EA_NULL) {
			for (i = 0; i < EA_SPECIAL_NUM_EVENTACTION; i++) {
				if (Q_strcasecmp(*token, ea_special_strings[i]) == 0) {
					type = EA_NUM_EVENTACTION + 1 + i;
					break;
				}
			}
		}

		/* unknown, we break the parsing */
		if (type == EA_NULL) {
			Com_Printf("MN_ParseAction: unknown token \"%s\" ignored (in event) (node: %s.%s)\n", *token, menuNode->menu->name, menuNode->name);
			return NULL;
		}

		/** @todo better to append the action after initialization */
		/* add the action */
		if (mn.numActions >= MAX_MENUACTIONS)
			Sys_Error("MN_ParseAction: MAX_MENUACTIONS exceeded (%i)\n", mn.numActions);
		action = &mn.menuActions[mn.numActions++];
		memset(action, 0, sizeof(*action));
		if (lastAction) {
			lastAction->next = action;
		}
		if (!firstAction) {
			firstAction = action;
		}
		action->type.op = type;

		/* decode action */
		switch (action->type.op) {
		case EA_CMD:
			/* get parameter values */
			*token = COM_EParse(text, errhead, NULL);
			if (!*text)
				return NULL;

			/* get the value */
			action->data = mn.curadata;
			mn.curadata += Com_EParseValue(mn.curadata, *token, V_LONGSTRING, 0, 0);
			break;

		case EA_SET:
			/* if not short syntaxe */
			if (Q_strcasecmp(*token, ea_strings[EA_SET]) == 0) {
				*token = COM_EParse(text, errhead, NULL);
				if (!*token)
					return NULL;
			}
			result = MN_ParseSetAction(menuNode, action, text, token, errhead);
			if (!result)
				return NULL;
			break;

		case EA_CALL:
			{
				menuNode_t* callNode = NULL;
				/* get the function name */
				*token = COM_EParse(text, errhead, NULL);
				callNode = MN_GetNode(menuNode->menu, *token);
				if (!callNode) {
					Com_Printf("MN_ParseAction: function '%s' not found (%s.%s)\n", *token, menuNode->menu->name, menuNode->name);
					return NULL;
				}
				action->data = mn.curadata;
				*(menuAction_t ***) mn.curadata = &callNode->onClick;
				mn.curadata += ALIGN(sizeof(menuAction_t *));
				lastAction = action;
			}
			break;

		case EA_IF:
			/* get the condition */
			*token = COM_EParse(text, errhead, NULL);
			if (!*text)
				return NULL;
			action->data = MN_AllocCondition(*token);

			/* get the action block */
			*token = COM_EParse(text, errhead, NULL);
			if (!*text)
				return NULL;
			action->scriptValues = (const value_t *) MN_ParseAction (menuNode, text, token);
			break;

		case EA_SPECIAL_TIMEOUT:
			/* get new token */
			*token = COM_EParse(text, errhead, NULL);
			/** @todo use scanef instead of atoi, no need to check '}' */
			if (!*token || **token == '}') {
				Com_Printf("MN_ParseAction: timeout with no value (in event) (node: %s)\n", menuNode->name);
				return NULL;
			}
			menuNode->timeOut = atoi(*token);

			/* no action for that */
			action->type.op = EA_NULL;
			break;

		default:
			assert(qfalse);
		}

		/* step */
		lastAction = action;
	}

	assert(*token[0] == '}');

	return firstAction;
}

static qboolean MN_ParseOption (menuNode_t * node, const char **text, const char **token, const char *errhead)
{
	/* get parameters */
	*token = COM_EParse(text, errhead, node->name);
	if (!*text)
		return qfalse;
	/** @todo mem corruption in case of too many select box options */
	Q_strncpyz(mn.menuSelectBoxes[mn.numSelectBoxes].id, *token, sizeof(mn.menuSelectBoxes[mn.numSelectBoxes].id));
	Com_DPrintf(DEBUG_CLIENT, "...found selectbox: '%s'\n", *token);

	*token = COM_EParse(text, errhead, node->name);
	if (!*text)
		return qfalse;
	if (*token[0] != '{') {
		Com_Printf("MN_ParseOption: node with bad option definition ignored (node \"%s\", menu %s)\n", node->name, node->menu->name);
		return qfalse;
	}

	if (mn.numSelectBoxes >= MAX_SELECT_BOX_OPTIONS) {
		FS_SkipBlock(text);
		Com_Printf("MN_ParseOption: Too many option entries for node %s (menu %s)\n", node->name, node->menu->name);
		return qfalse;
	}

	/**
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

	*token = COM_EParse(text, errhead, node->name);
	if (!*text)
		return qfalse;

	do {
		const value_t *property;
		int result;

		if (*token[0] == '}')
			break;

		property = MN_FindPropertyByName(selectBoxValues, *token);
		if (!property) {
			Com_Printf("MN_ParseOption: unknown options property: '%s' - ignore it\n", *token);
			return qfalse;
		}

		/* get parameter values */
		result = MN_ParseProperty(&mn.menuSelectBoxes[mn.numSelectBoxes], property, mn.menuSelectBoxes[mn.numSelectBoxes].id, text, token);
		if (!result) {
			Com_Printf("MN_ParseOption: Parsing for option '%s'. See upper\n", mn.menuSelectBoxes[mn.numSelectBoxes].id);
			return qfalse;
		}

	} while (**token != '}');
	MN_NodeAddOption(node);
	return qtrue;
}

static qboolean MN_ParseExcludeRect (menuNode_t * node, const char **text, const char **token, const char *errhead)
{
	excludeRect_t rect;

	/* get parameters */
	*token = COM_EParse(text, errhead, node->name);
	if (!*text)
		return qfalse;
	if (**token != '{') {
		Com_Printf("MN_ParseExcludeRect: node with bad excluderect ignored (node \"%s\", menu %s)\n", node->name, node->menu->name);
		return qtrue;
	}

	do {
		*token = COM_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;
		if (!Q_strcmp(*token, "pos")) {
			*token = COM_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;
			Com_EParseValue(&rect, *token, V_POS, offsetof(excludeRect_t, pos), sizeof(vec2_t));
		} else if (!Q_strcmp(*token, "size")) {
			*token = COM_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;
			Com_EParseValue(&rect, *token, V_POS, offsetof(excludeRect_t, size), sizeof(vec2_t));
		}
	} while (**token != '}');


	if (node->excludeRectNum >= MAX_EXLUDERECTS) {
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

	/* Com_Printf("  %s\n", *token); */

	/* add new actions to end of list */
	action = (menuAction_t **) ((byte *) node + event->ofs);
	for (; *action; action = &(*action)->next) {}

	/* get the action body */
	*token = COM_EParse(text, errhead, node->name);
	if (!*text)
		return qfalse;

	if (**token == '{') {
		*action = MN_ParseAction(node, text, token);
		/* get next token */
		*token = COM_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;
	} else {
		/** @todo dummy action, look at if its realy need */
		if (mn.numActions >= MAX_MENUACTIONS)
			Sys_Error("MN_ParseEventProperty: MAX_MENUACTIONS exceeded (%i)\n", mn.numActions);
		*action = &mn.menuActions[mn.numActions++];
		memset(*action, 0, sizeof(**action));
	}
	return qtrue;
}

/**
 * @brief Parse a property value
 * @todo dont read the next token (need to change the script language)
 */
static qboolean MN_ParseProperty (void* object, const value_t *property, const char* objectName, const char **text, const char **token)
{
	const char *errhead = "MN_ParseProperty: unexpected end of file (object";
	size_t bytes;
	int result;
	const int specialType = property->type & V_SPECIAL_TYPE;
	qboolean haveReadNextToken = qfalse;

	if (property->type == V_NULL) {
		return qfalse;
	}

	switch (specialType) {
	case 0:	/* common type */

		*token = COM_EParse(text, errhead, objectName);
		if (!*text)
			return qfalse;

		if (property->type == V_TRANSLATION_MANUAL_STRING) {
			/* selectbox values are static arrays */
			char *target = ((char*)object + property->ofs);
			const char *translateableToken = *token;
			assert(property->size);
			if (translateableToken[0] == '_')
				translateableToken++;
			Q_strncpyz(target, translateableToken, property->size);
		} else {
			result = Com_ParseValue(object, *token, property->type, property->ofs, property->size, &bytes);
			if (result != RESULT_OK) {
				Com_Printf("Invalid value for property '%s': %s\n", property->string, Com_GetError());
				return qfalse;
			}
		}
		break;

	case V_SPECIAL_REF:
		*token = COM_EParse(text, errhead, objectName);
		if (!*text)
			return qfalse;

		/* a reference to data is handled like this */
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
			Com_Printf("MN_ParseProperty: Invalid value for property '%s': %s\n", property->string, Com_GetError());
			return qfalse;
		}
		mn.curadata += bytes;

		break;

	case V_SPECIAL_CVAR:	/* common type */
		*token = COM_EParse(text, errhead, objectName);
		if (!*text)
			return qfalse;

		/* a reference to data is handled like this */
		*(byte **) ((byte *) object + property->ofs) = mn.curadata;
		/* references are parsed as string */
		if (**token == '*') {
			/* sanity check */
			if (strlen(*token) > MAX_VAR - 1) {
				Com_Printf(": Value '%s' is too long (key %s)\n", *token, property->string);
				return qfalse;
			}

			result = Com_ParseValue(mn.curadata, *token, V_STRING, 0, 0, &bytes);
			if (result != RESULT_OK) {
				Com_Printf("Invalid value for property '%s': %s\n", property->string, Com_GetError());
				return qfalse;
			}
			mn.curadata += bytes;
		} else {
			/* sanity check */
			if ((property->type & V_BASETYPEMASK) == V_STRING && strlen(*token) > MAX_VAR - 1) {
				Com_Printf("MN_ParseProperty: Value '%s' is too long (key %s)\n", *token, property->string);
				return qfalse;
			}

			result = Com_ParseValue(mn.curadata, *token, property->type & V_BASETYPEMASK, 0, property->size, &bytes);
			if (result != RESULT_OK) {
				Com_Printf("MN_ParseProperty: Invalid value for property '%s': %s\n", property->string, Com_GetError());
				return qfalse;
			}
			mn.curadata += bytes;
		}
		break;

	case V_SPECIAL:

		switch (property->type) {
		case V_SPECIAL_ACTION:
			result = MN_ParseEventProperty(object, property, text, token, errhead);
			if (!result)
				return qfalse;
			haveReadNextToken = qtrue;
			break;

		case V_SPECIAL_EXCLUDERECT:
			result = MN_ParseExcludeRect(object, text, token, errhead);
			if (!result)
				return qfalse;
			break;

		case V_SPECIAL_OPTIONNODE:
			result = MN_ParseOption(object, text, token, errhead);
			if (!result)
				return qfalse;
			break;

		case V_SPECIAL_ICONREF:
			{
				menuIcon_t** icon = (menuIcon_t**) ((byte *) object + property->ofs);
				*token = COM_EParse(text, errhead, objectName);
				if (!*text)
					return qfalse;
				*icon = MN_GetIconByName(*token);
				if (*icon == NULL) {
					Com_Printf("MN_ParseProperty: icon '%s' not found (object %s)\n", *token, objectName);
				}
			}
			break;

		case V_SPECIAL_IF:
			{
				menuDepends_t **condition = (menuDepends_t **) ((byte *) object + property->ofs);
				qboolean r;

				*token = COM_EParse(text, errhead, objectName);
				if (!*text)
					return qfalse;

				*condition = MN_AllocCondition(*token);
				if (!r)
					return qfalse;
			}
			break;

		default:
			Com_Printf("MN_ParseProperty: unknown property type '%d' (0x%X) (%s@%s)\n", property->type, property->type, objectName, property->string);
			return qfalse;
		}
		break;

	default:
		Com_Printf("MN_ParseNodeBody: unknown property type '%d' (0x%X) (%s@%s)\n", property->type, property->type, objectName, property->string);
		return qfalse;
	}

	if (!haveReadNextToken) {
		*token = COM_EParse(text, errhead, objectName);
		if (!*text)
			return qfalse;
	}

	return qtrue;
}

/**
 * @sa MN_ParseMenuBody
 */
static qboolean MN_ParseNodeBody (menuNode_t * node, const char **text, const char **token)
{
	const char *errhead = "MN_ParseNodeBody: unexpected end of file (node";
	qboolean nextTokenAlreadyRead = qfalse;

	/* functions are a special case */
	if (node->behaviour->isFunction) {
		menuAction_t **action;

		/* add new actions to end of list */
		action = &node->onClick;
		for (; *action; action = &(*action)->next) {}

		if (mn.numActions >= MAX_MENUACTIONS)
			Sys_Error("MN_ParseNodeBody: MAX_MENUACTIONS exceeded (%i)\n", mn.numActions);
		*action = &mn.menuActions[mn.numActions++];
		memset(*action, 0, sizeof(**action));

		*action = MN_ParseAction(node, text, token);
		return *token[0] == '}';
	}

	do {
		const value_t *val;
		int result;

		/* get new token */
		if (!nextTokenAlreadyRead) {
			*token = COM_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;
		} else {
			nextTokenAlreadyRead = qfalse;
		}

		/* is finished */
		if (**token == '}') {
			return qtrue;
		}

		/* find the property */

		val = MN_NodeGetPropertyDefinition(node, *token);

		if (!val) {
			/* unknown token, print message and continue */
			Com_Printf("MN_ParseNodeBody: unknown property \"%s\", node ignored (node %s.%s)\n", *token, node->menu->name, node->name);
			return qfalse;
		}

		/** @todo check if its realy need */
		node->scriptValues = val;

		/* get parameter values */
		result = MN_ParseProperty(node, val, node->name, text, token);
		if (!result) {
			Com_Printf("MN_ParseNodeBody: Problem with parsing of node property '%s.%s@%s'. See upper\n", node->menu->name, node->name, val->string);
			return qfalse;
		}

		nextTokenAlreadyRead = qtrue;


	} while (*text);

	return qfalse;
}

/**
 * @brief parse a node and complet the menu with it
 * @sa MN_ParseMenuBody
 * @sa MN_ParseNodeBody
 * @todo we can think about merging MN_ParseNodeBody here
 * @node first token already read
 * @node dont read more than the need token (last right token is '}' of end of node)
 */
static qboolean MN_ParseNode (menu_t * menu, const char **text, const char **token, const char *errhead)
{
	menuNode_t *node;
	nodeBehaviour_t *behaviour;
	qboolean result;

	/* allow to begin with the identifier "node" before the behaviour name */
	if (!Q_strcasecmp(*token, "node")) {
		*token = COM_EParse(text, errhead, menu->name);
		if (!*text)
			return qfalse;
	}

	/* get the behaviour */
	behaviour = MN_GetNodeBehaviour(*token);
	if (behaviour == NULL) {
		Com_Printf("MN_ParseNode: node behaviour '%s' doesn't exist (menu \"%s\")\n", *token, menu->name);
		return qfalse;
	}

	/* get the name */
	*token = COM_EParse(text, errhead, menu->name);
	if (!*text)
		return qfalse;

	/* test if node already exists */
	node = MN_GetNode(menu, *token);
	if (node) {
		if (node->behaviour != behaviour) {
			Com_Printf("MN_ParseNode: we can't change node type (node \"%s.%s\")\n", menu->name, node->name);
			return qfalse;
		}
		Com_DPrintf(DEBUG_CLIENT, "... over-riding node %s.%s\n", menu->name, node->name);
		/* reset action list of node */
		node->onClick = NULL;	/**< @todo understand why this strange hack (there is a lot of over actions) */

	/* else initialize node */
	} else {
		node = MN_AllocNode(behaviour->name);
		node->menu = menu;
		Q_strncpyz(node->name, *token, sizeof(node->name));
		MN_AppendNode(menu, node);
	}

	/* node default values */
	/** @todo move it into the respective "loading" function (for those nodes, that need it) */
	node->padding = 3;

	/* throw to the node, we begin to read attributes */
	if (node->behaviour->loading)
		node->behaviour->loading(node);

	/* get body */
	*token = COM_EParse(text, errhead, menu->name);
	if (!*text)
		return qfalse;

	/* node without body is out of spec */
	if (*token[0] != '{') {
		Com_Printf("MN_ParseNode: node dont have body, token '%s' read (node \"%s.%s\")\n", *token, menu->name, node->name);
		mn.numNodes--;
		return qfalse;
	}

	result = MN_ParseNodeBody(node, text, token);
	if (!result) {
		Com_Printf("MN_ParseNode: node with bad body ignored (node \"%s.%s\")\n", menu->name, node->name);
		mn.numNodes--;
		return qfalse;
	}

	/* already check on MN_ParseNodeBody */
	assert(*token[0] == '}');

	/* init the node according to his behaviour */
	if (node->behaviour->loaded) {
		node->behaviour->loaded(node);
	}
	return qtrue;
}

static qboolean MN_ParseMenuProperties (menu_t * menu, const char **text, const char **token, const char *errhead)
{
	const nodeBehaviour_t* menuBehaviour = MN_GetNodeBehaviour("menu");

	/* there are no menu properties */
	if (*token[0] != '{') {
		return qtrue;
	}

	/* get new token */
	*token = COM_EParse(text, errhead, menu->name);
	if (!*text)
		return qfalse;

	while (*token[0] != '}') {
		const value_t* property;
		int result;
		size_t bytes;

		property = MN_FindPropertyByName(menuBehaviour->properties, *token);
		if (!property) {
			Com_Printf("Unknown menu property '%s' find\n", *token);
			return qfalse;
		}

		/* get new token */
		*token = COM_EParse(text, errhead, menu->name);
		if (!*text)
			return qfalse;

		/* check value */
		result = Com_ParseValue(menu, *token, property->type, property->ofs, property->size, &bytes);
		if (result != RESULT_OK) {
			Com_Printf("Invalid value for property '%s': %s\n", property->string, Com_GetError());
			return qfalse;
		}

		/* get new token */
		*token = COM_EParse(text, errhead, menu->name);
		if (!*text)
			return qfalse;
	}

	/* get new token */
	*token = COM_EParse(text, errhead, menu->name);
	if (!*text)
		return qfalse;

	return qtrue;
}

/**
 * @sa MN_ParseNodeBody
 * @todo merge it with MN_ParseMenu
 */
static qboolean MN_ParseMenuBody (menu_t * menu, const char **text)
{
	const char *errhead = "MN_ParseMenuBody: unexpected end of file (menu";
	const char *token;
	qboolean result;

	Vector2Set(menu->pos, 0, 0);

	/* get new token */
	token = COM_EParse(text, errhead, menu->name);
	if (!*text)
		return qfalse;

	/* parse special menu values */
	result = MN_ParseMenuProperties(menu, text, &token, errhead);
	if (!result)
		return qfalse;

	/* for each nodes */
	while (token[0] != '}') {
		result = MN_ParseNode(menu, text, &token, errhead);
		if (!result)
			return qfalse;

		token = COM_EParse(text, errhead, menu->name);
		if (!*text)
			return qfalse;
	}

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
		if (!Q_strcmp(mn.menuModels[i].id, name)) {
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

	menuModel->id = Mem_PoolStrDup(name, cl_menuSysPool, CL_TAG_MENU);
	Com_DPrintf(DEBUG_CLIENT, "Found menu model %s (%i)\n", menuModel->id, mn.numMenuModels);

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("MN_ParseMenuModel: menu \"%s\" without body ignored\n", menuModel->id);
		return;
	}

	mn.numMenuModels++;

	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (v = menuModelValues; v->string; v++)
			if (!Q_strncmp(token, v->string, sizeof(v->string))) {
				/* found a definition */
				if (!Q_strncmp(v->string, "need", 4)) {
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;
					menuModel->next = MN_GetMenuModel(token);
					if (!menuModel->next)
						Com_Printf("Could not find menumodel %s", token);
					menuModel->need = Mem_PoolStrDup(token, cl_menuSysPool, CL_TAG_MENU);
				} else if (!Q_strncmp(v->string, "menutransform", 13)) {
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;
					if (*token != '{') {
						Com_Printf("Error in menumodel '%s' menutransform definition\n", name);
						break;
					}
					do {
						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						if (*token == '}')
							break;
						menuModel->menuTransform[menuModel->menuTransformCnt].menuID = Mem_PoolStrDup(token, cl_menuSysPool, CL_TAG_MENU);

						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						if (*token == '}') {
							Com_Printf("Error in menumodel '%s' menutransform definition - missing scale float value\n", name);
							break;
						}
						if (*token == '#') {
							menuModel->menuTransform[menuModel->menuTransformCnt].useScale = qfalse;
						} else {
							Com_EParseValue(&menuModel->menuTransform[menuModel->menuTransformCnt].scale, token, V_VECTOR, 0, sizeof(vec3_t));
							menuModel->menuTransform[menuModel->menuTransformCnt].useScale = qtrue;
						}

						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						if (*token == '}') {
							Com_Printf("Error in menumodel '%s' menutransform definition - missing angles float value\n", name);
							break;
						}
						if (*token == '#') {
							menuModel->menuTransform[menuModel->menuTransformCnt].useAngles = qfalse;
						} else {
							Com_EParseValue(&menuModel->menuTransform[menuModel->menuTransformCnt].angles, token, V_VECTOR, 0, sizeof(vec3_t));
							menuModel->menuTransform[menuModel->menuTransformCnt].useAngles = qtrue;
						}

						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						if (*token == '}') {
							Com_Printf("Error in menumodel '%s' menutransform definition - missing origin float value\n", name);
							break;
						}
						if (*token == '#') {
							menuModel->menuTransform[menuModel->menuTransformCnt].useOrigin = qfalse;
						} else {
							Com_EParseValue(&menuModel->menuTransform[menuModel->menuTransformCnt].origin, token, V_VECTOR, 0, sizeof(vec3_t));
							menuModel->menuTransform[menuModel->menuTransformCnt].useOrigin = qtrue;
						}

						menuModel->menuTransformCnt++;
					} while (*token != '}'); /* dummy condition - break is earlier here */
				} else {
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;

					switch (v->type) {
					case V_CLIENT_HUNK_STRING:
						Mem_PoolStrDupTo(token, (char**) ((char*)menuModel + (int)v->ofs), cl_menuSysPool, CL_TAG_MENU);
						break;
					default:
						Com_EParseValue(menuModel, token, v->type, v->ofs, v->size);
					}
				}
				break;
			}

		if (!v->string)
			Com_Printf("MN_ParseMenuModel: unknown token \"%s\" ignored (menu_model %s)\n", token, name);

	} while (*text);
}

void MN_ParseIcon (const char *name, const char **text)
{
	menuIcon_t *icon;
	const char *token;

	/* search for menus with same name */
	icon = MN_AllocIcon(name);

	/* get it's body */
	token = COM_Parse(text);
	assert(Q_strcmp(token, "{") == 0);

	/** @todo While MN_ParseProperty read the next token */
	token = COM_Parse(text);
	if (*text == NULL)
		return;

	/* read properties */
	while (qtrue) {
		const value_t *property;
		qboolean result;

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
 * @sa CL_ParseClientData
 */
void MN_ParseMenu (const char *name, const char **text)
{
	menu_t *menu;
	menuNode_t *node;
	const char *token;
	int i;

	/* search for menus with same name */
	for (i = 0; i < mn.numMenus; i++)
		if (!Q_strncmp(name, mn.menus[i].name, MAX_VAR))
			break;

	if (i < mn.numMenus) {
		Com_Printf("MN_ParseMenus: menu \"%s\" with same name found, second ignored\n", name);
	}

	if (mn.numMenus >= MAX_MENUS) {
		Sys_Error("MN_ParseMenu: max menus exceeded (%i) - ignore '%s'\n", MAX_MENUS, name);
		return;	/* never reached */
	}

	/* initialize the menu */
	menu = &mn.menus[mn.numMenus++];
	memset(menu, 0, sizeof(*menu));

	Q_strncpyz(menu->name, name, sizeof(menu->name));

	/** @todo waithing for the merge of menu and node */
	/* menuBehaviour->loading(menu); */
	MN_WindowNodeLoading(menu);

	/* get it's body */
	token = COM_Parse(text);

	/* does this menu inherit data from another menu? */
	if (!Q_strncmp(token, "extends", 7)) {
		menu_t *superMenu;
		menuNode_t *newNode;

		token = COM_Parse(text);
		Com_DPrintf(DEBUG_CLIENT, "MN_ParseMenus: menu \"%s\" inheriting menu \"%s\"\n", name, token);
		superMenu = MN_GetMenu(token);
		if (!superMenu)
			Sys_Error("MN_ParseMenu: menu '%s' can't inherit from menu '%s' - because '%s' was not found\n", name, token, token);
		*menu = *superMenu;
		menu->super = superMenu;
		Q_strncpyz(menu->name, name, sizeof(menu->name));

		/* start a new list */
		menu->firstChild = NULL;
		menu->lastChild = NULL;

		/* clone all super menu's nodes */
		for (node = superMenu->firstChild; node; node = node->next) {
			if (mn.numNodes >= MAX_MENUNODES)
				Sys_Error("MAX_MENUNODES exceeded\n");

			newNode = MN_CloneNode(node, menu, qtrue);
			newNode->super = node;
			MN_AppendNode(menu, newNode);

			/* update special links */
			if (superMenu->renderNode == node)
				menu->renderNode = newNode;
			else if (superMenu->popupNode == node)
				menu->popupNode = newNode;
			else if (superMenu->onTimeOut == node->onClick) {
				menu->onTimeOut = newNode->onClick;
				menu->eventNode = newNode;
			}
		}

		token = COM_Parse(text);
	}

	if (!*text || *token != '{') {
		Com_Printf("MN_ParseMenu: menu \"%s\" without body ignored\n", menu->name);
		mn.numMenus--;
		return;
	}

	/* parse it's body */
	if (!MN_ParseMenuBody(menu, text)) {
		Sys_Error("MN_ParseMenu: menu \"%s\" have a bad body\n", menu->name);
		return;	/* never reached */
	}

	/** @todo waithing for the merge of menu and node */
	/* menuBehaviour->loaded(menu); */
	MN_WindowNodeLoaded(menu);
}

/**
 * @sa COM_MacroExpandString
 */
const char *MN_GetReferenceString (const menu_t* const menu, const char *ref)
{
	if (!ref)
		return NULL;
	if (*ref == '*') {
		char ident[MAX_VAR];
		const char *text, *token;
		char command[MAX_VAR];
		char param[MAX_VAR];

		/* get the reference and the name */
		text = COM_MacroExpandString(ref);
		if (text)
			return text;

		text = ref + 1;
		token = COM_Parse(&text);
		if (!text)
			return NULL;
		Q_strncpyz(ident, token, sizeof(ident));
		token = COM_Parse(&text);
		if (!text)
			return NULL;

		if (!Q_strncmp(ident, "binding", 7)) {
			/* get the cvar value */
			if (*text && *text <= ' ') {
				/* check command and param */
				Q_strncpyz(command, token, sizeof(command));
				token = COM_Parse(&text);
				Q_strncpyz(param, token, sizeof(param));
				/*Com_sprintf(token, MAX_VAR, "%s %s", command, param);*/
			}
			return Key_GetBinding(token, (cls.state != ca_active ? KEYSPACE_MENU : KEYSPACE_GAME));
		} else {
			menuNode_t *refNode;
			const value_t *val;

			/* draw a reference to a node property */
			refNode = MN_GetNode(menu, ident);
			if (!refNode)
				return NULL;

			/* get the property */
			val = MN_NodeGetPropertyDefinition(refNode, token);
			if (!val)
				return NULL;

			/* get the string */
			return Com_ValueToStr(refNode, val->type & V_BASETYPEMASK, val->ofs);
		}
	} else if (*ref == '_') {
		ref++;
		return _(ref);
	} else {
		/* just get the data */
		return ref;
	}
}

float MN_GetReferenceFloat (const menu_t* const menu, void *ref)
{
	if (!ref)
		return 0.0;
	if (*(char *) ref == '*') {
		char ident[MAX_VAR];
		const char *token, *text;

		/* get the reference and the name */
		text = (char *) ref + 1;
		token = COM_Parse(&text);
		if (!text)
			return 0.0;
		Q_strncpyz(ident, token, sizeof(ident));
		token = COM_Parse(&text);
		if (!text)
			return 0.0;

		if (!Q_strncmp(ident, "cvar", 4)) {
			/* get the cvar value */
			return Cvar_VariableValue(token);
		} else {
			menuNode_t *refNode;
			const value_t *val;

			/* draw a reference to a node property */
			refNode = MN_GetNode(menu, ident);
			if (!refNode)
				return 0.0;

			/* get the property */
			val = MN_NodeGetPropertyDefinition(refNode, token);
			if (!val || val->type != V_FLOAT)
				return 0.0;

			/* get the string */
			return *(float *) ((byte *) refNode + val->ofs);
		}
	} else {
		/* just get the data */
		return *(float *) ref;
	}
}

/**
 * @brief Checks the parsed menus for errors
 * @return false if there are errors - true otherwise
 */
qboolean MN_ScriptSanityCheck (void)
{
	return qtrue;
}
