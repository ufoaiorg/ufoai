/*
==============================================================

CVAR

==============================================================
*/

/*

cvar_t variables are used to hold scalar or string variables that can be changed or displayed at the console or prog code as well as accessed directly
in C code.

The user can access cvars from the console in three ways:
r_draworder			prints the current value
r_draworder 0		sets the current value to 0
set r_draworder 0	as above, but creates the cvar if not present
Cvars are restricted from having the same names as commands to keep this
interface from being ambiguous.
*/

cvar_t *Cvar_Get(char *var_name, char *value, int flags);

/* creates the variable if it doesn't exist, or returns the existing one */
/* if it exists, the value will not be changed, but flags will be ORed in */
/* that allows variables to be unarchived without needing bitflags */

cvar_t *Cvar_Set(char *var_name, char *value);

/* will create the variable if it doesn't exist */

cvar_t *Cvar_ForceSet(char *var_name, char *value);

/* will set the variable even if NOSET or LATCH */

cvar_t *Cvar_FullSet(char *var_name, char *value, int flags);

void Cvar_SetValue(char *var_name, float value);

/* expands value to a string and calls Cvar_Set */

float Cvar_VariableValue(char *var_name);

/* returns 0 if not defined or non numeric */

char *Cvar_VariableString(char *var_name);

/* returns an empty string if not defined */

int Cvar_CompleteVariable(char *partial, char **match);

/* attempts to match a partial variable name for command line completion */
/* returns NULL if nothing fits */

void Cvar_GetLatchedVars(void);

/* any CVAR_LATCHED variables that have been set will now take effect */

qboolean Cvar_Command(void);

/* called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known */
/* command.  Returns true if the command was a variable reference that */
/* was handled. (print or change) */

void Cvar_WriteVariables(char *path);

/* appends lines containing "set variable value" for all variables */
/* with the archive flag set to true. */

void Cvar_Init(void);

char *Cvar_Userinfo(void);

/* returns an info string containing all the CVAR_USERINFO cvars */

char *Cvar_Serverinfo(void);

/* returns an info string containing all the CVAR_SERVERINFO cvars */

extern qboolean userinfo_modified;

/* this is set each time a CVAR_USERINFO variable is changed */
/* so that the client knows to send it to the server */


