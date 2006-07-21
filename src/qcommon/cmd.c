/**
 * @file cmd.c
 * @brief Script command processing module
 */

/*
 * All original material Copyright (C) 2002-2006 UFO: Alien Invasion team.
 *
 * Original file from Quake 2 v3.21: quake2-2.31/qcommon/cmd.c
 * Copyright (C) 1997-2001 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include "cmd.h"
/* FIXME: Check and minimise inclusions. */
#include "qcommon.h"

void Cmd_ForwardToServer(void);

#define	MAX_ALIAS_NAME	32

typedef struct cmdalias_s {
	struct cmdalias_s *next;
	char name[MAX_ALIAS_NAME];
	char *value;
} cmdalias_t;

static cmdalias_t *cmd_alias;

static bool_t cmd_wait, cmd_closed;

/**
 * @brief The maximum depth of command aliases.
 *
 * This is really for preventing infinite loops in aliases. This is cheaper than name checking.
 */
#define	ALIAS_LOOP_COUNT	16

/* Returns the status of the cmd_closed variable */
bool_t Cmd_IsClosed(void)
{
	return cmd_closed;
}

/*
============
Cmd_Open_f

Reopens the command buffer for writing
============
*/
void Cmd_Open_f(void)
{
	Com_DPrintf("Cmd_Close_f: command buffer opened again\n");
	cmd_closed = false;
}

/*
============
Cmd_Close_f

Will no longer add any command to command buffer
...until cmd_close is qfalse again
Does not affect EXEC_NOW
============
*/
void Cmd_Close_f(void)
{
	Com_DPrintf("Cmd_Close_f: command buffer closed\n");
	cmd_closed = true;
}

bool_t Cmd_IsWaiting(void)
{
	return cmd_wait;
}

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
============
*/
void Cmd_Wait_f(void)
{
	cmd_wait = true;
}

void Cmd_Continue(void)
{
	cmd_wait = false;
}

/*
===============
Cmd_Exec_f
===============
*/
void Cmd_Exec_f(void)
{
	char *f, *f2;
	int len;

	if (Cmd_Argc() != 2) {
		Com_Printf("exec <filename> : execute a script file\n");
		return;
	}

	len = FS_LoadFile(Cmd_Argv(1), (void **) &f);
	if (!f) {
		Com_Printf("couldn't exec %s\n", Cmd_Argv(1));
		return;
	}
	Com_Printf("execing %s\n", Cmd_Argv(1));

	/* the file doesn't have a trailing 0, so we need to copy it off */
	f2 = Z_Malloc(len + 1);
	memcpy(f2, f, len);
	f2[len] = 0;

	Cbuf_ExecuteText(f2, EXEC_INSERT);

	Z_Free(f2);
	FS_FreeFile(f);
}


/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
void Cmd_Echo_f(void)
{
	int i;

	for (i = 1; i < Cmd_Argc(); i++)
		Com_Printf("%s ", Cmd_Argv(i));
	Com_Printf("\n");
}

/*
===============
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; seperated)
===============
*/
void Cmd_Alias_f(void)
{
	cmdalias_t *a;
	char cmd[1024];
	int i, c;
	char *s;

	if (Cmd_Argc() == 1) {
		Com_Printf("Current alias commands:\n");
		for (a = cmd_alias; a; a = a->next)
			Com_Printf("%s : %s\n", a->name, a->value);
		return;
	}

	s = Cmd_Argv(1);
	if (strlen(s) >= MAX_ALIAS_NAME) {
		Com_Printf("Alias name is too long\n");
		return;
	}

	/* if the alias already exists, reuse it */
	for (a = cmd_alias; a; a = a->next) {
		if (!Q_strncmp(s, a->name, MAX_ALIAS_NAME)) {
			Z_Free(a->value);
			break;
		}
	}

	if (!a) {
		a = Z_Malloc(sizeof(cmdalias_t));
		a->next = cmd_alias;
		cmd_alias = a;
	}
	Q_strncpyz(a->name, s, MAX_ALIAS_NAME);

	/* copy the rest of the command line */
	cmd[0] = 0;					/* start out with a null string */
	c = Cmd_Argc();
	for (i = 2; i < c; i++) {
		Q_strcat(cmd, Cmd_Argv(i), sizeof(cmd));
		if (i != (c - 1))
			Q_strcat(cmd, " ", sizeof(cmd));
	}
	Q_strcat(cmd, "\n", sizeof(cmd));

	a->value = CopyString(cmd);
}

/*
=============================================================================

					COMMAND EXECUTION

=============================================================================
*/

typedef struct cmd_function_s {
	struct cmd_function_s *next;
	char *name;
	xcommand_t function;
} cmd_function_t;

static int cmd_argc;
static char *cmd_argv[MAX_STRING_TOKENS];
static char *cmd_null_string = "";
static char cmd_args[MAX_STRING_CHARS];

static cmd_function_t *cmd_functions;	/* possible commands to execute */

