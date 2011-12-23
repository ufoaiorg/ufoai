/**
 * @file cvar.h
 * @brief Cvar (console variable) header file
 *
 * cvar_t variables are used to hold scalar or string variables that can be changed or displayed at the console or prog code as well as accessed directly
 * in C code.
 * The user can access cvars from the console in three ways:
 * r_draworder			prints the current value
 * r_draworder 0		sets the current value to 0
 * set r_draworder 0	as above, but creates the cvar if not present
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

#ifndef COMMON_CVAR
#define COMMON_CVAR

#define CVAR_ARCHIVE    1       /**< set to cause it to be saved to vars.rc */
#define CVAR_USERINFO   2       /**< added to userinfo  when changed */
#define CVAR_SERVERINFO 4       /**< added to serverinfo when changed */
#define CVAR_NOSET      8       /**< don't allow change from console at all, but can be set from the command line */
#define CVAR_LATCH      16      /**< save changes until server restart */
#define CVAR_DEVELOPER  32      /**< set from commandline (not from within the game) and hide from console */
#define CVAR_CHEAT      64      /**< clamp to the default value when cheats are off */
#define CVAR_R_IMAGES   128     /**< effects image filtering */
#define CVAR_R_CONTEXT    256     /**< vid shutdown if such a cvar was modified */
#define CVAR_R_PROGRAMS 512		/**< if changed, shaders are restarted */

#define CVAR_R_MASK (CVAR_R_IMAGES | CVAR_R_CONTEXT | CVAR_R_PROGRAMS)

/**
 * @brief Callback for the listener
 * @param cvarName The name of the cvar that was changed.
 * @param oldValue The old value of the cvar - this is never @c NULL, but can be empty.
 * @param newValue The new value of the cvar - this is never @c NULL, but can be empty.
 */
typedef void (*cvarChangeListenerFunc_t) (const char *cvarName, const char *oldValue, const char *newValue, void *data);

typedef struct cvarListener_s {
	cvarChangeListenerFunc_t exec;
	struct cvarListener_s *next;
	void *data;
} cvarChangeListener_t;

/**
 * @brief This is a cvar defintion. Cvars can be user modified and used in our menus e.g.
 * @note nothing outside the Cvar_*() functions should modify these fields!
 */
typedef struct cvar_s {
	char *name;				/**< cvar name */
	char *string;			/**< value as string */
	char *latchedString;	/**< for CVAR_LATCH vars */
	char *defaultString;	/**< default string set on first init - only set for CVAR_CHEAT */
	char *oldString;		/**< value of the cvar before we changed it */
	char *description;		/**< cvar description */
	int flags;				/**< cvar flags CVAR_ARCHIVE|CVAR_NOSET.... */
	qboolean modified;		/**< set each time the cvar is changed */
	float value;			/**< value as float */
	int integer;			/**< value as integer */
	qboolean (*check) (struct cvar_s* cvar);	/**< cvar check function */
	cvarChangeListener_t *changeListener;
	struct cvar_s *next;
	struct cvar_s *prev;
	struct cvar_s *hash_next;
} cvar_t;

typedef struct cvarList_s {
	const char *name;
	const char *value;
	cvar_t *var;
} cvarList_t;

/**
 * @brief Return the first cvar of the cvar list
 */
cvar_t *Cvar_GetFirst(void);

/**
 * @brief creates the variable if it doesn't exist, or returns the existing one
 * if it exists, the value will not be changed, but flags will be ORed in
 * that allows variables to be unarchived without needing bitflags
 */
cvar_t *Cvar_Get(const char *varName, const char *value, int flags, const char* desc);

/**
 * @brief will create the variable if it doesn't exist
 */
cvar_t *Cvar_Set(const char *varName, const char *value);

/**
 * @brief will set the variable even if NOSET or LATCH
 */
cvar_t *Cvar_ForceSet(const char *varName, const char *value);

cvar_t *Cvar_FullSet(const char *varName, const char *value, int flags);

/**
 * @brief expands value to a string and calls Cvar_Set
 */
void Cvar_SetValue(const char *varName, float value);

/**
 * @brief returns 0 if not defined or non numeric
 */
int Cvar_GetInteger(const char *varName);

/**
 * @brief returns 0.0 if not defined or non numeric
 */
float Cvar_GetValue(const char *varName);

/**
 * @brief returns an empty string if not defined
 */
const char *Cvar_GetString(const char *varName);

/**
 * @brief returns an empty string if not defined
 */
const char *Cvar_VariableStringOld(const char *varName);

/**
 * @brief attempts to match a partial variable name for command line completion
 * returns NULL if nothing fits
 */
int Cvar_CompleteVariable(const char *partial, const char **match);

/**
 * @brief any CVAR_LATCHED variables that have been set will now take effect
 */
void Cvar_UpdateLatchedVars(void);

/**
 * @brief called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
 * command.  Returns true if the command was a variable reference that
 * was handled. (print or change)
 */
qboolean Cvar_Command(void);

void Cvar_Init(void);
void Cvar_Shutdown(void);

/**
 * @brief returns an info string containing all the CVAR_USERINFO cvars
 */
const char *Cvar_Userinfo(void);

/**
 * @brief returns an info string containing all the CVAR_SERVERINFO cvars
 */
const char *Cvar_Serverinfo(void);

/**
 * @brief this function checks cvar ranges and integral values
 */
qboolean Cvar_AssertValue(cvar_t * cvar, float minVal, float maxVal, qboolean shouldBeIntegral);

/**
 * @brief Sets the check functions for a cvar (e.g. Cvar_Assert)
 */
qboolean Cvar_SetCheckFunction(const char *varName, qboolean (*check) (cvar_t* cvar));

/**
 * @brief Registers a listener that is executed each time a cvar changed its value.
 * @sa Cvar_ExecuteChangeListener
 * @param varName The cvar name to register the listener for
 * @param listenerFunc The listener callback to register
 */
cvarChangeListener_t *Cvar_RegisterChangeListener(const char *varName, cvarChangeListenerFunc_t listenerFunc);
/**
 * @brief Unregisters a cvar change listener
 * @param varName The cvar name to register the listener for
 * @param listenerFunc The listener callback to unregister
 */
void Cvar_UnRegisterChangeListener(const char *varName, cvarChangeListenerFunc_t listenerFunc);

/**
 * @brief Reset cheat cvar values to default
 */
void Cvar_FixCheatVars(void);

/**
 * @brief Function to remove the cvar and free the space
 */
qboolean Cvar_Delete(const char *varName);

/**
 * @brief Searches for a cvar given by parameter
 */
cvar_t *Cvar_FindVar(const char *varName);

/**
 * @brief Checks whether there are pending cvars for the given flags
 * @param flags The CVAR_* flags
 * @return true if there are pending cvars, false otherwise
 */
qboolean Cvar_PendingCvars(int flags);

void Com_SetUserinfoModified(qboolean modified);

qboolean Com_IsUserinfoModified(void);

void Com_SetRenderModified(qboolean modified);

qboolean Com_IsRenderModified(void);

void Cvar_ClearVars(int flags);

void Cvar_Reset(cvar_t *cvar);

#ifdef DEBUG
void Cvar_PrintDebugCvars(void);
#endif

#endif
