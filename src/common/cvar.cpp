/**
 * @file
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

#include "common.h"
#include "../shared/infostring.h"
#include <vector>

#define CVAR_HASH_SIZE 64

static cvar_t* cvarVarsHash[CVAR_HASH_SIZE];

// @todo This uses the STL, however, in a recent discussion on the forum it was stated that there
// should be no use of STL due to memory issues, so this needs to change
typedef std::vector<CvarListenerPtr> CvarListeners;
static CvarListeners cvarListeners;

/**
 * @brief This is set each time a CVAR_USERINFO variable is changed
 * so that the renderer knows to update stuff accordingly
 */
static bool renderModified;

/**
 * @brief This is set each time a CVAR_USERINFO variable is changed
 * so that the client knows to send it to the server
 */
static bool userinfoModified;

void Com_SetUserinfoModified (bool modified)
{
	userinfoModified = modified;
}

bool Com_IsUserinfoModified (void)
{
	return userinfoModified;
}

void Com_SetRenderModified (bool modified)
{
	renderModified = modified;
}

bool Com_IsRenderModified (void)
{
	return renderModified;
}

/**
 * @brief Cvar list
 */
static cvar_t* cvarVars;

cvar_t* Cvar_GetFirst (void)
{
	return cvarVars;
}


static bool Cvar_InfoValidate (const char* s)
{
	return s[strcspn(s, "\\\";")] == '\0';
}

/**
 * @brief Searches for a cvar given by parameter
 * @param varName The cvar name as string
 * @return Pointer to cvar_t struct or @c nullptr if no cvar with the specified name was found
 * @sa Cvar_GetString
 * @sa Cvar_SetValue
 */
cvar_t* Cvar_FindVar (const char* varName)
{
	const unsigned hash = Com_HashKey(varName, CVAR_HASH_SIZE);

	for (cvar_t* var = cvarVarsHash[hash]; var; var = var->hash_next) {
		if (Q_streq(varName, var->name))
			return var;
	}

	return nullptr;
}

/**
 * @brief Returns the float value of a cvar
 * @sa Cvar_GetString
 * @sa Cvar_FindVar
 * @sa Cvar_GetInteger
 * @return 0 if not defined
 */
float Cvar_GetValue (const char* varName)
{
	cvar_t* var;

	var = Cvar_FindVar(varName);
	if (!var)
		return 0.0;
	return atof(var->string);
}


/**
 * @brief Set a checker function for cvar values
 * @sa Cvar_FindVar
 * @return true if set
 */
bool Cvar_SetCheckFunction (const char* varName, bool (*check) (cvar_t* cvar))
{
	cvar_t* var;

	var = Cvar_FindVar(varName);
	if (!var) {
		Com_Printf("Could not set check function for cvar '%s'\n", varName);
		return false;
	}
	var->check = check;
	/* execute the check */
	var->check(var);
	return true;
}

/**
 * @brief Checks cvar values
 * @return @c true if assert isn't true and the cvar was changed to a valid value, @c false if the
 * new value is ok and nothing was changed.
 * @param[in] cvar Cvar to check
 * @param[in] minVal The minimal value the cvar should have
 * @param[in] maxVal The maximal value the cvar should have
 * @param[in] shouldBeIntegral No floats for this cvar please
 */
bool Cvar_AssertValue (cvar_t* cvar, float minVal, float maxVal, bool shouldBeIntegral)
{
	assert(cvar);

	if (shouldBeIntegral) {
		if ((int)cvar->value != cvar->integer) {
			Com_Printf("WARNING: cvar '%s' must be integral (%f)\n", cvar->name, cvar->value);
			Cvar_Set(cvar->name, "%d", cvar->integer);
			return true;
		}
	}

	if (cvar->value < minVal) {
		Com_Printf("WARNING: cvar '%s' out of range (%f < %f)\n", cvar->name, cvar->value, minVal);
		Cvar_SetValue(cvar->name, minVal);
		return true;
	} else if (cvar->value > maxVal) {
		Com_Printf("WARNING: cvar '%s' out of range (%f > %f)\n", cvar->name, cvar->value, maxVal);
		Cvar_SetValue(cvar->name, maxVal);
		return true;
	}

	/* no changes */
	return false;
}

