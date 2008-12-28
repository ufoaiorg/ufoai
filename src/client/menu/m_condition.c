/**
 * @file m_condition.c
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

#include "m_condition.h"

static const char *const if_strings[] = {
	"==",
	"<=",
	">=",
	">",
	"<",
	"!=",
	"",
	"eq",
	"ne"
};
CASSERT(lengthof(if_strings) == IF_SIZE);

/**
 * @brief Check the if conditions for a given node
 * @returns True is the condition is qfalse if the node is not drawn due to not meet if conditions
 */
qboolean MN_CheckCondition (menuDepends_t *condition)
{
	if (!condition->var)
		return qtrue;

	/** @todo we can't update if at run time, is Q_strcmp really need? */
	if (!condition->cvar || Q_strcmp(condition->cvar->name, condition->var)) {
		condition->cvar = Cvar_Get(condition->var, condition->value ? condition->value : "", 0, "Menu if condition cvar");
	}

	assert(condition->cvar);

	/* menuIfCondition_t */
	switch (condition->cond) {
	case IF_EQ:
		assert(condition->value);
		if (atof(condition->value) != condition->cvar->value)
			return qfalse;
		break;
	case IF_LE:
		assert(condition->value);
		if (condition->cvar->value > atof(condition->value))
			return qfalse;
		break;
	case IF_GE:
		assert(condition->value);
		if (condition->cvar->value < atof(condition->value))
			return qfalse;
		break;
	case IF_GT:
		assert(condition->value);
		if (condition->cvar->value <= atof(condition->value))
			return qfalse;
		break;
	case IF_LT:
		assert(condition->value);
		if (condition->cvar->value >= atof(condition->value))
			return qfalse;
		break;
	case IF_NE:
		assert(condition->value);
		if (condition->cvar->value == atof(condition->value))
			return qfalse;
		break;
	case IF_EXISTS:
		assert(condition->cvar->string);
		if (!*condition->cvar->string)
			return qfalse;
		break;
	case IF_STR_EQ:
		assert(condition->value);
		assert(condition->cvar->string);
		if (Q_strncmp(condition->cvar->string, condition->value, MAX_VAR))
			return qfalse;
		break;
	case IF_STR_NE:
		assert(condition->value);
		assert(condition->cvar->string);
		if (!Q_strncmp(condition->cvar->string, condition->value, MAX_VAR))
			return qfalse;
		break;
	default:
		Sys_Error("Unknown condition for if statement: %i\n", condition->cond);
		break;
	}

	return qtrue;
}

/**
 * @brief Translate the condition string to menuIfCondition_t enum value
 * @param[in] conditionString The string from scriptfiles (see if_strings)
 * @return menuIfCondition_t value
 * @return enum value for condition string
 * @note Produces a Sys_Error if conditionString was not found in if_strings array
 */
int Com_ParseConditionType (const char* conditionString, const char *token)
{
	int i;
	for (i = 0; i < IF_SIZE; i++) {
		if (!Q_strncmp(if_strings[i], conditionString, 2)) {
			return i;
		}
	}
	Sys_Error("Invalid if statement with condition '%s' token: '%s'\n", conditionString, token);
	return -1;
}
