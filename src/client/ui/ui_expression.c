/**
 * @file ui_expression.c
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

#include "ui_expression.h"
#include "ui_main.h"
#include "ui_internal.h"
#include "ui_parse.h"
#include "ui_actions.h"
#include "node/ui_node_abstractnode.h"
#include "../../shared/parse.h"
#include "../../shared/shared.h"

/**
 * @brief Get a node and a property from an expression
 * @param expression Expression tree to analyse
 * @param context Call context
 * @param[out] property A node property
 * @return A node (else NULL, if no node found) and a property (else NULL if no/wrong property defined)
 */
uiNode_t* UI_GetNodeFromExpression (uiAction_t *expression, const uiCallContext_t *context, const value_t **property)
{
	if (property != NULL)
		*property = NULL;

	switch (expression->type & EA_HIGHT_MASK) {
	case EA_VALUE:
		switch (expression->type) {
		case EA_VALUE_VAR:
			{
				uiValue_t *variable =  UI_GetVariable(context, expression->d.terminal.d1.integer);
				switch (variable->type) {
				case EA_VALUE_NODE:
					return variable->value.node;
				default:
					break;
				}
			}
			break;

		case EA_VALUE_PATHNODE:
		case EA_VALUE_PATHNODE_WITHINJECTION:
		{
			uiNode_t *node;
			const value_t *propertyTmp;
			const char *path = expression->d.terminal.d1.constString;
			if (expression->type == EA_VALUE_PATHNODE_WITHINJECTION)
				path = UI_GenInjectedString(path, qfalse, context);

			UI_ReadNodePath(path, context->source, &node, &propertyTmp);
			if (!node) {
				Com_Printf("UI_GetNodeFromExpression: Node '%s' wasn't found; NULL returned\n", path);
				return NULL;
			}
			if (propertyTmp != NULL)
				Com_Printf("UI_GetNodeFromExpression: No property expected, but path '%s' contain a property. Property ignored.\n", path);

			return node;
		}

		case EA_VALUE_PATHPROPERTY:
		case EA_VALUE_PATHPROPERTY_WITHINJECTION:
			{
				uiNode_t *node;
				const value_t *propertyTmp;
				const char *path = expression->d.terminal.d1.constString;
				if (expression->type == EA_VALUE_PATHPROPERTY_WITHINJECTION)
					path = UI_GenInjectedString(path, qfalse, context);

				UI_ReadNodePath(path, context->source, &node, &propertyTmp);
				if (!node) {
					Com_Printf("UI_GetNodeFromExpression: Node '%s' wasn't found; NULL returned\n", path);
					return NULL;
				}
				if (property == NULL) {
					if (propertyTmp != NULL)
						Com_Printf("UI_GetNodeFromExpression: No property expected, but path '%s' contain a property. Property ignored.\n", path);
				} else {
					 *property = propertyTmp;
				}

				return node;
			}

		case EA_VALUE_THIS:
			return context->source;

		case EA_VALUE_PARENT:
			return context->source->parent;

		case EA_VALUE_WINDOW:
			return context->source->root;

		default:
			break;
		}

	case EA_OPERATOR_UNARY:
		switch (expression->type) {
		case EA_OPERATOR_PATHPROPERTYFROM:
			{
				uiNode_t *relativeTo = UI_GetNodeFromExpression(expression->d.nonTerminal.left, context, NULL);
				uiNode_t *node;
				const value_t *propertyTmp;
				const char* path = expression->d.terminal.d2.constString;
				UI_ReadNodePath(path, relativeTo, &node, &propertyTmp);
				if (!node) {
					Com_Printf("UI_GetNodeFromExpression: Path '%s' from node '%s' found no node; NULL returned\n", path, UI_GetPath(relativeTo));
					return NULL;
				}
				if (property == NULL) {
					if (propertyTmp != NULL)
						Com_Printf("UI_GetNodeFromExpression:  No property expected, but path '%s' from node '%s' found no node; NULL returned\n", path, UI_GetPath(relativeTo));
				} else {
					*property = propertyTmp;
				}
				return node;
			}
		default:
			break;
		}

	default:
		break;
	}

	return NULL;
}


