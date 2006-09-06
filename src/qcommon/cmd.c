/**
 * @file cmd.c
 * @brief Script command processing module
 * Command text buffering. Any number of commands can be added in a frame, from several different sources.
 * Most commands come from either keybindings or console line input, but remote
 * servers can also send across commands and entire text files can be accessed.
 *
 * The + command line options are also added to the command buffer.
 *
 * Command execution takes a null terminated string, breaks it into tokens,
 * then searches for a command or variable that matches the first token.
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

void Cmd_ForwardToServer(void);

#define	MAX_ALIAS_NAME	32

typedef struct cmdalias_s {
	struct cmdalias_s *next;
	char name[MAX_ALIAS_NAME];
	char *value;
} cmdalias_t;

static cmdalias_t *cmd_alias;

static qboolean cmd_wait, cmd_closed;

#define	ALIAS_LOOP_COUNT	16
static int alias_count;				/* for detecting runaway loops */


/**
 * @brief Reopens the command buffer for writing
 * @sa Cmd_Close_f
 */
void Cmd_Open_f(void)
{
	Com_DPrintf("Cmd_Close_f: command buffer opened again\n");
	cmd_closed = qfalse;
}

/**
 * @brief Will no longer add any command to command buffer
 * ...until cmd_close is qfalse again
 * Does not affect EXEC_NOW
 * @sa Cmd_Open_f
 */
void Cmd_Close_f(void)
{
	Com_DPrintf("Cmd_Close_f: command buffer closed\n");
	cmd_closed = qtrue;
}

/**
 * @brief Causes execution of the remainder of the command buffer to be delayed until
 * next frame.  This allows commands like:
 * bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
 */
void Cmd_Wait_f(void)
{
	cmd_wait = qtrue;
}


/*
=============================================================================
COMMAND BUFFER
=============================================================================
*/

static sizebuf_t cmd_text;
static byte cmd_text_buf[8192];

static char defer_text_buf[8192];

/**
 * @brief
 * @note The initial buffer will grow as needed.
 */
void Cbuf_Init(void)
{
	SZ_Init(&cmd_text, cmd_text_buf, sizeof(cmd_text_buf));
}

/**
 * @brief Adds command text at the end of the buffer
 * @note Normally when a command is generate from the console or keybinings, it will be added to the end of the command buffer.
 */
void Cbuf_AddText(char *text)
{
	int l;
	char *cmdopen;

	if (cmd_closed && (cmdopen = strstr(text, "cmdopen")) != NULL)
		text = cmdopen;
	else if (cmd_closed) {
		Com_DPrintf("Cbuf_AddText: currently closed\n");
		return;
	}

	l = strlen(text);

	if (cmd_text.cursize + l >= cmd_text.maxsize) {
		Com_Printf("Cbuf_AddText: overflow\n");
		return;
	}
	SZ_Write(&cmd_text, text, strlen(text));
}


/**
 * @brief Adds command text immediately after the current command
 * @note Adds a \n to the text
 * FIXME: actually change the command buffer to do less copying
 */
void Cbuf_InsertText(char *text)
{
	char *temp;
	int templen;

	/* copy off any commands still remaining in the exec buffer */
	templen = cmd_text.cursize;
	if (templen) {
		temp = Z_Malloc(templen);
		memcpy(temp, cmd_text.data, templen);
		SZ_Clear(&cmd_text);
	} else
		temp = NULL;			/* shut up compiler */

	/* add the entire text of the file */
	Cbuf_AddText(text);

	/* add the copied off data */
	if (templen) {
		SZ_Write(&cmd_text, temp, templen);
		Z_Free(temp);
	}
}


/**
 * @brief Defers any outstanding commands.
 *
 * Used when loading a map, for example.
 * Copies then clears the command buffer to a temporary area.
 */
void Cbuf_CopyToDefer(void)
{
	memcpy(defer_text_buf, cmd_text_buf, cmd_text.cursize);
	defer_text_buf[cmd_text.cursize] = 0;
	cmd_text.cursize = 0;
}