/*
============
Cmd_Argc
============
*/
int Cmd_Argc(void)
{
	return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
char *Cmd_Argv(int arg)
{
	if ((unsigned) arg >= cmd_argc)
		return cmd_null_string;
	return cmd_argv[arg];
}

/*
============
Cmd_Args

Returns a single string containing argv(1) to argv(argc()-1)
============
*/
char *Cmd_Args(void)
{
	return cmd_args;
}


/*
======================
Cmd_MacroExpandString
======================
*/
char *Cmd_MacroExpandString(char *text)
{
	int i, j, count, len;
	qboolean inquote;
	char *scan;
	static char expanded[MAX_STRING_CHARS];
	char *token, *start;

	inquote = qfalse;
	scan = text;

	len = strlen(scan);
	if (len >= MAX_STRING_CHARS) {
		Com_Printf("Line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
		return NULL;
	}

	count = 0;

	for (i = 0; i < len; i++) {
		if (scan[i] == '"')
			inquote ^= 1;
		if (inquote)
			continue;			/* don't expand inside quotes */
		if (scan[i] != '$')
			continue;
		/* scan out the complete macro */
		start = scan + i + 1;
		token = COM_Parse(&start);
		if (!start)
			continue;

		token = Cvar_VariableString(token);

		j = strlen(token);
		len += j;
		if (len >= MAX_STRING_CHARS) {
			Com_Printf("Expanded line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
			return NULL;
		}

		Com_sprintf(expanded, MAX_STRING_CHARS, "%s%s%s", scan, token, start);

		scan = expanded;
		i--;

		if (++count == 100) {
			Com_Printf("Macro expansion loop, discarded.\n");
			return NULL;
		}
	}

	if (inquote) {
		Com_Printf("Line has unmatched quote, discarded.\n");
		return NULL;
	}

	return scan;
}


/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
$Cvars will be expanded unless they are in a quoted token
============
*/
void Cmd_TokenizeString(char *text, bool_t macroExpand)
{
	int i;
	char *com_token;

	/* clear the args from the last string */
	for (i = 0; i < cmd_argc; i++)
		Z_Free(cmd_argv[i]);

	cmd_argc = 0;
	cmd_args[0] = 0;

	/* macro expand the text */
	if (macroExpand)
		text = Cmd_MacroExpandString(text);
	if (!text)
		return;

	while (1) {
		/* skip whitespace up to a /n */
		while (*text && *text <= ' ' && *text != '\n') {
			text++;
		}

		if (*text == '\n') {	/* a newline seperates commands in the buffer */
			text++;
			break;
		}

		if (!*text)
			return;

		/* set cmd_args to everything after the first arg */
		if (cmd_argc == 1) {
			int l;

			/* sku - removed potentional buffer overflow vulnerability */
			Q_strncpyz(cmd_args, text, sizeof(cmd_args));

			/* strip off any trailing whitespace */
			l = strlen(cmd_args) - 1;
			for (; l >= 0; l--)
				if (cmd_args[l] <= ' ')
					cmd_args[l] = 0;
				else
					break;
		}

		com_token = COM_Parse(&text);
		if (!text)
			return;

		if (cmd_argc < MAX_STRING_TOKENS) {
			/* check first char of string if it is a variable */
			if (*com_token == '*') {
				com_token++;
				com_token = Cvar_VariableString(com_token);
			}
			cmd_argv[cmd_argc] = Z_Malloc(strlen(com_token) + 1);
			Q_strncpyz(cmd_argv[cmd_argc], com_token, strlen(com_token) + 1);
			cmd_argc++;
		}
	}

}

/**
 * @brief Registers a command with the console interface, with the associated function to call.
 *
 * The cmd_name is referenced later, so it should not be allocated on the stack.
 * If function is NULL, the command will be forwarded to the server as a clc_stringcmd instead of executed locally.
 *
 * @param[in] cmd_name The name of the command.
 * @param[in] function The function to call when the command is executed.
 * @return true if the command is added successfully.
 */
bool_t Cmd_AddCommand(char *cmd_name, xcommand_t function)
{
	cmd_function_t *cmd;

	/* fail if the command is a variable name */
	if (Cvar_VariableString(cmd_name)[0]) {
		Com_Printf("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return false;
	}

	/* fail if the command already exists */
	for (cmd = cmd_functions; cmd; cmd = cmd->next) {
		if (!Q_strcmp(cmd_name, cmd->name)) {
			Com_Printf("Cmd_AddCommand: %s already defined\n", cmd_name);
			return false;
		}
	}

	cmd = Z_Malloc(sizeof(cmd_function_t));
	cmd->name = cmd_name;
	cmd->function = function;
	cmd->next = cmd_functions;
	cmd_functions = cmd;
	return true;
}

/**
 * @brief Removes the specified command.
 *
 * @param[in]
 * @return true if the command is removed, else false.
 * FIXME: Re-write this to get rid of infinite loop.
 */
bool_t Cmd_RemoveCommand(char *cmd_name)
{
	cmd_function_t *cmd, **back;

	back = &cmd_functions;
	while (1) {
		cmd = *back;
		if (!cmd) {
			Com_Printf("Cmd_RemoveCommand: %s not added\n", cmd_name);
			return false;
		}
		if (!Q_strcmp(cmd_name, cmd->name)) {
			*back = cmd->next;
			Z_Free(cmd);
			return true;
		}
		back = &cmd->next;
	}
}

/**
 * @brief Tests that the given command has been registered.
 *
 * @param[in] cmd_name The name of the command to search for.
 * @return true if the command is found, else false.
 */
bool_t Cmd_Exists(char *cmd_name)
{
	cmd_function_t *cmd;

	for (cmd = cmd_functions; cmd; cmd = cmd->next) {
		if (!Q_strcmp(cmd_name, cmd->name)) {
			return true;
		}
	}
	return false;
}

/**
 * @brief Unix like tab completion for console commands.
 *
 * @param[in] partial The beginning of the command we try to complete.
 * @return The name of the the matching command, else NULL.
 * @sa Cvar_CompleteVariable
 * @sa Key_CompleteCommand
 */
const char *Cmd_CompleteCommand(char *partial)
{
	cmd_function_t *cmd;
	cmdalias_t *a;

	const char *match = NULL;
	int len, matches = 0;

	len = strlen(partial);

	if (!len)
		return NULL;

	/* check for exact match in commands */
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!Q_strncmp(partial, cmd->name, sizeof(cmd->name)))
			return cmd->name;

	/* and then aliases */
	for (a = cmd_alias; a; a = a->next)
		if (!Q_strncmp(partial, a->name, len))
			return a->name;

	/* check for partial matches in commands */
	for (cmd = cmd_functions; cmd; cmd = cmd->next) {
		if (!Q_strncmp(partial, cmd->name, len)) {
			Com_Printf("[cmd] %s\n", cmd->name);
			match = cmd->name;
			matches++;
		}
	}

	/* and then aliases */
	for (a = cmd_alias; a; a = a->next) {
		if (strstr(a->name, partial)) {
			Com_Printf("[ali] %s\n", a->name);
			match = a->name;
			matches++;
		}
	}

	return (matches == 1) ? match : NULL;
}


/**
 * @brief Execute a command string immediately.
 *
 * FIXME: lookupnoadd the token to speed search?
 * @param[in] text The command string to execute.
 */
void Cmd_ExecuteString(char *text)
{
	cmd_function_t *cmd;
	cmdalias_t *a;
	int alias_count = 0;

	Cmd_TokenizeString(text, qtrue);

	/* execute the command line */
	if (!Cmd_Argc())
		/* no tokens */
		return;

	/* check functions */
	for (cmd = cmd_functions; cmd; cmd = cmd->next) {
		if (!Q_strcasecmp(cmd_argv[0], cmd->name)) {
			if (!cmd->function) {	/* forward to server command */
				Cmd_ExecuteString(va("cmd %s", text));
			} else
				cmd->function();
			return;
		}
	}

	/* check alias */
	for (a = cmd_alias; a; a = a->next) {
		if (!Q_strcasecmp(cmd_argv[0], a->name)) {
			if (++alias_count == ALIAS_LOOP_COUNT) {
				Com_Printf("ALIAS_LOOP_COUNT\n");
				return;
			}
			Cbuf_ExecuteText(a->value, EXEC_INSERT);
			return;
		}
	}

	/* check cvars */
	if (Cvar_Command())
		return;

	/* send it as a server command if we are connected */
	Cmd_ForwardToServer();
}

/*
============
Cmd_List_f
============
*/
void Cmd_List_f(void)
{
	cmd_function_t *cmd;
	int i, c, l = 0;
	char *token = NULL;

	c = Cmd_Argc();

	if (c == 2) {
		token = Cmd_Argv(1);
		l = strlen(token);
	}

	i = 0;
	for (cmd = cmd_functions; cmd; cmd = cmd->next, i++) {
		if (c == 2 && Q_strncmp(cmd->name, token, l)) {
			i--;
			continue;
		}
		Com_Printf("%s\n", cmd->name);
	}
	Com_Printf("%i commands\n", i);
}

/*
============
Cmd_Init
============
*/
bool_t Cmd_Init(void)
{
	/* register our commands */
	Cmd_AddCommand("cmdlist", Cmd_List_f);
	Cmd_AddCommand("exec", Cmd_Exec_f);
	Cmd_AddCommand("echo", Cmd_Echo_f);
	Cmd_AddCommand("alias", Cmd_Alias_f);
	Cmd_AddCommand("wait", Cmd_Wait_f);
	Cmd_AddCommand("cmdclose", Cmd_Close_f);
	Cmd_AddCommand("cmdopen", Cmd_Open_f);
	return true;
}
