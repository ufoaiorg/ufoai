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

#include "../client.h"
#include "m_condition.h"
#include "m_main.h"
#include "m_internal.h"
#include "m_parse.h"

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
 * @returns True if the condition is qfalse if the node is not drawn
 */
qboolean MN_CheckCondition (menuDepends_t *condition)
{
	if (!condition->var)
		return qtrue;

	/** @todo we can't update it at the run time */
#if 0	/**< this code cache the result; it make problem when we delete/create cvar */
	if (!condition->cvar || Q_strcmp(condition->cvar->name, condition->var))
#endif
		condition->cvar = Cvar_Get(condition->var, condition->value ? condition->value : "", 0, "Menu if condition cvar");


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
		if (condition->cvar->string[0] == '\0')
			return qfalse;
		break;
	case IF_STR_EQ:
		assert(condition->value);
		assert(condition->cvar->string);
		if (Q_strcmp(condition->cvar->string, condition->value))
			return qfalse;
		break;
	case IF_STR_NE:
		assert(condition->value);
		assert(condition->cvar->string);
		if (!Q_strcmp(condition->cvar->string, condition->value))
			return qfalse;
		break;
	default:
		Sys_Error("Unknown condition for if statement: %i", condition->cond);
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
static int MN_GetOperatorByName (const char* operatorName)
{
	int i;
	for (i = 0; i < IF_SIZE; i++) {
		if (!Q_strcmp(if_strings[i], operatorName)) {
			return i;
		}
	}
	return IF_INVALID;
}

#define BUF_SIZE MAX_VAR - 1
/**
 * @brief Initialize a condition according to a string
 * @param[out] condition Condition to initialize
 * @param[in] token String describing a condition
 * @return True if the condition is initialized
 */
qboolean MN_InitCondition (menuDepends_t *condition, const char *token)
{
	memset(condition, 0, sizeof(*condition));
	if (!strstr(token, " ")) {
		/* cvar exists? (not null) */
		condition->var = MN_AllocString(token, 0);
		condition->cond = IF_EXISTS;
	} else {
		char param1[BUF_SIZE + 1];
		char operator[BUF_SIZE + 1];
		char param2[BUF_SIZE + 1];
		if (sscanf(token, "%"DOUBLEQUOTE(MAX_VAR)"s %"DOUBLEQUOTE(MAX_VAR)"s %"DOUBLEQUOTE(MAX_VAR)"s", param1, operator, param2) != 3) {
			Com_Printf("MN_InitCondition: Could not parse node condition.\n");
			return qfalse;
		}

		condition->var = MN_AllocString(param1, 0);
		condition->value = MN_AllocString(param2, 0);

		condition->cond = MN_GetOperatorByName(operator);
		if (condition->cond == IF_INVALID) {
			Com_Printf("Invalid 'if' statement. Unknown '%s' operator from token: '%s'\n", operator, token);
			return qfalse;
		}
	}
	return qtrue;
}

/**
 * @brief Allocate and initialize a condition according to a string
 * @param[in] token String describing a condition
 * @param[out] condition Condition to initialize
 * @return The condition if everything is ok, NULL otherwise
 */
menuDepends_t *MN_AllocCondition (const char *description)
{
	menuDepends_t condition;
	qboolean result;

	if (mn.numConditions >= MAX_MENUCONDITIONS)
		Sys_Error("MN_AllocCondition: Too many menu conditions");

	result = MN_InitCondition(&condition, description);
	if (!result)
		return NULL;

	/* allocates memory */
	mn.menuConditions[mn.numConditions] = condition;
	mn.numConditions++;

	return &mn.menuConditions[mn.numConditions - 1];
}