/**
 * @brief Copies back any deferred commands.
 */
void Cbuf_InsertFromDefer(void)
{
	Cbuf_InsertText(defer_text_buf);
	defer_text_buf[0] = 0;
}


/**
 * @brief Adds a command to the buffer.
 * @param[in] text The command string to execute.
 * @param[in] exec_when Defines when the command should be executed. One of EXEC_NOW, EXEC_INSERT or EXEC_APPEND.
 * @sa EXEC_NOW
 * @sa EXEC_INSERT
 * @sa EXEC_APPEND
 */
void Cbuf_ExecuteText(int exec_when, char *text)
{
	switch (exec_when) {
	case EXEC_NOW:
		Cmd_ExecuteString(text);
		break;
	case EXEC_INSERT:
		Cbuf_InsertText(text);
		break;
	case EXEC_APPEND:
		Cbuf_AddText(text);
		break;
	default:
		Com_Error(ERR_FATAL, "Cbuf_ExecuteText: bad exec_when");
	}
}

/**
 * @brief
 * @sa Cmd_ExecuteString
 * Pulls off \n terminated lines of text from the command buffer and sends them through Cmd_ExecuteString, stopping when the buffer is empty.
 * Normally called once per frame, but may be explicitly invoked.
 * @note Do not call inside a command function!
 */
void Cbuf_Execute(void)
{
	int i;
	char *text;
	char line[1024];
	int quotes;

	/* don't allow infinite alias loops */
	alias_count = 0;

	while (cmd_text.cursize) {
		/* find a \n or ; line break */
		text = (char *) cmd_text.data;

		quotes = 0;
		for (i = 0; i < cmd_text.cursize; i++) {
			if (text[i] == '"')
				quotes++;
			/* don't break if inside a quoted string */
			if (!(quotes & 1) && text[i] == ';')
				break;
			if (text[i] == '\n')
				break;
		}

		/* sku - removed potentional buffer overflow vulnerability */
		if (i > sizeof(line) - 1)
			i = sizeof(line) - 1;

		memcpy(line, text, i);
		line[i] = 0;

		/* delete the text from the command buffer and move remaining commands down */
		/* this is necessary because commands (exec, alias) can insert data at the */
		/* beginning of the text buffer */

		if (i == cmd_text.cursize)
			cmd_text.cursize = 0;
		else {
			i++;
			cmd_text.cursize -= i;
			memmove(text, text + i, cmd_text.cursize);
		}

		/* execute the command line */
		Cmd_ExecuteString(line);

		if (cmd_wait) {
			/* skip out while text still remains in buffer, leaving it */
			/* for next frame */
			cmd_wait = qfalse;
			break;
		}
	}
}


/**
 * @brief Adds command line parameters as script statements
 * Commands lead with a +, and continue until another +
 * Set commands are added early, so they are guaranteed to be set before
 * the client and server initialize for the first time.
 * Other commands are added late, after all initialization is complete.
 */
void Cbuf_AddEarlyCommands(qboolean clear)
{
	int i;
	char *s;

	for (i = 0; i < COM_Argc(); i++) {
		s = COM_Argv(i);
		if (Q_strncmp(s, "+set", 4))
			continue;
		Cbuf_AddText(va("set %s %s\n", COM_Argv(i + 1), COM_Argv(i + 2)));
		if (clear) {
			COM_ClearArgv(i);
			COM_ClearArgv(i + 1);
			COM_ClearArgv(i + 2);
		}
		i += 2;
	}
}

/**
 * @brief Adds command line parameters as script statements
 * @note Commands lead with a + and continue until another + or -
 * @return true if any late commands were added, which will keep the demoloop from immediately starting
 */