/**
 * @return A float value, else 0
 */
float UI_GetFloatFromExpression (uiAction_t *expression, const uiCallContext_t *context)
{
	switch (expression->type & EA_HIGHT_MASK) {
	case EA_VALUE:
		switch (expression->type) {
		case EA_VALUE_VAR:
			{
				uiValue_t *variable =  UI_GetVariable(context, expression->d.terminal.d1.integer);
				switch (variable->type) {
				case EA_VALUE_STRING:
					if (variable->value.string == NULL) {
						Com_Printf("UI_GetFloatFromExpression: String variable not initialized. '0' returned");
						return 0;
					}
					return atof(variable->value.string);
				case EA_VALUE_FLOAT:
					return variable->value.number;
				case EA_VALUE_CVAR:
					{
						cvar_t *cvar = variable->value.cvar;
						if (cvar == NULL) {
							Com_Printf("UI_GetFloatFromExpression: Cvar variable not initialized. '0' returned");
							return 0;
						}
						return cvar->value;
					}
				default:
					Com_Printf("UI_GetFloatFromExpression: Unsupported variable type: %i. '0' returned", variable->type);
					return 0;
				}
			}
		case EA_VALUE_STRING:
		case EA_VALUE_STRING_WITHINJECTION:
			{
				const char* string = expression->d.terminal.d1.constString;
				if (expression->type == EA_VALUE_STRING_WITHINJECTION)
					string = UI_GenInjectedString(string, qfalse, context);
				return atof(string);
			}
		case EA_VALUE_FLOAT:
			return expression->d.terminal.d1.number;
		case EA_VALUE_CVARNAME:
		case EA_VALUE_CVARNAME_WITHINJECTION:
			{
				cvar_t *cvar = NULL;
				const char *cvarName = expression->d.terminal.d1.constString;
				if (expression->type == EA_VALUE_CVARNAME_WITHINJECTION)
					cvarName = UI_GenInjectedString(cvarName, qfalse, context);
				cvar = Cvar_Get(cvarName, "", 0, "Cvar from UI script expression");
				return cvar->value;
			}
		case EA_VALUE_PATHPROPERTY:
		case EA_VALUE_PATHPROPERTY_WITHINJECTION:
			{
				uiNode_t *node;
				const value_t *property;
				node = UI_GetNodeFromExpression(expression, context, &property);
				if (!node) {
					Com_Printf("UI_GetFloatFromParam: Node wasn't found; '0'\n");
					return 0;
				}
				if (!property) {
					Com_Printf("UI_GetFloatFromParam: Property wasn't found; '0' returned\n");
					return 0;
				}
				return UI_GetFloatFromNodeProperty(node, property);
			}
		case EA_VALUE_PARAM:
		{
			const int paramId = expression->d.terminal.d1.integer;
			const char *string = UI_GetParam(context, paramId);
			if (string[0] == '\0') {
				Com_Printf("UI_GetFloatFromParam: Param '%i' is out of range, or empty; '0' returned\n", paramId);
				return 0;
			}
			return atof(string);
		}
		case EA_VALUE_PARAMCOUNT:
			return UI_GetParamNumber(context);
		}
		break;

	case EA_OPERATOR_FLOAT2FLOAT:
		{
			const float value1 = UI_GetFloatFromExpression(expression->d.nonTerminal.left, context);
			const float value2 = UI_GetFloatFromExpression(expression->d.nonTerminal.right, context);

			switch (expression->type) {
			case EA_OPERATOR_ADD:
				return value1 + value2;
			case EA_OPERATOR_SUB:
				return value1 - value2;
			case EA_OPERATOR_MUL:
				return value1 * value2;
			case EA_OPERATOR_DIV:
				if (value2 == 0) {
					Com_Printf("UI_GetFloatFromExpression: Div by 0. '0' returned");
					return 0;
				} else
					return value1 / value2;
			case EA_OPERATOR_MOD:
				{
					const int v1 = value1;
					const int v2 = value2;
					/** @todo do we have some check to do? */
					return v1 % v2;
				}
			}
		}
		break;

	case EA_OPERATOR_UNARY:
		switch (expression->type) {
		case EA_OPERATOR_PATHPROPERTYFROM:
		{
			uiNode_t *node;
			const value_t *property;
			node = UI_GetNodeFromExpression(expression, context, &property);
			return UI_GetFloatFromNodeProperty(node, property);
		}
		default:
			Com_Error(ERR_FATAL, "UI_GetFloatFromExpression: (EA_OPERATOR_UNARY) Invalid expression type %i", expression->type);
		}

	}

	Com_Printf("UI_GetFloatFromExpression: Unsupported expression type: %i. '0' returned", expression->type);
	return 0;
}