/**
 * @brief Returns the int value of a cvar
 * @sa Cvar_GetValue
 * @sa Cvar_GetString
 * @sa Cvar_FindVar
 * @return 0 if not defined
 */
int Cvar_GetInteger (const char* varName)
{
	const cvar_t* var;

	var = Cvar_FindVar(varName);
	if (!var)
		return 0;
	return var->integer;
}

/**
 * @brief Returns the value of cvar as string
 * @sa Cvar_GetValue
 * @sa Cvar_FindVar
 *
 * Even if the cvar does not exist this function will not return a null pointer
 * but an empty string
 */
const char* Cvar_GetString (const char* varName)
{
	const cvar_t* var;

	var = Cvar_FindVar(varName);
	if (!var)
		return "";
	return var->string;
}

/**
 * @brief Returns the old value of cvar as string before we changed it
 * @sa Cvar_GetValue
 * @sa Cvar_FindVar
 *
 * Even if the cvar does not exist this function will not return a null pointer
 * but an empty string
 */
const char* Cvar_VariableStringOld (const char* varName)
{
	cvar_t* var;

	var = Cvar_FindVar(varName);
	if (!var)
		return "";
	if (var->oldString)
		return var->oldString;
	else
		return "";
}

/**
 * @brief Sets the cvar value back to the old value
 * @param cvar The cvar to reset
 */
void Cvar_Reset (cvar_t* cvar)
{
	char* str;

	if (cvar->oldString == nullptr)
		return;

	str = Mem_StrDup(cvar->oldString);
	Cvar_Set(cvar->name, "%s", str);
	Mem_Free(str);
}

/**
 * @brief Unix like tab completion for console variables
 * @param partial The beginning of the variable we try to complete
 * @param[out] match The found entry of the list we are searching, in case of more than one entry their common suffix is returned.
 * @sa Cmd_CompleteCommand
 * @sa Key_CompleteCommand
 */
int Cvar_CompleteVariable (const char* partial, const char** match)
{
	int n = 0;
	for (cvar_t const* cvar = cvarVars; cvar; cvar = cvar->next) {
#ifndef DEBUG
		if (cvar->flags & CVAR_DEVELOPER)
			continue;
#endif
		if (Cmd_GenericCompleteFunction(cvar->name, partial, match)) {
			Com_Printf("[var] %-20s = \"%s\"\n", cvar->name, cvar->string);
			if (cvar->description)
				Com_Printf(S_COLOR_GREEN "      %s\n", cvar->description);
			++n;
		}
	}
	return n;
}

/**
 * @brief Function to remove the cvar and free the space
 */
bool Cvar_Delete (const char* varName)
{
	unsigned hash;

	hash = Com_HashKey(varName, CVAR_HASH_SIZE);
	for (cvar_t** anchor = &cvarVarsHash[hash]; *anchor; anchor = &(*anchor)->hash_next) {
		cvar_t* const var = *anchor;
		if (!Q_strcasecmp(varName, var->name)) {
			cvarChangeListener_t* changeListener;
			if (var->flags != 0) {
				Com_Printf("Can't delete the cvar '%s' - it's a special cvar\n", varName);
				return false;
			}
			HASH_Delete(anchor);
			if (var->prev) {
				assert(var->prev->next == var);
				var->prev->next = var->next;
			} else
				cvarVars = var->next;
			if (var->next) {
				assert(var->next->prev == var);
				var->next->prev = var->prev;
			}

			for (CvarListeners::iterator i = cvarListeners.begin(); i != cvarListeners.end(); ++i) {
				(*i)->onDelete(var);
			}

			Mem_Free(var->name);
			Mem_Free(var->string);
			Mem_Free(var->description);
			Mem_Free(var->oldString);
			Mem_Free(var->defaultString);
			/* latched cvars should not be removable */
			assert(var->latchedString == nullptr);
			changeListener = var->changeListener;
			while (changeListener) {
				cvarChangeListener_t* changeListener2 = changeListener->next;
				Mem_Free(changeListener);
				changeListener = changeListener2;
			}
			Mem_Free(var);

			return true;
		}
	}
	Com_Printf("Cvar '%s' wasn't found\n", varName);
	return false;
}

