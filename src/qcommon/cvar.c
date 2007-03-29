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

#define CVAR_HASH_SIZE          64

static cvar_t *cvar_vars_hash[CVAR_HASH_SIZE];

/**
 * @brief This is set each time a CVAR_USERINFO variable is changed
 * so that the client knows to send it to the server
 */
qboolean userinfo_modified;

/**
 * @brief Cvar list
 */
static cvar_t *cvar_vars;

/**
 * @brief
 */
static qboolean Cvar_InfoValidate (const char *s)
{
	if (strstr(s, "\\"))
		return qfalse;
	if (strstr(s, "\""))
		return qfalse;
	if (strstr(s, ";"))
		return qfalse;
	return qtrue;
}

/**
 * @brief Searches for a cvar given by parameter
 * @param var_name The cvar name as string
 * @return Pointer to cvar_t struct
 * @sa Cvar_VariableString
 * @sa Cvar_SetValue
 */
static cvar_t *Cvar_FindVar (const char *var_name)
{
	unsigned hash;
	cvar_t *var;

	hash = Com_HashKey (var_name, CVAR_HASH_SIZE);
	for (var = cvar_vars_hash[hash]; var; var = var->next)
		if (!Q_strcmp(var_name, var->name))
			return var;

	return NULL;
}

/**
 * @brief Returns the float value of a cvar
 * @sa Cvar_VariableString
 * @sa Cvar_FindVar
 * @sa Cvar_VariableInteger
 * @return 0 if not defined
 */
extern float Cvar_VariableValue (const char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar(var_name);
	if (!var)
		return 0.0;
	return atof(var->string);
}


/**
 * @brief Returns the float value of a cvar
 * @sa Cvar_FindVar
 * @return true if set
 */
extern qboolean Cvar_SetCheckFunction (char *var_name, qboolean (*check) (void) )
{
	cvar_t *var;

	var = Cvar_FindVar(var_name);
	if (!var)
		return qfalse;
	var->check = check;
	return qtrue;
}

/**
 * @brief Checks cvar values
 * @return true if assert
 * @sa Cvar_AssertString
 */
extern qboolean Cvar_AssertValue (cvar_t * cvar, float minVal, float maxVal, qboolean shouldBeIntegral)
{
	assert(cvar);

	if (shouldBeIntegral) {
		if((int)cvar->value != cvar->integer) {
			Com_Printf("WARNING: cvar '%s' must be integral (%f)\n", cvar->name, cvar->value);
			Cvar_Set(cvar->name, va("%d", cvar->integer));
			return qtrue;
		}
	}

	if (cvar->value < minVal) {
		Com_Printf("WARNING: cvar '%s' out of range (%f < %f)\n", cvar->name, cvar->value, minVal);
		Cvar_Set(cvar->name, va("%f", minVal));
		return qtrue;
	} else if(cvar->value > maxVal) {
		Com_Printf("WARNING: cvar '%s' out of range (%f > %f)\n", cvar->name, cvar->value, maxVal);
		Cvar_Set(cvar->name, va("%f", maxVal));
		return qtrue;
	}

	/* no changes */
	return qfalse;
}

/**
 * @brief Checks cvar values
 * @return true if assert
 * @sa Cvar_AssertValue
 */
extern qboolean Cvar_AssertString (cvar_t * cvar, char **array, int arraySize)
{
	int i;
	char *string;

	assert(cvar);

	for (i = 0; i < arraySize; i++) {
		string = array[i];
		if (Q_strncmp(cvar->string, string, sizeof(cvar->string))) {
			/* valid value */
			return qfalse;
		}
	}

	Com_Printf("Cvar '%s' has not a valid value\n", cvar->name);

	if (cvar->old_string)
		Cvar_Set(cvar->name, cvar->old_string);

	/* not a valid value */
	return qtrue;
}

/**
 * @brief Returns the int value of a cvar
 * @sa Cvar_VariableValue
 * @sa Cvar_VariableString
 * @sa Cvar_FindVar
 * @return 0 if not defined
 */
extern int Cvar_VariableInteger (const char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar(var_name);
	if (!var)
		return 0;
	return atoi(var->string);
}

/**
 * @brief Returns the value of cvar as string
 * @sa Cvar_VariableValue
 * @sa Cvar_FindVar
 *
 * Even if the cvar does not exist this function will not return a null pointer
 * but an empty string
 */
extern char *Cvar_VariableString (const char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar(var_name);
	if (!var)
		return "";
	return var->string;
}

/**
 * @brief Returns the old value of cvar as string before we changed it
 * @sa Cvar_VariableValue
 * @sa Cvar_FindVar
 *
 * Even if the cvar does not exist this function will not return a null pointer
 * but an empty string
 */
