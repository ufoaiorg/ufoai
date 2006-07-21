/**
 * @file cvar.c
 * @brief Manage cvars
 *
 * cvar_t variables are used to hold scalar or string variables that can be changed or displayed at the console or prog code as well as accessed directly
 * in C code.
 * Cvars are restricted from having the same names as commands to keep this
 * interface from being ambiguous.
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "qcommon.h"

/**
 * @brief This is set each time a CVAR_USERINFO variable is changed
 * so that the client knows to send it to the server
 */
bool_t userinfo_modified;

/**
 * @brief Cvar list
 */
static cvar_t *cvar_vars;

/*
============
Cvar_InfoValidate
============
*/
static bool_t Cvar_InfoValidate(char *s)
{
	if (strstr(s, "\\"))
		return false;
	if (strstr(s, "\""))
		return false;
	if (strstr(s, ";"))
		return false;
	return true;
}

/**
 * @brief Searches for a cvar given by parameter
 * @param var_name The cvar name as string
 * @return Pointer to cvar_t struct
 * @sa Cvar_VariableString
 * @sa Cvar_SetValue
 */
static cvar_t *Cvar_FindVar(char *var_name)
{
	cvar_t *var;

	for (var = cvar_vars; var; var = var->next)
		if (!Q_strcmp(var_name, var->name))
			return var;

	return NULL;
}

/**
 * @brief Returns the float value of a cvar
 * @sa Cvar_VariableString
 * @sa Cvar_FindVar
 * @return 0 if not defined
 */
float Cvar_VariableValue(char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar(var_name);
	if (!var)
		return 0.0;
	return atof(var->string);
}


/**
 * @brief Returns the value of cvar as string
 * @sa Cvar_VariableValue
 * @sa Cvar_FindVar
 *
 * Even if the cvar does not exist this function will not return a null pointer
 * but an empty string
 */
char *Cvar_VariableString(char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar(var_name);
	if (!var)
		return "";
	return var->string;
}


/**
 * @brief Unix like tab completion for console variables
 * @param partial The beginning of the variable we try to complete
 * @sa Cmd_CompleteCommand
 * @sa Key_CompleteCommand
 */
char *Cvar_CompleteVariable(char *partial)
{
	cvar_t *cvar;
	char *match = NULL;
	int len, matches = 0;

	len = strlen(partial);

	if (!len)
		return NULL;

	/* check for exact match */
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
		if (!Q_strncmp(partial, cvar->name, sizeof(cvar->name)))
			return cvar->name;

	/* check for partial matches */
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
		if (!Q_strncmp(partial, cvar->name, len)) {
			Com_Printf("[var] %s\n", cvar->name);
			match = cvar->name;
			matches++;
		}

	return matches == 1 ? match : NULL;
}

/**
 * @brief Init or return a cvar
 *
 * If the variable already exists, the value will not be set
 * The flags will be or'ed in if the variable exists.
 */
cvar_t *Cvar_Get(char *var_name, char *var_value, int flags)
{
	cvar_t *var;

	if (flags & (CVAR_USERINFO | CVAR_SERVERINFO))
		if (!Cvar_InfoValidate(var_name)) {
			Com_Printf("invalid info cvar name\n");
			return NULL;
		}

	var = Cvar_FindVar(var_name);
	if (var) {
		var->flags |= flags;
		return var;
	}

	if (!var_value)
		return NULL;

	if (flags & (CVAR_USERINFO | CVAR_SERVERINFO))
		if (!Cvar_InfoValidate(var_value)) {
			Com_Printf("invalid info cvar value\n");
			return NULL;
		}

	var = Z_Malloc(sizeof(*var));
	var->name = CopyString(var_name);
	var->string = CopyString(var_value);
	var->modified = true;
	var->value = atof(var->string);

	/* link the variable in */
	var->next = cvar_vars;
	cvar_vars = var;

	var->flags = flags;

	return var;
}

/**
 * @brief Sets a cvar values
 * Handles writeprotection and latched cvars as expected
 * @param var_name Which cvar
 * @param value Set the cvar to the value specified by 'value'
 * @param force Force the update of the cvar
 */