/**
 * @return A string, else an empty string
 * @todo this should not work very well, because too much va are used
 * then we should locally cache values, or manage a temporary string structure
 */
const char* UI_GetStringFromExpression (uiAction_t *expression, const uiCallContext_t *context)
{
	switch (expression->type & EA_HIGHT_MASK) {
	case EA_VALUE:
		switch (expression->type) {
		case EA_VALUE_VAR:
			{
				uiValue_t *variable =  UI_GetVariable(context, expression->d.terminal.d1.integer);
				switch (variable->type) {
				case EA_VALUE_STRING:
					if (variable->value.string == NULL) {
						Com_Printf("UI_GetStringFromExpression: String variable not initialized. Empty string returned");
						return "";
					}
					return variable->value.string;
				case EA_VALUE_FLOAT:
					{
						const float number = variable->value.number;
						const int integer = number;
						/** @todo should we add a delta? */
						if (number == integer)
							return va("%i", integer);
						else
							return va("%f", number);
					}
				case EA_VALUE_CVAR:
					{
						cvar_t *cvar = variable->value.cvar;
						if (cvar == NULL) {
							Com_Printf("UI_GetStringFromExpression: Cvar variable not initialized. Empty string returned");
							return "";
						}
						return cvar->string;
					}
				default:
					Com_Printf("UI_GetStringFromExpression: Unsupported variable type: %i. Empty string returned", variable->type);
					return "";
				}
			}
		case EA_VALUE_STRING:
		case EA_VALUE_STRING_WITHINJECTION:
			{
				const char* string = expression->d.terminal.d1.constString;
				if (expression->type == EA_VALUE_STRING_WITHINJECTION)
					string = UI_GenInjectedString(string, qfalse, context);
				return string;
			}
		case EA_VALUE_FLOAT:
		{
			const float number = expression->d.terminal.d1.number;
			const int integer = number;
			/** @todo should we add a delta? */
			if (number == integer)
				return va("%i", integer);
			else
				return va("%f", number);
		}
		case EA_VALUE_CVARNAME:
		case EA_VALUE_CVARNAME_WITHINJECTION:
		{
			cvar_t *cvar = NULL;
			const char *cvarName = expression->d.terminal.d1.constString;
			if (expression->type == EA_VALUE_CVARNAME_WITHINJECTION)
				cvarName = UI_GenInjectedString(cvarName, qfalse, context);
			cvar = Cvar_Get(cvarName, "", 0, "Cvar from UI script expression");
			return cvar->string;
		}
		case EA_VALUE_PATHPROPERTY:
		case EA_VALUE_PATHPROPERTY_WITHINJECTION:
			{
				uiNode_t *node;
				const value_t *property;
				const char* string;
				node = UI_GetNodeFromExpression(expression, context, &property);
				if (!node) {
					Com_Printf("UI_GetStringFromExpression: Node wasn't found; Empty string returned\n");
					return "";
				}
				if (!property) {
					Com_Printf("UI_GetStringFromExpression: Property wasn't found; Empty string returned\n");
					return "";
				}
				string = UI_GetStringFromNodeProperty(node, property);
				if (string == NULL) {
					Com_Printf("UI_GetStringFromExpression: String getter for '%s@%s' property do not exists; '' returned\n", node->behaviour->name, property->string);
					return "";
				}
				return string;
			}
			break;
		case EA_VALUE_PARAM:
			return UI_GetParam(context, expression->d.terminal.d1.integer);
		case EA_VALUE_PARAMCOUNT:
			return va("%i", UI_GetParamNumber(context));
		}
		break;

	case EA_OPERATOR_UNARY:
		switch (expression->type) {
		case EA_OPERATOR_PATHPROPERTYFROM:
		{
			uiNode_t *node;
			const value_t *property;
			node = UI_GetNodeFromExpression(expression, context, &property);
			return UI_GetStringFromNodeProperty(node, property);
		}
		default:
			Com_Error(ERR_FATAL, "UI_GetFloatFromExpression: (EA_OPERATOR_UNARY) Invalid expression type %i", expression->type);
		}

	case EA_OPERATOR_BOOLEAN2BOOLEAN:
	case EA_OPERATOR_FLOAT2BOOLEAN:
	case EA_OPERATOR_STRING2BOOLEAN:
		{
			const qboolean v = UI_GetBooleanFromExpression(expression, context);
			return (v)?"1":"0";
		}

	case EA_OPERATOR_FLOAT2FLOAT:
		{
			const float number = UI_GetFloatFromExpression(expression, context);
			const int integer = number;
			/** @todo should we add a delta? */
			if (number == integer)
				return va("%i", integer);
			else
				return va("%f", number);
		}
	}

	Com_Printf("UI_GetStringFromExpression: Unsupported expression type: %i", expression->type);
	return "";
}