extern char *Cvar_VariableStringOld (const char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar(var_name);
	if (!var)
		return "";
	if (var->old_string)
		return var->old_string;
	else
		return "";
}

/**
 * @brief Unix like tab completion for console variables
 * @param partial The beginning of the variable we try to complete
 * @sa Cmd_CompleteCommand
 * @sa Key_CompleteCommand
 */
extern int Cvar_CompleteVariable (const char *partial, const char **match)
{
	cvar_t *cvar;
	const char *localMatch = NULL;
	int len, matches = 0;

	len = strlen(partial);

	if (!len)
		return 0;

	/* check for partial matches */
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
		if (!Q_strncmp(partial, cvar->name, len)) {
			Com_Printf("[var] %s\n", cvar->name);
			localMatch = cvar->name;
			matches++;
		}

	if (matches == 1)
		*match = localMatch;
	return matches;
}

/**
 * @brief Init or return a cvar
 *
 * If the variable already exists, the value will not be set
 * The flags will be or'ed in if the variable exists.
 */
extern cvar_t *Cvar_Get (const char *var_name, const char *var_value, int flags, const char* desc)
{
	unsigned hash;
	cvar_t *var;

	if (flags & (CVAR_USERINFO | CVAR_SERVERINFO))
		if (!Cvar_InfoValidate(var_name)) {
			Com_Printf("invalid info cvar name\n");
			return NULL;
		}

	hash = Com_HashKey (var_name, CVAR_HASH_SIZE);
	for (var = cvar_vars_hash[hash]; var;  var=var->hash_next)
		if (!Q_stricmp (var_name, var->name)) {
			var->flags |= flags;
			if (desc)
				var->description = desc;
			return var;
		}

	if (!var_value)
		return NULL;

	if (flags & (CVAR_USERINFO | CVAR_SERVERINFO))
		if (!Cvar_InfoValidate(var_value)) {
			Com_Printf("invalid info cvar value\n");
			return NULL;
		}

	var = Mem_Alloc(sizeof(*var));
	var->name = CopyString(var_name);
	var->string = CopyString(var_value);
	var->old_string = NULL;
	var->modified = qtrue;
	var->value = atof(var->string);
	var->integer = atoi(var->string);
	var->description = desc;

	/* link the variable in */
	/* cvar_vars_hash should be null on the first run */
	var->hash_next = cvar_vars_hash[hash];
	/* set the cvar_vars_hash pointer to the current cvar */
	/* if there were already others in cvar_vars_hash at position hash, they are
	 * now accessable via var->next - loop until var->next is null (the first
	 * cvar at that position)
	 */
	cvar_vars_hash[hash] = var;
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
static cvar_t *Cvar_Set2 (const char *var_name, const char *value, qboolean force)
{
	cvar_t *var;

	var = Cvar_FindVar(var_name);
	/* create it */
	if (!var)
		return Cvar_Get(var_name, value, 0, NULL);

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
				if (!Q_strcmp(value, var->latched_string))
					return var;
				Mem_Free(var->latched_string);
			} else {
				if (!Q_strcmp(value, var->string))
					return var;
			}

			/* if we are running a server */
			if (Com_ServerState()) {
				Com_Printf("%s will be changed for next game.\n", var_name);
				var->latched_string = CopyString(value);
			} else {
				var->old_string = CopyString(var->string);
				var->string = CopyString(value);
				var->value = atof(var->string);
				var->integer = atoi(var->string);
				if (!Q_strncmp(var->name, "game", MAX_VAR)) {
					FS_SetGamedir(var->string);
					FS_ExecAutoexec();
				}
			}
			return var;
		}
	} else {
		if (var->latched_string) {
			Mem_Free(var->latched_string);
			var->latched_string = NULL;
		}
	}

	if (!Q_strcmp(value, var->string))
		return var;				/* not changed */

	var->old_string = CopyString(var->string);
	var->modified = qtrue;

	if (var->flags & CVAR_USERINFO)
		userinfo_modified = qtrue;	/* transmit at next oportunity */

	Mem_Free(var->string);		/* free the old value string */

	var->string = CopyString(value);
	var->value = atof(var->string);
	var->integer = atoi(var->string);

	return var;
}

/**
 * @brief Will set the variable even if NOSET or LATCH
 */
extern cvar_t *Cvar_ForceSet (const char *var_name, const char *value)
{
	return Cvar_Set2(var_name, value, qtrue);
}

/**
 * @brief Sets a cvar value
 * @param var_name Which cvar should be set
 * @param value Which value should the cvar get
 * @note Look after the CVAR_LATCH stuff and check for write protected cvars
 */