cvar_t *Cvar_Set2(char *var_name, char *value, bool_t force)
{
	cvar_t *var;

	var = Cvar_FindVar(var_name);
	/* create it */
	if (!var)
		return Cvar_Get(var_name, value, 0);

	if (var->flags & (CVAR_USERINFO | CVAR_SERVERINFO))
		if (!Cvar_InfoValidate(value)) {
			Com_Printf("invalid info cvar value\n");
			return var;
		}

	if (!force) {
		if (var->flags & CVAR_NOSET) {
			Com_Printf("%s is write protected.\n", var_name);
			return var;
		}

		if (var->flags & CVAR_LATCH) {
			if (var->latched_string) {
				if (Q_strcmp(value, var->latched_string) == 0)
					return var;
				Z_Free(var->latched_string);
			} else {
				if (Q_strcmp(value, var->string) == 0)
					return var;
			}

			if (Com_ServerState()) {
				Com_Printf("%s will be changed for next game.\n", var_name);
				var->latched_string = CopyString(value);
			} else {
				var->string = CopyString(value);
				var->value = atof(var->string);
				if (!Q_strncmp(var->name, "game", MAX_VAR)) {
					FS_SetGamedir(var->string);
					FS_ExecAutoexec();
				}
			}
			return var;
		}
	} else {
		if (var->latched_string) {
			Z_Free(var->latched_string);
			var->latched_string = NULL;
		}
	}

	if (!Q_strcmp(value, var->string))
		return var;				/* not changed */

	var->modified = true;

	if (var->flags & CVAR_USERINFO)
		userinfo_modified = true;	/* transmit at next oportunity */

	Z_Free(var->string);		/* free the old value string */

	var->string = CopyString(value);
	var->value = atof(var->string);

	return var;
}

/**
 * @brief Will set the variable even if NOSET or LATCH
 */
cvar_t *Cvar_ForceSet(char *var_name, char *value)
{
	return Cvar_Set2(var_name, value, true);
}

/**
 * @brief Sets a cvar value
 * @param var_name Which cvar should be set
 * @param value Which value should the cvar get
 * @note Look after the CVAR_LATCH stuff and check for write protected cvars
 */
cvar_t *Cvar_Set(char *var_name, char *value)
{
	return Cvar_Set2(var_name, value, false);
}

/**
 * @brief Sets a cvar from console with the given flags
 * @note flags are:
 * CVAR_ARCHIVE These cvars will be saved.
 * CVAR_USERINFO Added to userinfo  when changed.
 * CVAR_SERVERINFO Added to serverinfo when changed.
 * CVAR_NOSET Don't allow change from console at all but can be set from the command line.
 * CVAR_LATCH Save changes until server restart.
 *
 * @param var_name Which cvar
 * @param value Which value for the cvar
 * @param flags which flags
 * @sa Cvar_Set_f
 */
cvar_t *Cvar_FullSet(char *var_name, char *value, int flags)
{
	cvar_t *var;

	var = Cvar_FindVar(var_name);
	/* create it */
	if (!var)
		return Cvar_Get(var_name, value, flags);

	var->modified = true;

	/* transmit at next oportunity */
	if (var->flags & CVAR_USERINFO)
		userinfo_modified = true;

	/* free the old value string */
	Z_Free(var->string);

	var->string = CopyString(value);
	var->value = atof(var->string);
	var->flags = flags;

	return var;
}

/**
 * @brief Expands value to a string and calls Cvar_Set
 */
void Cvar_SetValue(char *var_name, float value)
{
	char val[32];

	if (value == (int) value)
		Com_sprintf(val, sizeof(val), "%i", (int) value);
	else
		Com_sprintf(val, sizeof(val), "%1.2f", value);
	Cvar_Set(var_name, val);
}


/**
 * @brief Any variables with latched values will now be updated
 * @note CVAR_LATCH cvars are not updated during a game (tactical mission)
 */
void Cvar_GetLatchedVars(void)
{
	cvar_t *var;

	for (var = cvar_vars; var; var = var->next) {
		if (!var->latched_string)
			continue;
		Z_Free(var->string);
		var->string = var->latched_string;
		var->latched_string = NULL;
		var->value = atof(var->string);
		if (!Q_strncmp(var->name, "game", MAX_VAR)) {
			FS_SetGamedir(var->string);
			FS_ExecAutoexec();
		}
	}
}

/**
 * @brief Handles variable inspection and changing from the console
 * @return bool_t True if cvar exists - false otherwise
 *
 * You can print the current value or set a new value with this function
 * To set a new value for a cvar from within the console just type the cvar name
 * followed by the value. To print the current cvar's value just type the cvar name
 * and hit enter
 * @sa Cvar_Set_f
 * @sa Cvar_SetValue
 * @sa Cvar_Set
 */
bool_t Cvar_Command(void)
{
	cvar_t *v;

	/* check variables */
	v = Cvar_FindVar(Cmd_Argv(0));
	if (!v)
		return false;

	/* perform a variable print or set */
	if (Cmd_Argc() == 1) {
		Com_Printf("\"%s\" is \"%s\"\n", v->name, v->string);
		return true;
	}

	Cvar_Set(v->name, Cmd_Argv(1));
	return true;
}