/**
 * @brief Init or return a cvar
 * @param[in] var_name The cvar name
 * @param[in] var_value The standard cvar value (will be set if the cvar doesn't exist)
 * @param[in] flags CVAR_USERINFO, CVAR_LATCH, CVAR_SERVERINFO, CVAR_ARCHIVE and so on
 * @param[in] desc This is a short description of the cvar (see console command cvarlist)
 * @note CVAR_ARCHIVE: Cvar will be saved to config.cfg when game shuts down - and
 * will be reloaded when game starts up the next time
 * @note CVAR_LATCH: Latched cvars will be updated at the next map load
 * @note CVAR_SERVERINFO: This cvar will be send in the server info response strings (server browser)
 * @note CVAR_NOSET: This cvar can not be set from the commandline
 * @note CVAR_USERINFO: This cvar will be added to the userinfo string when changed (network synced)
 * @note CVAR_DEVELOPER: Only changeable if we are in development mode
 * If the variable already exists, the value will not be set
 * The flags will be or'ed in if the variable exists.
 */
cvar_t* Cvar_Get (const char* var_name, const char* var_value, int flags, const char* desc)
{
	const unsigned hash = Com_HashKey(var_name, CVAR_HASH_SIZE);

	if (flags & (CVAR_USERINFO | CVAR_SERVERINFO)) {
		if (!Cvar_InfoValidate(var_name)) {
			Com_Printf("invalid info cvar name\n");
			return nullptr;
		}
	}

	if (cvar_t* const var = Cvar_FindVar(var_name)) {
		if (!var->defaultString && (flags & CVAR_CHEAT))
			var->defaultString = Mem_PoolStrDup(var_value, com_cvarSysPool, 0);
		var->flags |= flags;
		if (desc) {
			Mem_Free(var->description);
			var->description = Mem_PoolStrDup(desc, com_cvarSysPool, 0);
		}
		return var;
	}

	if (!var_value)
		return nullptr;

	if (flags & (CVAR_USERINFO | CVAR_SERVERINFO)) {
		if (!Cvar_InfoValidate(var_value)) {
			Com_Printf("invalid info cvar value '%s' of cvar '%s'\n", var_value, var_name);
			return nullptr;
		}
	}

	cvar_t* const var = Mem_PoolAllocType(cvar_t, com_cvarSysPool);
	var->name = Mem_PoolStrDup(var_name, com_cvarSysPool, 0);
	var->string = Mem_PoolStrDup(var_value, com_cvarSysPool, 0);
	var->oldString = nullptr;
	var->modified = true;
	var->value = atof(var->string);
	var->integer = atoi(var->string);
	if (desc)
		var->description = Mem_PoolStrDup(desc, com_cvarSysPool, 0);

	HASH_Add(cvarVarsHash, var, hash);
	/* link the variable in */
	var->next = cvarVars;
	cvarVars = var;
	if (var->next)
		var->next->prev = var;

	var->flags = flags;
	if (var->flags & CVAR_CHEAT)
		var->defaultString = Mem_PoolStrDup(var_value, com_cvarSysPool, 0);

	for (CvarListeners::iterator i = cvarListeners.begin(); i != cvarListeners.end(); ++i) {
		(*i)->onCreate(var);
	}

	return var;
}

/**
 * @brief Registers a cvar listener
 * @param listener The listener callback to register
 */
void Cvar_RegisterCvarListener (CvarListenerPtr listener)
{
	cvarListeners.push_back(listener);
}

/**
 * @brief Unregisters a cvar listener
 * @param listener The listener callback to unregister
 */