/**
 * @brief Check if an expression is true
 * @return True if the expression is true
 */
qboolean UI_GetBooleanFromExpression (uiAction_t *expression, const uiCallContext_t *context)
{
	if (expression == NULL)
		return qfalse;

	switch (expression->type & EA_HIGHT_MASK) {
	case EA_VALUE:
		return UI_GetFloatFromExpression(expression, context) != 0;

	case EA_OPERATOR_BOOLEAN2BOOLEAN:
		{
			const qboolean value1 = UI_GetBooleanFromExpression(expression->d.nonTerminal.left, context);
			const qboolean value2 = UI_GetBooleanFromExpression(expression->d.nonTerminal.right, context);

			switch (expression->type) {
			case EA_OPERATOR_AND:
				return value1 && value2;
			case EA_OPERATOR_OR:
				return value1 || value2;
			case EA_OPERATOR_XOR:
				return value1 ^ value2;
			case EA_OPERATOR_NOT:
				return !value1;
			default:
				Com_Error(ERR_FATAL, "UI_GetBooleanFromExpression: (BOOL2BOOL) Invalid expression type");
			}
		}

	case EA_OPERATOR_FLOAT2BOOLEAN:
		{
			const float value1 = UI_GetFloatFromExpression(expression->d.nonTerminal.left, context);
			const float value2 = UI_GetFloatFromExpression(expression->d.nonTerminal.right, context);

			switch (expression->type) {
			case EA_OPERATOR_EQ:
				return value1 == value2;
			case EA_OPERATOR_LE:
				return value1 <= value2;
			case EA_OPERATOR_GE:
				return value1 >= value2;
			case EA_OPERATOR_GT:
				return value1 > value2;
			case EA_OPERATOR_LT:
				return value1 < value2;
			case EA_OPERATOR_NE:
				return value1 != value2;
			default:
				Com_Error(ERR_FATAL, "UI_GetBooleanFromExpression: (FLOAT2BOOL) Invalid expression type");
			}
		}

	case EA_OPERATOR_UNARY:
		switch (expression->type) {
		case EA_OPERATOR_EXISTS:
			{
				const uiAction_t *e = expression->d.nonTerminal.left;
				const char* cvarName;
				assert(e);
				assert(e->type == EA_VALUE_CVARNAME || e->type == EA_VALUE_CVARNAME_WITHINJECTION);
				cvarName = e->d.terminal.d1.constString;
				if (e->type == EA_VALUE_CVARNAME_WITHINJECTION)
					cvarName = UI_GenInjectedString(cvarName, qfalse, context);
				return Cvar_FindVar(cvarName) != NULL;
			}
		case EA_OPERATOR_PATHPROPERTYFROM:
			return UI_GetFloatFromExpression(expression, context) != 0;
		default:
			Com_Error(ERR_FATAL, "UI_GetBooleanFromExpression: (EA_OPERATOR_UNARY) Invalid expression type %i", expression->type);
		}

	case EA_OPERATOR_STRING2BOOLEAN:
		{
			const char* value1 = UI_GetStringFromExpression(expression->d.nonTerminal.left, context);
			const char* value2 = UI_GetStringFromExpression(expression->d.nonTerminal.right, context);

			switch (expression->type) {
			case EA_OPERATOR_STR_EQ:
				return Q_streq(value1, value2);
			case EA_OPERATOR_STR_NE:
				return !Q_streq(value1, value2);
			default:
				Com_Error(ERR_FATAL, "UI_GetBooleanFromExpression: (STRING2BOOL) Invalid expression type");
			}
		}

	default:
		Com_Error(ERR_FATAL, "UI_GetBooleanFromExpression: Unsupported expression type: %i", expression->type);
	}
}