qboolean Cbuf_AddLateCommands(void)
{
	int i, j;
	int s;
	char *text, *build, c;
	int argc;
	qboolean ret;

	/* build the combined string to parse from */
	s = 0;
	argc = COM_Argc();
	for (i = 1; i < argc; i++) {
		s += strlen(COM_Argv(i)) + 1;
	}
	if (!s)
		return qfalse;

	text = Z_Malloc(s + 1);
	text[0] = 0;
	for (i = 1; i < argc; i++) {
		Q_strcat(text, COM_Argv(i), s);
		if (i != argc - 1)
			Q_strcat(text, " ", s);
	}

	/* pull out the commands */
	build = Z_Malloc(s + 1);
	build[0] = 0;

	for (i = 0; i < s - 1; i++) {
		if (text[i] == '+') {
			i++;

			for (j = i; (text[j] != '+') && (text[j] != '-') && (text[j] != 0); j++);

			c = text[j];
			text[j] = 0;

			Q_strcat(build, text + i, s);
			Q_strcat(build, "\n", s);
			text[j] = c;
			i = j - 1;
		}
	}

	ret = (build[0] != 0);
	if (ret)
		Cbuf_AddText(build);

	Z_Free(text);
	Z_Free(build);

	return ret;
}


/*
==============================================================================
SCRIPT COMMANDS
==============================================================================
*/

/**
 * @brief
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

	Cbuf_InsertText(f2);

	Z_Free(f2);
	FS_FreeFile(f);
}


/**
 * @brief Just prints the rest of the line to the console
 */
void Cmd_Echo_f(void)
{
	int i;

	for (i = 1; i < Cmd_Argc(); i++)
		Com_Printf("%s ", Cmd_Argv(i));
	Com_Printf("\n");
}

/**
 * @brief Creates a new command that executes a command string (possibly ; seperated)
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

/**
 * @brief
 * @return the number of arguments
 * @sa Cmd_Argv
 */
int Cmd_Argc(void)
{
	return cmd_argc;
}

/**
 * @brief Returns a given argument
 * @param[in] arg The argument at position arg in cmd_argv
 * @return the argument from cmd_argv
 * @sa Cmd_Argc
 */
char *Cmd_Argv(int arg)
{
	if ((unsigned) arg >= cmd_argc)
		return cmd_null_string;
	return cmd_argv[arg];
}

/**
 * @brief Returns a single string containing argv(1) to argv(argc()-1)
 */
char *Cmd_Args(void)
{
	return cmd_args;
}


/**
 * @brief
 * @sa Cmd_TokenizeString
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


/**
 * @brief Parses the given string into command line tokens.
 * @note $Cvars will be expanded unless they are in a quoted token
 * @sa Cmd_MacroExpandString
 */
void Cmd_TokenizeString(char *text, qboolean macroExpand)
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
 * @brief Add a new command to the script interface
 * @param[in] cmd_name The name the command is available via script interface
 * @param[in] function The function pointer
 * @sa Cmd_RemoveCommand
 */