void Cvar_UnRegisterCvarListener (CvarListenerPtr listener)
{
	cvarListeners.erase(std::remove(cvarListeners.begin(), cvarListeners.end(), listener), cvarListeners.end());
}

/**
 * @brief Executes the change listeners for a cvar
 * @param cvar The cvar which change listeners are executed
 */
static void Cvar_ExecuteChangeListener (const cvar_t* cvar)
{
	const cvarChangeListener_t* listener = cvar->changeListener;
	while (listener) {
		listener->exec(cvar->name, cvar->oldString, cvar->string, listener->data);
		listener = listener->next;
	}
}

static cvarChangeListener_t* Cvar_GetChangeListener (cvarChangeListenerFunc_t listenerFunc)
{
	cvarChangeListener_t* const listener = Mem_PoolAllocType(cvarChangeListener_t, com_cvarSysPool);
	listener->exec = listenerFunc;
	return listener;
}

/**
 * @brief Registers a listener that is executed each time a cvar changed its value.
 * @sa Cvar_ExecuteChangeListener
 * @param varName The cvar name to register the listener for
 * @param listenerFunc The listener callback to register
 */
cvarChangeListener_t* Cvar_RegisterChangeListener (const char* varName, cvarChangeListenerFunc_t listenerFunc)
{
	cvar_t* var = Cvar_FindVar(varName);
	if (!var) {
		Com_Printf("Could not register change listener, cvar '%s' wasn't found\n", varName);
		return nullptr;
	}

	if (!var->changeListener) {
		cvarChangeListener_t* l = Cvar_GetChangeListener(listenerFunc);
		var->changeListener = l;
		return l;
	} else {
		cvarChangeListener_t* l = var->changeListener;
		while (l) {
			if (l->exec == listenerFunc) {
				return l;
			}
			l = l->next;
		}

		l = var->changeListener;
		while (l) {
			if (!l->next) {
				cvarChangeListener_t* listener = Cvar_GetChangeListener(listenerFunc);
				l->next = listener;
				l->next->next = nullptr;
				return listener;
			}
			l = l->next;
		}
	}

	return nullptr;
}

/**
 * @brief Unregisters a cvar change listener
 * @param varName The cvar name to register the listener for
 * @param listenerFunc The listener callback to unregister
 */
void Cvar_UnRegisterChangeListener (const char* varName, cvarChangeListenerFunc_t listenerFunc)
{
	cvar_t* var = Cvar_FindVar(varName);
	if (!var) {
		Com_Printf("Could not unregister change listener, cvar '%s' wasn't found\n", varName);
		return;
	}

	for (cvarChangeListener_t** anchor = &var->changeListener; *anchor; anchor = &(*anchor)->next) {
		cvarChangeListener_t* const l = *anchor;
		if (l->exec == listenerFunc) {
			*anchor = l->next;
			Mem_Free(l);
			return;
		}
	}
}

/**
 * @brief Sets a cvar values
 * Handles write protection and latched cvars as expected
 * @param[in] varName Which cvar
 * @param[in] value Set the cvar to the value specified by 'value'
 * @param[in] force Force the update of the cvar
 */