extern cvar_t *Cvar_Set (const char *var_name, const char *value)
{
	return Cvar_Set2(var_name, value, qfalse);
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
extern cvar_t *Cvar_FullSet (const char *var_name, const char *value, int flags)
{
	cvar_t *var;

	var = Cvar_FindVar(var_name);
	/* create it */
	if (!var)
		return Cvar_Get(var_name, value, flags, NULL);

	var->modified = qtrue;

	/* transmit at next oportunity */
	if (var->flags & CVAR_USERINFO)
		userinfo_modified = qtrue;

	/* free the old value string */
	Mem_Free(var->string);

	var->string = CopyString(value);
	var->value = atof(var->string);
	var->integer = atoi(var->string);
	var->flags = flags;

	return var;
}

/**
 * @brief Expands value to a string and calls Cvar_Set
 */
extern void Cvar_SetValue (const char *var_name, float value)
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
extern void Cvar_GetLatchedVars (void)
{
	cvar_t *var;

	for (var = cvar_vars; var; var = var->next) {
		if (!var->latched_string)
			continue;
		var->old_string = var->string;
		var->string = var->latched_string;
		var->latched_string = NULL;
		var->value = atof(var->string);
		var->integer = atoi(var->string);
		if (!Q_strncmp(var->name, "game", MAX_VAR)) {
			FS_SetGamedir(var->string);
			FS_ExecAutoexec();
		}
	}
}

/**
 * @brief Handles variable inspection and changing from the console
 * @return qboolean True if cvar exists - false otherwise
 *
 * You can print the current value or set a new value with this function
 * To set a new value for a cvar from within the console just type the cvar name
 * followed by the value. To print the current cvar's value just type the cvar name
 * and hit enter
 * @sa Cvar_Set_f
 * @sa Cvar_SetValue
 * @sa Cvar_Set
 */
extern qboolean Cvar_Command (void)
{
	cvar_t *v;

	/* check variables */
	v = Cvar_FindVar(Cmd_Argv(0));
	if (!v)
		return qfalse;

	/* perform a variable print or set */
	if (Cmd_Argc() == 1) {
		Com_Printf("\"%s\" is \"%s\"\n", v->name, v->string);
		return qtrue;
	}

	Cvar_Set(v->name, Cmd_Argv(1));
	return qtrue;
}

/**
 * @brief Allows resetting cvars to old value from console
 */
static void Cvar_SetOld_f (void)
{
	cvar_t *v;

	if (Cmd_Argc() != 2) {
		Com_Printf("usage: setold <variable>\n");
		return;
	}

	/* check variables */
	v = Cvar_FindVar(Cmd_Argv(1));
	if (!v) {
		Com_Printf("cvar '%s' not found\n", Cmd_Argv(1));
		return;
	}
	if (v->old_string)
		Cvar_Set(Cmd_Argv(1), v->old_string);
}

/**
 * @brief Allows setting and defining of arbitrary cvars from console
 */
static void Cvar_Set_f (void)
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
static void Cvar_Copy_f (void)
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
extern void Cvar_WriteVariables (const char *path)
{
	cvar_t *var;
	char buffer[256];
	FILE *f;

	memset(buffer, 0, sizeof(buffer));

	f = fopen(path, "w");
	if (!f) {
		Com_Printf("Could not open '%s' to store the user settings\n", path);
		return;
	}

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
static void Cvar_List_f (void)
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
		if (var->description)
			Com_Printf("%c - %s\n", 2, var->description);
	}
	Com_Printf("%i cvars\n", i);
}

/**
 * @brief Return a string with all cvars with bitflag given by parameter set
 * @param bit The bitflag we search the global cvar array for
 */
static char *Cvar_BitInfo (int bit)
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
extern char *Cvar_Userinfo (void)
{
	return Cvar_BitInfo(CVAR_USERINFO);
}

/**
 * @brief Returns an info string containing all the CVAR_SERVERINFO cvars
 */
extern char *Cvar_Serverinfo (void)
{
	return Cvar_BitInfo(CVAR_SERVERINFO);
}

/**
 * @brief Delete a cvar - set [cvar] "" isn't working from within the scripts
 * @sa Cvar_Set_f
 */
static void Cvar_Del_f (void)
{
	int c;

	c = Cmd_Argc();
	if (c != 2) {
		Com_Printf("usage: del <variable>\n");
		return;
	}

	Cbuf_ExecuteText(EXEC_NOW, va("set %s \"\"", Cmd_Argv(1)));
}

/**
 * @brief Reads in all archived cvars
 * @sa Qcommon_Init
 */
extern void Cvar_Init (void)
{
	Cmd_AddCommand("setold", Cvar_SetOld_f, NULL);
	Cmd_AddCommand("del", Cvar_Del_f, NULL);
	Cmd_AddCommand("set", Cvar_Set_f, NULL);
	Cmd_AddCommand("copy", Cvar_Copy_f, NULL);
	Cmd_AddCommand("cvarlist", Cvar_List_f, NULL);
}