/**
 * @brief Allocate and initialize an expression according to a string
 * @param[in] description String describing a condition
 * @return The condition if everything is ok, NULL otherwise
 */
uiAction_t *UI_AllocStaticStringCondition (const char *description)
{
	const char *text, *base;
	uiAction_t *expression;

	base = va("( %s )", description);
	text = base;
	expression = UI_ParseExpression(&text);
	if (!expression) {
		Com_Printf("UI_AllocStaticStringCondition: Parse error on expression \"%s\"\n", base);
		return NULL;
	}

	return expression;
}

/**
 * @brief Read a value from the stream and init an action with it
 * @return An initialized action else NULL
 */
static uiAction_t *UI_ParseValueExpression (const char **text)
{
	const char* token;
	uiAction_t *expression = UI_AllocStaticAction();

	token = Com_Parse(text);
	if (*text == NULL) {
		Com_Printf("UI_ParseTerminalExpression: Token expected\n");
		return NULL;
	}

	/* it is a const string (or an injection tag for compatibility) */
	if (Com_ParsedTokenIsQuoted() || token[0] == '<') {
		expression->d.terminal.d1.constString = UI_AllocStaticString(token, 0);
		if (UI_IsInjectedString(token))
			expression->type = EA_VALUE_STRING_WITHINJECTION;
		else
			expression->type = EA_VALUE_STRING;
		return expression;
	}

	/* it is a param */
	if (!Q_strncasecmp(token, "param", 5)) {
		if (!Q_strcasecmp(token, "paramcount")) {
			expression->type = EA_VALUE_PARAMCOUNT;
			return expression;
		} else if (token[5] >= '1' && token[5] <= '9') {
			int i;
			if (sscanf(token + 5, "%i", &i) == 1) {
				/* token range 1-9 already avoid 0 */
				assert(i != 0);
				/** @todo when it is possible, we must check range of param id */
				expression->type = EA_VALUE_PARAM;
				expression->d.terminal.d1.integer = i;
				return expression;
			}
		}
	}

	/* it is a cvarname */
	if (Q_strstart(token, "*cvar:")) {
		const char* cvarName = token + 6;
		expression->d.terminal.d1.constString = UI_AllocStaticString(cvarName, 0);
		if (UI_IsInjectedString(cvarName))
			expression->type = EA_VALUE_CVARNAME_WITHINJECTION;
		else
			expression->type = EA_VALUE_CVARNAME;
		return expression;
	}

	/* it is a node property or it is a OLD syntax node property */
	/** @todo We MUST remove the OLD code as fast as possible */
	if (Q_strstart(token, "*node:") || Q_strstart(token, "*")) {
		const char* path = token + 1;
		const char *propertyName;
		qboolean relativeToNode;
#if 0	/* it looks useless, an unused cache */
		const value_t *property;
#endif

		relativeToNode = Q_strstart(path, "node:");
		if (relativeToNode)
			path = path + 5;

		if (UI_IsInjectedString(path))
			expression->type = EA_VALUE_PATHPROPERTY_WITHINJECTION;
		else
			expression->type = EA_VALUE_PATHPROPERTY;
		if (!relativeToNode) {
			Com_Printf("UI_ParseExpression: Old syntax, please prefix '%s' with \"*node:root.\" \n", token);
			path = va("root.%s", path);
		}
		expression->d.terminal.d1.constString = UI_AllocStaticString(path, 0);

		/* get property name */
		propertyName = strchr(path, '@');
		if (propertyName == NULL) {
			if (expression->type == EA_VALUE_PATHPROPERTY_WITHINJECTION)
				expression->type = EA_VALUE_PATHNODE_WITHINJECTION;
			else
				expression->type = EA_VALUE_PATHNODE;
			return expression;
		}
		propertyName++;

		return expression;
	}

	/* unsigned and signed number */
	if ((token[0] >= '0' && token[0] <= '9')
		|| (token[0] == '-' && token[1] >= '0' && token[1] <= '9')) {
		/** @todo use a better check - e.g. Com_ParseValue with V_INT or V_FLOAT */
		float f = atof(token);
		expression->d.terminal.d1.number = f;
		expression->type = EA_VALUE_FLOAT;
		return expression;
	}

	/* boolean */
	if (Q_streq(token, "true")) {
		expression->d.terminal.d1.number = 1.0;
		expression->type = EA_VALUE_FLOAT;
		return expression;
	}
	if (Q_streq(token, "false")) {
		expression->d.terminal.d1.number = 0.0;
		expression->type = EA_VALUE_FLOAT;
		return expression;
	}

	Com_Error(ERR_FATAL, "UI_ParseValueExpression: Token \"%s\" unknown. String must use quotes, cvar and nodes must use prefix.\n", token);
}