static cvar_t* Cvar_Set2 (const char* varName, const char* value, bool force)
{
	cvar_t* var;

	if (!value)
		return nullptr;

	var = Cvar_FindVar(varName);
	/* create it */
	if (!var)
		return Cvar_Get(varName, value);

	if (var->flags & (CVAR_USERINFO | CVAR_SERVERINFO)) {
		if (!Cvar_InfoValidate(value)) {
			Com_Printf("invalid info cvar value '%s' of cvar '%s'\n", value, varName);
			return var;
		}
	}

	if (!force) {
		if (var->flags & CVAR_NOSET) {
			Com_Printf("%s is write protected.\n", varName);
			return var;
		}
#ifndef DEBUG
		if (var->flags & CVAR_DEVELOPER) {
			Com_Printf("%s is a developer cvar.\n", varName);
			return var;
		}
#endif

		if (var->flags & CVAR_LATCH) {
			if (var->latchedString) {
				if (Q_streq(value, var->latchedString))
					return var;
				Mem_Free(var->latchedString);
				var->latchedString = nullptr;
			} else {
				if (Q_streq(value, var->string))
					return var;
			}

			/* if we are running a server */
			if (Com_ServerState()) {
				Com_Printf("%s will be changed for next game.\n", varName);
				var->latchedString = Mem_PoolStrDup(value, com_cvarSysPool, 0);
			} else {
				Mem_Free(var->oldString);
				var->oldString = var->string;
				var->string = Mem_PoolStrDup(value, com_cvarSysPool, 0);
				var->value = atof(var->string);
				var->integer = atoi(var->string);
			}

			if (var->check && var->check(var))
				Com_Printf("Invalid value for cvar %s\n", varName);

			return var;
		}
	} else {
		Mem_Free(var->latchedString);
		var->latchedString = nullptr;
	}

	if (Q_streq(value, var->string))
		return var;				/* not changed */

	if (var->flags & CVAR_R_MASK)
		Com_SetRenderModified(true);

	Mem_Free(var->oldString);		/* free the old value string */
	var->oldString = var->string;
	var->modified = true;

	if (var->flags & CVAR_USERINFO)
		Com_SetUserinfoModified(true);	/* transmit at next opportunity */

	var->string = Mem_PoolStrDup(value, com_cvarSysPool, 0);
	var->value = atof(var->string);
	var->integer = atoi(var->string);

	if (var->check && var->check(var)) {
		Com_Printf("Invalid value for cvar %s\n", varName);
		return var;
	}

	Cvar_ExecuteChangeListener(var);

	return var;
}

/**
 * @brief Will set the variable even if NOSET or LATCH
 */
cvar_t* Cvar_ForceSet (const char* varName, const char* value)
{
	return Cvar_Set2(varName, value, true);
}

/**
 * @brief Sets a cvar value
 * @param varName Which cvar should be set
 * @param value Which value should the cvar get
 * @note Look after the CVAR_LATCH stuff and check for write protected cvars
 */
cvar_t* Cvar_Set (const char* varName, const char* value, ...)
{
	va_list argptr;
	char text[512];
	va_start(argptr, value);
	Q_vsnprintf(text, sizeof(text), value, argptr);
	va_end(argptr);

	return Cvar_Set2(varName, text, false);
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
 * @param varName Which cvar
 * @param value Which value for the cvar
 * @param flags which flags
 * @sa Cvar_Set_f
 */
cvar_t* Cvar_FullSet (const char* varName, const char* value, int flags)
{
	cvar_t* var;

	if (!value)
		return nullptr;

	var = Cvar_FindVar(varName);
	/* create it */
	if (!var)
		return Cvar_Get(varName, value, flags);

	var->modified = true;

	/* transmit at next opportunity */
	if (var->flags & CVAR_USERINFO)
		Com_SetUserinfoModified(true);

	Mem_Free(var->oldString);		/* free the old value string */
	var->oldString = var->string;

	var->string = Mem_PoolStrDup(value, com_cvarSysPool, 0);
	var->value = atof(var->string);
	var->integer = atoi(var->string);
	var->flags = flags;

	return var;
}

/**
 * @brief Expands value to a string and calls Cvar_Set
 * @note Float values are in the format #.##
 */
void Cvar_SetValue (const char* varName, float value)
{
	if (value == (int) value)
		Cvar_Set(varName, "%i", (int)value);
	else
		Cvar_Set(varName, "%1.2f", value);
}


/**
 * @brief Any variables with latched values will now be updated
 * @note CVAR_LATCH cvars are not updated during a game (tactical mission)
 */
void Cvar_UpdateLatchedVars (void)
{
	for (cvar_t* var = cvarVars; var; var = var->next) {
		if (!var->latchedString)
			continue;
		var->oldString = var->string;
		var->string = var->latchedString;
		var->latchedString = nullptr;
		var->value = atof(var->string);
		var->integer = atoi(var->string);
	}
}

/**
 * @brief Handles variable inspection and changing from the console
 * @return True if cvar exists - false otherwise
 *
 * You can print the current value or set a new value with this function
 * To set a new value for a cvar from within the console just type the cvar name
 * followed by the value. To print the current cvar's value just type the cvar name
 * and hit enter
 * @sa Cvar_Set_f
 * @sa Cvar_SetValue
 * @sa Cvar_Set
 */
bool Cvar_Command (void)
{
	/* check variables */
	cvar_t* v = Cvar_FindVar(Cmd_Argv(0));
	if (!v)
		return false;

	/* perform a variable print or set */
	if (Cmd_Argc() == 1) {
		Com_Printf("\"%s\" is \"%s\"\n", v->name, v->string);
		return true;
	}

	Cvar_Set(v->name, "%s", Cmd_Argv(1));
	return true;
}

/**
 * @brief Allows resetting cvars to old value from console
 */
static void Cvar_SetOld_f (void)
{
	cvar_t* v;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <variable>\n", Cmd_Argv(0));
		return;
	}

	/* check variables */
	v = Cvar_FindVar(Cmd_Argv(1));
	if (!v) {
		Com_Printf("cvar '%s' not found\n", Cmd_Argv(1));
		return;
	}
	if (v->oldString)
		Cvar_Set(Cmd_Argv(1), "%s", v->oldString);
}