/**
 * @brief Allows setting and defining of arbitrary cvars from console
 */
void Cvar_Set_f(void)
{
	int c;
	int flags;

	c = Cmd_Argc();
	if (c != 3 && c != 4) {
		Com_Printf("usage: set <variable> <value> [u / s]\n");
		return;
	}

	if (c == 4) {
		if (!Q_strncmp(Cmd_Argv(3), "u", 1))
			flags = CVAR_USERINFO;
		else if (!Q_strncmp(Cmd_Argv(3), "s", 1))
			flags = CVAR_SERVERINFO;
		else {
			Com_Printf("flags can only be 'u' or 's'\n");
			return;
		}
		Cvar_FullSet(Cmd_Argv(1), Cmd_Argv(2), flags);
	} else
		Cvar_Set(Cmd_Argv(1), Cmd_Argv(2));
}


/**
 * @brief Allows copying variables
 * Available via console command copy
 */
void Cvar_Copy_f(void)
{
	int c;

	c = Cmd_Argc();
	if (c < 3) {
		Com_Printf("usage: set <target> <source>\n");
		return;
	}

	Cvar_Set(Cmd_Argv(1), Cvar_VariableString(Cmd_Argv(2)));
}


/**
 * @brief Stores the archive cvars
 * @param path Config file where we will save all cvars with the archive flag set
 */
void Cvar_WriteVariables(char *path)
{
	cvar_t *var;
	char buffer[1024];
	FILE *f;

	memset(buffer, 0, sizeof(buffer));

	f = fopen(path, "w");
	if (!f)
		return;

	fprintf(f, "// generated by ufo, do not modify\n");

	for (var = cvar_vars; var; var = var->next)
		if (var->flags & CVAR_ARCHIVE) {
			Com_sprintf(buffer, sizeof(buffer), "set %s \"%s\"\n", var->name, var->string);
			fprintf(f, "%s", buffer);
		}
	fclose(f);
}

/**
 * @brief List all cvars via console command 'cvarlist'
 */
void Cvar_List_f(void)
{
	cvar_t *var;
	int i, c, l = 0;
	char *token = NULL;

	c = Cmd_Argc();

	if (c == 2) {
		token = Cmd_Argv(1);
		l = strlen(token);
	}

	i = 0;
	for (var = cvar_vars; var; var = var->next, i++) {
		if (c == 2 && Q_strncmp(var->name, token, l)) {
			i--;
			continue;
		}
		if (var->flags & CVAR_ARCHIVE)
			Com_Printf("*");
		else
			Com_Printf(" ");
		if (var->flags & CVAR_USERINFO)
			Com_Printf("U");
		else
			Com_Printf(" ");
		if (var->flags & CVAR_SERVERINFO)
			Com_Printf("S");
		else
			Com_Printf(" ");
		if (var->modified)
			Com_Printf("M");
		else
			Com_Printf(" ");
		if (var->flags & CVAR_NOSET)
			Com_Printf("-");
		else if (var->flags & CVAR_LATCH)
			Com_Printf("L");
		else
			Com_Printf(" ");
		Com_Printf(" %s \"%s\"\n", var->name, var->string);
	}
	Com_Printf("%i cvars\n", i);
}

/**
 * @brief Return a string with all cvars with bitflag given by parameter set
 * @param bit The bitflag we search the global cvar array for
 */
char *Cvar_BitInfo(int bit)
{
	static char info[MAX_INFO_STRING];
	cvar_t *var;

	info[0] = 0;

	for (var = cvar_vars; var; var = var->next)
		if (var->flags & bit)
			Info_SetValueForKey(info, var->name, var->string);
	return info;
}

/**
 * @brief Returns an info string containing all the CVAR_USERINFO cvars
 */
char *Cvar_Userinfo(void)
{
	return Cvar_BitInfo(CVAR_USERINFO);
}

/**
 * @brief Returns an info string containing all the CVAR_SERVERINFO cvars
 */
char *Cvar_Serverinfo(void)
{
	return Cvar_BitInfo(CVAR_SERVERINFO);
}

/**
 * @brief Reads in all archived cvars
 */
void Cvar_Init(void)
{
	Cmd_AddCommand("set", Cvar_Set_f);
	Cmd_AddCommand("copy", Cvar_Copy_f);
	Cmd_AddCommand("cvarlist", Cvar_List_f);
}