void Cmd_AddCommand(char *cmd_name, xcommand_t function, char *desc)
{
	cmd_function_t *cmd;

	/* fail if the command is a variable name */
	if (Cvar_VariableString(cmd_name)[0]) {
		Com_Printf("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return;
	}

	/* fail if the command already exists */
	for (cmd = cmd_functions; cmd; cmd = cmd->next) {
		if (!Q_strcmp(cmd_name, cmd->name)) {
			Com_Printf("Cmd_AddCommand: %s already defined\n", cmd_name);
			return;
		}
	}

	cmd = Z_Malloc(sizeof(cmd_function_t));
	cmd->name = cmd_name;
	cmd->function = function;
	cmd->next = cmd_functions;
	cmd_functions = cmd;
}

/**
 * @brief Removes a command from script interface
 * @param[in] cmd_name The script interface function name to remove
 * @sa Cmd_AddCommand
 */
void Cmd_RemoveCommand(char *cmd_name)
{
	cmd_function_t *cmd, **back;

	back = &cmd_functions;
	while (1) {
		cmd = *back;
		if (!cmd) {
			Com_Printf("Cmd_RemoveCommand: %s not added\n", cmd_name);
			return;
		}
		if (!Q_strcmp(cmd_name, cmd->name)) {
			*back = cmd->next;
			Z_Free(cmd);
			return;
		}
		back = &cmd->next;
	}
}

/**
 * @brief Checks whether a function exists already
 * @param[in] cmd_name The script interface function name to search for
 */
qboolean Cmd_Exists(char *cmd_name)
{
	cmd_function_t *cmd;

	for (cmd = cmd_functions; cmd; cmd = cmd->next) {
		if (!Q_strcmp(cmd_name, cmd->name))
			return qtrue;
	}

	return qfalse;
}

/**
  * @brief Unix like tab completion for console commands
  * @param partial The beginning of the command we try to complete
  * @sa Cvar_CompleteVariable
  * @sa Key_CompleteCommand
  */
int Cmd_CompleteCommand(char *partial, char **match)
{
	cmd_function_t *cmd;
	cmdalias_t *a;
	char *localMatch = NULL;
	int len, matches = 0;

	len = strlen(partial);

	if (!len)
		return 0;

	/* check for partial matches in commands */
	for (cmd = cmd_functions; cmd; cmd = cmd->next) {
		if (!Q_strncmp(partial, cmd->name, len)) {
			Com_Printf("[cmd] %s\n", cmd->name);
			localMatch = cmd->name;
			matches++;
		}
	}

	/* and then aliases */
	for (a = cmd_alias; a; a = a->next) {
		if (strstr(a->name, partial)) {
			Com_Printf("[ali] %s\n", a->name);
			localMatch = a->name;
			matches++;
		}
	}

	if (matches == 1)
		*match = localMatch;
	return matches;
}


/**
 * @brief A complete command line has been parsed, so try to execute it
 * FIXME: lookupnoadd the token to speed search?
 */
void Cmd_ExecuteString(char *text)
{
	cmd_function_t *cmd;
	cmdalias_t *a;

#ifdef DEBUG
	Com_DPrintf("ExecuteString: '%s'\n", text);
#endif

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
			Cbuf_InsertText(a->value);
			return;
		}
	}

	/* check cvars */
	if (Cvar_Command())
		return;

	/* send it as a server command if we are connected */
	Cmd_ForwardToServer();
}

/**
 * @brief List all available script interface functions
 */
void Cmd_List_f(void)
{
	cmd_function_t *cmd;
	cmdalias_t *alias;
	int i = 0, j = 0, c, l = 0;
	char *token = NULL;

	c = Cmd_Argc();

	if (c == 2) {
		token = Cmd_Argv(1);
		l = strlen(token);
	}

	for (cmd = cmd_functions; cmd; cmd = cmd->next, i++) {
		if (c == 2 && Q_strncmp(cmd->name, token, l)) {
			i--;
			continue;
		}
		Com_Printf("C %s\n", cmd->name);
	}
	/* check alias */
	for (alias = cmd_alias; alias; alias = alias->next, j++) {
		if (c == 2 && Q_strncmp(alias->name, token, l)) {
			j--;
			continue;
		}
		Com_Printf("M %s\n", alias->name);
	}
	Com_Printf("%i commands\n", i);
	Com_Printf("%i macros\n", j);
}

/**
 * @brief
 */
void Cmd_Init(void)
{
	/* register our commands */
	Cmd_AddCommand("cmdlist", Cmd_List_f, NULL);
	Cmd_AddCommand("exec", Cmd_Exec_f, NULL);
	Cmd_AddCommand("echo", Cmd_Echo_f, NULL);
	Cmd_AddCommand("alias", Cmd_Alias_f, NULL);
	Cmd_AddCommand("wait", Cmd_Wait_f, NULL);
	Cmd_AddCommand("cmdclose", Cmd_Close_f, NULL);
	Cmd_AddCommand("cmdopen", Cmd_Open_f, NULL);
}