static void Cvar_Define_f (void)
{
	const char* name;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <cvarname> <value>\n", Cmd_Argv(0));
		return;
	}

	name = Cmd_Argv(1);

	if (Cvar_FindVar(name) == nullptr)
		Cvar_Set(name, "%s", Cmd_Argc() == 3 ? Cmd_Argv(2) : "");
}

/**
 * @brief Allows setting and defining of arbitrary cvars from console
 */
static void Cvar_Set_f (void)
{
	const int c = Cmd_Argc();
	if (c != 3 && c != 4) {
		Com_Printf("Usage: %s <variable> <value> [u / s]\n", Cmd_Argv(0));
		return;
	}

	if (c == 4) {
		const char* arg = Cmd_Argv(3);
		int flags = 0;

		while (arg[0] != '\0') {
			switch (arg[0]) {
			case 'u':
				flags |= CVAR_USERINFO;
				break;
			case 's':
				flags |= CVAR_SERVERINFO;
				break;
			case 'a':
				flags |= CVAR_ARCHIVE;
				break;
			default:
				Com_Printf("invalid flags %c given\n", arg[0]);
				break;
			}
			arg++;
		}
		Cvar_FullSet(Cmd_Argv(1), Cmd_Argv(2), flags);
	} else {
		Cvar_Set(Cmd_Argv(1), "%s", Cmd_Argv(2));
	}
}

/**
 * @brief Allows switching boolean cvars between zero and not-zero from console
 */
static void Cvar_Switch_f (void)
{
	const int c = Cmd_Argc();
	if (c != 2 && c != 3) {
		Com_Printf("Usage: %s <variable> [u / s / a]\n", Cmd_Argv(0));
		return;
	}

	if (c == 3) {
		const char* arg = Cmd_Argv(2);
		int flags = 0;

		while (arg[0] != '\0') {
			switch (arg[0]) {
			case 'u':
				flags |= CVAR_USERINFO;
				break;
			case 's':
				flags |= CVAR_SERVERINFO;
				break;
			case 'a':
				flags |= CVAR_ARCHIVE;
				break;
			default:
				Com_Printf("invalid flags %c given\n", arg[0]);
				break;
			}
			arg++;
		}
		Cvar_FullSet(Cmd_Argv(1), va("%i", !Cvar_GetInteger(Cmd_Argv(1))), flags);
	} else {
		Com_Printf("val: %i\n", Cvar_GetInteger(Cmd_Argv(1)));
		Cvar_Set(Cmd_Argv(1), "%i", !Cvar_GetInteger(Cmd_Argv(1)));
	}
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
		Com_Printf("Usage: %s <target> <source>\n", Cmd_Argv(0));
		return;
	}

	Cvar_Set(Cmd_Argv(1), "%s", Cvar_GetString(Cmd_Argv(2)));
}