uiAction_t *UI_ParseExpression (const char **text)
{
	const char* token;

	token = Com_Parse(text);
	if (*text == NULL)
		return NULL;

	if (Q_streq(token, "(")) {
		uiAction_t *expression;
		uiAction_t *e;

		e = UI_ParseExpression(text);

		token = Com_Parse(text);
		if (*text == NULL)
			return NULL;

		/* unary operator or unneed "( ... )" */
		if (Q_streq(token, ")"))
			return e;

		/* then its an operator */
		expression = UI_AllocStaticAction();
		expression->d.nonTerminal.left = e;
		expression->type = UI_GetActionTokenType(token, EA_BINARYOPERATOR);
		if (expression->type == EA_NULL) {
			Com_Printf("UI_ParseExpression: Invalid 'expression' statement. Unknown '%s' operator\n", token);
			return NULL;
		}

		e = UI_ParseExpression(text);
		expression->d.nonTerminal.right = e;

		token = Com_Parse(text);
		if (*text == NULL)
			return NULL;
		if (!Q_streq(token, ")")) {
			Com_Printf("UI_ParseExpression: Token ')' expected\n");
			return NULL;
		}

		return expression;
	} else {
		const int type = UI_GetActionTokenType(token, EA_UNARYOPERATOR);
		if (type == EA_NULL) {
			Com_UnParseLastToken();
			return UI_ParseValueExpression(text);
		} else {
			uiAction_t *expression = UI_AllocStaticAction();
			uiAction_t *e;

			e = UI_ParseExpression(text);
			expression->type = type;
			expression->d.nonTerminal.left = e;

			if (expression->type == EA_OPERATOR_EXISTS) {
				if (e->type != EA_VALUE_CVARNAME && e->type != EA_VALUE_CVARNAME_WITHINJECTION) {
					Com_Printf("UI_ParseExpression: Cvar expected, but type %d found\n", e->type);
					return NULL;
				}
			}
			return expression;
		}
	}
}