/**
 * @brief appends lines containing "set variable value" for all variables
 * with the archive flag set to true.
 * @note Stores the archive cvars
 */
void Cvar_WriteVariables (qFILE* f)
{
	for (const cvar_t* var = cvarVars; var; var = var->next) {
		if (var->flags & CVAR_ARCHIVE)
			FS_Printf(f, "set %s \"%s\" a\n", var->name, var->string);
	}
}

/**
 * @brief Checks whether there are pending cvars for the given flags
 * @param flags The CVAR_* flags
 * @return true if there are pending cvars, false otherwise
 */
bool Cvar_PendingCvars (int flags)
{
	for (const cvar_t* var = cvarVars; var; var = var->next) {
		if ((var->flags & flags) && var->modified)
			return true;
	}

	return false;
}

void Cvar_ClearVars (int flags)
{
	for (cvar_t* var = cvarVars; var; var = var->next) {
		if (var->flags & flags)
			var->modified = false;
	}
}

/**
 * @brief List all cvars via console command 'cvarlist'
 */
static void Cvar_List_f (void)
{
	int i, c, l = 0;
	const char* token = nullptr;

	c = Cmd_Argc();

	if (c == 2) {
		token = Cmd_Argv(1);
		l = strlen(token);
	}

	i = 0;
	for (cvar_t* var = cvarVars; var; var = var->next, i++) {
		if (token && strncmp(var->name, token, l)) {
			i--;
			continue;
		}
#ifndef DEBUG
		/* don't show developer cvars in release mode */
		if (var->flags & CVAR_DEVELOPER)
			continue;
#endif

		Com_Printf(var->flags & CVAR_ARCHIVE	? "A" : " ");
		Com_Printf(var->flags & CVAR_USERINFO	? "U" : " ");
		Com_Printf(var->flags & CVAR_SERVERINFO ? "S" : " ");
		Com_Printf(var->modified				? "M" : " ");
		Com_Printf(var->flags & CVAR_DEVELOPER	? "D" : " ");
		Com_Printf(var->flags & CVAR_R_IMAGES	? "I" : " ");
		Com_Printf(var->flags & CVAR_NOSET		? "-" :
		           var->flags & CVAR_LATCH		? "L" : " ");
		Com_Printf(" %-20s \"%s\"\n", var->name, var->string);
		if (var->description)
			Com_Printf(S_COLOR_GREEN "        %s\n", var->description);
	}
	Com_Printf("%i cvars\n", i);
	Com_Printf("legend:\n"
		"S: Serverinfo\n"
		"L: Latched\n"
		"D: Developer\n"
		"U: Userinfo\n"
		"I: Image\n"
		"*: Archive\n"
		"-: Not changeable\n"
	);
}

/**
 * @brief Return a string with all cvars with bitflag given by parameter set
 * @param bit The bitflag we search the global cvar array for
 * @param info The resulting string
 * @param infoSize The max length of the result (MAX_INFO_STRING is a good idea here)
 */
static char* Cvar_BitInfo (int bit, char* info, size_t infoSize)
{
	for (cvar_t* var = cvarVars; var; var = var->next) {
		if (var->flags & bit)
			Info_SetValueForKey(info, infoSize, var->name, var->string);
	}
	return info;
}

/**
 * @brief Returns an info string containing all the CVAR_USERINFO cvars
 */
const char* Cvar_Userinfo (char* info, size_t infoSize)
{
	info[0] = '\0';
	return Cvar_BitInfo(CVAR_USERINFO, info, infoSize);
}

/**
 * @brief Returns an info string containing all the CVAR_SERVERINFO cvars
 * @sa SV_StatusString
 */
const char* Cvar_Serverinfo (char* info, size_t infoSize)
{
	info[0] = '\0';
	return Cvar_BitInfo(CVAR_SERVERINFO, info, infoSize);
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
		Com_Printf("Usage: %s <variable>\n", Cmd_Argv(0));
		return;
	}

	Cvar_Delete(Cmd_Argv(1));
}

/**
 * @brief Add a value to a cvar
 */
static void Cvar_Add_f (void)
{
	cvar_t* cvar;
	float value;
	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <variable> <value>\n", Cmd_Argv(0));
		return;
	}

	cvar = Cvar_FindVar(Cmd_Argv(1));
	if (!cvar) {
		Com_Printf("Cvar_Add_f: %s does not exist\n", Cmd_Argv(1));
		return;
	}

	value = cvar->value + atof(Cmd_Argv(2));
	Cvar_SetValue(Cmd_Argv(1), value);
}

/**
 * @brief Apply a modulo to a cvar
 */
static void Cvar_Mod_f (void)
{
	cvar_t* cvar;
	int value;
	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <variable> <value>\n", Cmd_Argv(0));
		return;
	}

	cvar = Cvar_FindVar(Cmd_Argv(1));
	if (!cvar) {
		Com_Printf("Cvar_Mod_f: %s does not exist\n", Cmd_Argv(1));
		return;
	}

	value = cvar->integer % atoi(Cmd_Argv(2));
	Cvar_SetValue(Cmd_Argv(1), value);
}

/**
 * @brief Reset cheat cvar values to default
 * @sa CL_SendCommand
 */
void Cvar_FixCheatVars (void)
{
	if (!(Com_ServerState() && !Cvar_GetInteger("sv_cheats")))
		return;

	for (cvar_t* var = cvarVars; var; var = var->next) {
		if (!(var->flags & CVAR_CHEAT))
			continue;

		if (!var->defaultString) {
			Com_Printf("Cheat cvars: Cvar %s has no default value\n", var->name);
			continue;
		}

		if (Q_streq(var->string, var->defaultString))
			continue;

		/* also remove the oldString value here */
		Mem_Free(var->oldString);
		var->oldString = nullptr;
		Mem_Free(var->string);
		var->string = Mem_PoolStrDup(var->defaultString, com_cvarSysPool, 0);
		var->value = atof(var->string);
		var->integer = atoi(var->string);

		Com_Printf("'%s' is a cheat cvar - activate sv_cheats to use it.\n", var->name);
	}
}

#ifdef DEBUG
void Cvar_PrintDebugCvars (void)
{
	Com_Printf("Debug cvars:\n");
	for (const cvar_t* var = cvarVars; var; var = var->next) {
		if ((var->flags & CVAR_DEVELOPER) || !strncmp(var->name, "debug_", 6))
			Com_Printf(" * %s (%s)\n   %s\n", var->name, var->string, var->description ? var->description : "");
	}
	Com_Printf("\n");
}
#endif

/**
 * @brief Reads in all archived cvars
 * @sa Qcommon_Init
 */
void Cvar_Init (void)
{
	Cmd_AddCommand("setold", Cvar_SetOld_f, "Restore the cvar old value");
	Cmd_AddCommand("del", Cvar_Del_f, "Delete a cvar");
	Cmd_AddCommand("set", Cvar_Set_f, "Set a cvar value");
	Cmd_AddCommand("switch", Cvar_Switch_f, "Switch a boolean cvar value");
	Cmd_AddCommand("add", Cvar_Add_f, "Add a value to a cvar");
	Cmd_AddCommand("define", Cvar_Define_f, "Defines a cvar if it does not exist");
	Cmd_AddCommand("mod", Cvar_Mod_f, "Apply a modulo on a cvar");
	Cmd_AddCommand("copy", Cvar_Copy_f, "Copy cvar target to source");
	Cmd_AddCommand("cvarlist", Cvar_List_f, "Show all cvars");
}

void Cvar_Shutdown (void)
{
	cvarVars = nullptr;
	OBJZERO(cvarVarsHash);
}
