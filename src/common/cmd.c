/**
 * @file cmd.c
 * @brief Script command processing module
 * Command text buffering. Any number of commands can be added in a frame, from several different sources.
 * Most commands come from either keyBindings or console line input, but remote
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

#include "common.h"
#include "msg.h"
#include "../shared/parse.h"

void Cmd_ForwardToServer(void);
#define ALIAS_HASH_SIZE	32

#define	MAX_ALIAS_NAME	32

typedef struct cmd_alias_s {
	char name[MAX_ALIAS_NAME];
	char *value;
	qboolean archive;	/**< store the alias and reload it on the next game start */
	struct cmd_alias_s *hash_next;
	struct cmd_alias_s *next;
} cmd_alias_t;

static cmd_alias_t *cmd_alias;
static cmd_alias_t *cmd_alias_hash[ALIAS_HASH_SIZE];
static qboolean cmdWait;
static qboolean cmdClosed;

#define	ALIAS_LOOP_COUNT	16
static int alias_count;				/* for detecting runaway loops */


/**
 * @brief Reopens the command buffer for writing
 * @sa Cmd_Close_f
 */
static void Cmd_Open_f (void)
{
	Com_DPrintf(DEBUG_COMMANDS, "Cmd_Close_f: command buffer opened again\n");
	cmdClosed = qfalse;
}

/**
 * @brief Will no longer add any command to command buffer
 * ...until cmd_close is qfalse again
 * @sa Cmd_Open_f
 */
static void Cmd_Close_f (void)
{
	Com_DPrintf(DEBUG_COMMANDS, "Cmd_Close_f: command buffer closed\n");
	cmdClosed = qtrue;
}

/**
 * @brief Causes execution of the remainder of the command buffer to be delayed until
 * next frame. This allows commands like:
 * bind g "impulse 5; +attack; wait; -attack; impulse 2"
 */
static void Cmd_Wait_f (void)
{
	cmdWait = qtrue;
}

/*
=============================================================================
COMMAND BUFFER
=============================================================================
*/

#define CMD_BUFFER_SIZE 8192
static sizebuf_t cmd_text;
static byte cmd_text_buf[CMD_BUFFER_SIZE];
static char defer_text_buf[CMD_BUFFER_SIZE];

/**
 * @note The initial buffer will grow as needed.
 */
void Cbuf_Init (void)
{
	SZ_Init(&cmd_text, cmd_text_buf, sizeof(cmd_text_buf));
}

/**
 * @note Reset the Cbuf memory.
 */
void Cbuf_Shutdown (void)
{
	SZ_Init(&cmd_text, NULL, 0);
}

/**
 * @brief Adds command text at the end of the buffer
 * @note Normally when a command is generate from the console or keyBindings, it will be added to the end of the command buffer.
 */
void Cbuf_AddText (const char *text)
{
	int l;

	if (cmdClosed) {
		text = strstr(text, "cmdopen");
		if (text == NULL) {
			Com_DPrintf(DEBUG_COMMANDS, "Cbuf_AddText: currently closed\n");
			return;
		}
	}

	l = strlen(text);

	if (cmd_text.cursize + l >= cmd_text.maxsize) {
		Com_Printf("Cbuf_AddText: overflow (%i) (%s)\n", cmd_text.maxsize, text);
		Com_Printf("buffer content: %s\n", cmd_text_buf);
		return;
	}
	SZ_Write(&cmd_text, text, l);
}


/**
 * @brief Adds command text immediately after the current command
 * @note Adds a @c \\n to the text
 * @todo actually change the command buffer to do less copying
 */
void Cbuf_InsertText (const char *text)
{
	char *temp;
	int templen;

	if (!text || !*text)
		return;

	/* copy off any commands still remaining in the exec buffer */
	templen = cmd_text.cursize;
	if (templen) {
		temp = (char *)Mem_Alloc(templen);
		memcpy(temp, cmd_text.data, templen);
		SZ_Clear(&cmd_text);
	} else
		temp = NULL;			/* shut up compiler */

	/* add the entire text of the file */
	Cbuf_AddText(text);

	/* add the copied off data */
	if (templen) {
		SZ_Write(&cmd_text, temp, templen);
		Mem_Free(temp);
	}
}


/**
 * @brief Defers any outstanding commands.
 *
 * Used when loading a map, for example.
 * Copies then clears the command buffer to a temporary area.
 */
void Cbuf_CopyToDefer (void)
{
	memcpy(defer_text_buf, cmd_text_buf, cmd_text.cursize);
	defer_text_buf[cmd_text.cursize] = 0;
	cmd_text.cursize = 0;
}

/**
 * @brief Copies back any deferred commands.
 */
void Cbuf_InsertFromDefer (void)
{
	Cbuf_InsertText(defer_text_buf);
	defer_text_buf[0] = 0;
}

/**
 * @sa Cmd_ExecuteString
 * Pulls off @c \\n terminated lines of text from the command buffer and sends them
 * through Cmd_ExecuteString, stopping when the buffer is empty.
 * Normally called once per frame, but may be explicitly invoked.
 * @note Do not call inside a command function!
 */
void Cbuf_Execute (void)
{
	unsigned int i;
	char line[1024];

	/* don't allow infinite alias loops */
	alias_count = 0;

	while (cmd_text.cursize) {
		/* find a \n or ; line break */
		char *text = (char *) cmd_text.data;
		int quotes = 0;

		for (i = 0; i < cmd_text.cursize; i++) {
			if (text[i] == '"')
				quotes++;
			/* don't break if inside a quoted string */
			if (!(quotes & 1) && text[i] == ';')
				break;
			if (text[i] == '\n')
				break;
		}

		if (i > sizeof(line) - 1)
			i = sizeof(line) - 1;

		memcpy(line, text, i);
		line[i] = 0;

		/* delete the text from the command buffer and move remaining commands down
		 * this is necessary because commands (exec, alias) can insert data at the
		 * beginning of the text buffer */
		if (i == cmd_text.cursize)
			cmd_text.cursize = 0;
		else {
			i++;
			cmd_text.cursize -= i;
			memmove(text, text + i, cmd_text.cursize);
		}

		/* execute the command line */
		Cmd_ExecuteString(line);

		if (cmdWait) {
			/* skip out while text still remains in buffer, leaving it
			 * for next frame */
			cmdWait = qfalse;
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
 * @sa Cbuf_AddLateCommands
 */
void Cbuf_AddEarlyCommands (qboolean clear)
{
	int i;

	for (i = 1; i < Com_Argc(); i++) {
		const char *s = Com_Argv(i);
		if (!Q_streq(s, "+set"))
			continue;
		Cbuf_AddText(va("set %s %s\n", Com_Argv(i + 1), Com_Argv(i + 2)));
		if (clear) {
			Com_ClearArgv(i);
			Com_ClearArgv(i + 1);
			Com_ClearArgv(i + 2);
		}
		i += 2;
	}
}

/**
 * @brief Adds command line parameters as script statements
 * @note Commands lead with a + and continue until another + or -
 * @return true if any late commands were added
 * @sa Cbuf_AddEarlyCommands
 */
qboolean Cbuf_AddLateCommands (void)
{
	int i, j;
	int s;
	char *text, *build, c;
	int argc;
	qboolean ret;

	/* build the combined string to parse from */
	s = 0;
	argc = Com_Argc();
	for (i = 1; i < argc; i++) {
		s += strlen(Com_Argv(i)) + 1;
	}
	if (!s)
		return qfalse;

	text = (char *)Mem_Alloc(s + 1);
	text[0] = 0;
	for (i = 1; i < argc; i++) {
		Q_strcat(text, Com_Argv(i), s);
		if (i != argc - 1)
			Q_strcat(text, " ", s);
	}

	/* pull out the commands */
	build = (char *)Mem_Alloc(s + 1);
	build[0] = 0;

	for (i = 0; i < s - 1; i++) {
		if (text[i] == '+') {
			i++;

			for (j = i; text[j] != '+' && text[j] != '-' && text[j] != 0; j++) {}

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

	Mem_Free(text);
	Mem_Free(build);

	return ret;
}


/*
==============================================================================
SCRIPT COMMANDS
==============================================================================
*/

static void Cmd_Exec_f (void)
{
	byte *f;
	char *f2;
	int len;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <filename> : execute a script file\n", Cmd_Argv(0));
		return;
	}

	len = FS_LoadFile(Cmd_Argv(1), &f);
	if (!f) {
		Com_Printf("couldn't execute %s\n", Cmd_Argv(1));
		return;
	}
	Com_Printf("executing %s\n", Cmd_Argv(1));

	/* the file doesn't have a trailing 0, so we need to copy it off */
	f2 = (char *)Mem_Alloc(len + 2);
	memcpy(f2, f, len);
	/* make really sure that there is a newline */
	f2[len] = '\n';
	f2[len + 1] = 0;

	Cbuf_InsertText(f2);

	Mem_Free(f2);
	FS_FreeFile(f);
}


/**
 * @brief Just prints the rest of the line to the console
 */
static void Cmd_Echo_f (void)
{
	int i;

	for (i = 1; i < Cmd_Argc(); i++)
		Com_Printf("%s ", Cmd_Argv(i));
	Com_Printf("\n");
}

/**
 * @brief Creates a new command that executes a command string (possibly ; separated)
 */
static void Cmd_Alias_f (void)
{
	cmd_alias_t *a;
	char cmd[MAX_STRING_CHARS];
	size_t len;
	unsigned int hash;
	int i, c;
	const char *s;

	if (Cmd_Argc() == 1) {
		Com_Printf("Current alias commands:\n");
		for (a = cmd_alias; a; a = a->next)
			Com_Printf("%s : %s\n", a->name, a->value);
		return;
	}

	s = Cmd_Argv(1);
	len = strlen(s);
	if (len == 0)
		return;

	if (len >= MAX_ALIAS_NAME) {
		Com_Printf("Alias name is too long\n");
		return;
	}

	/* if the alias already exists, reuse it */
	hash = Com_HashKey(s, ALIAS_HASH_SIZE);
	for (a = cmd_alias_hash[hash]; a; a = a->hash_next) {
		if (Q_streq(s, a->name)) {
			Mem_Free(a->value);
			break;
		}
	}

	if (!a) {
		a = (cmd_alias_t *)Mem_PoolAlloc(sizeof(*a), com_aliasSysPool, 0);
		a->next = cmd_alias;
		/* cmd_alias_hash should be null on the first run */
		a->hash_next = cmd_alias_hash[hash];
		cmd_alias_hash[hash] = a;
		cmd_alias = a;
	}
	Q_strncpyz(a->name, s, sizeof(a->name));

	/* copy the rest of the command line */
	cmd[0] = 0;					/* start out with a null string */
	c = Cmd_Argc();
	for (i = 2; i < c; i++) {
		Q_strcat(cmd, Cmd_Argv(i), sizeof(cmd));
		if (i != (c - 1))
			Q_strcat(cmd, " ", sizeof(cmd));
	}

	if (Q_streq(Cmd_Argv(0), "aliasa"))
		a->archive = qtrue;

	a->value = Mem_PoolStrDup(cmd, com_aliasSysPool, 0);
}

/**
 * @brief Write lines containing "aliasa alias value" for all aliases
 * with the archive flag set to true
 * @param f Filehandle to write the aliases to
 */
void Cmd_WriteAliases (qFILE *f)
{
	cmd_alias_t *a;

	for (a = cmd_alias; a; a = a->next)
		if (a->archive) {
			int i;
			FS_Printf(f, "aliasa %s \"", a->name);
			for (i = 0; i < strlen(a->value); i++) {
				if (a->value[i] == '"')
					FS_Printf(f, "\\\"");
				else
					FS_Printf(f, "%c", a->value[i]);
			}
			FS_Printf(f, "\"\n");
		}
}

/*
=============================================================================
COMMAND EXECUTION
=============================================================================
*/

#define CMD_HASH_SIZE 32

typedef struct cmd_function_s {
	struct cmd_function_s *next;
	struct cmd_function_s *hash_next;
	const char *name;
	const char *description;
	xcommand_t function;
	int (*completeParam) (const char *partial, const char **match);
	void* userdata;
} cmd_function_t;

static int cmd_argc;
static char *cmd_argv[MAX_STRING_TOKENS];
static char cmd_args[MAX_STRING_CHARS];
static void *cmd_userdata;

static cmd_function_t *cmd_functions;	/* possible commands to execute */
static cmd_function_t *cmd_functions_hash[CMD_HASH_SIZE];

/**
 * @brief Return the number of arguments of the current command.
 * "command parameter" will result in a @c argc of 2, not 1.
 * @return the number of arguments including the command itself.
 * @sa Cmd_Argv
 */
int Cmd_Argc (void)
{
	return cmd_argc;
}

/**
 * @brief Returns a given argument
 * @param[in] arg The argument at position arg in cmd_argv. @c 0 will return the command name.
 * @return The argument from @c cmd_argv
 * @sa Cmd_Argc
 */
const char *Cmd_Argv (int arg)
{
	if (arg >= cmd_argc)
		return "";
	return cmd_argv[arg];
}

/**
 * @brief Returns a single string containing argv(1) to argv(argc()-1)
 */
const char *Cmd_Args (void)
{
	return cmd_args;
}

/**
 * @brief Return the userdata of the called command
 */
void *Cmd_Userdata (void)
{
	return cmd_userdata;
}

/**
 * @brief Clears the argv vector and set argc to zero
 * @sa Cmd_TokenizeString
 */
void Cmd_BufClear (void)
{
	int i;

	/* clear the args from the last string */
	for (i = 0; i < cmd_argc; i++) {
		Mem_Free(cmd_argv[i]);
		cmd_argv[i] = NULL;
	}

	cmd_argc = 0;
	cmd_args[0] = 0;
	cmd_userdata = NULL;
}

/**
 * @brief Parses the given string into command line tokens.
 * @note @c cmd_argv and @c cmd_argv are filled and set here
 * @note *cvars will be expanded unless they are in a quoted token
 * @sa Com_MacroExpandString
 * @param[in] text The text to parse and tokenize
 * @param[in] macroExpand expand cvar string with their values
 */
void Cmd_TokenizeString (const char *text, qboolean macroExpand)
{
	const char *com_token, *expanded;

	Cmd_BufClear();

	/* macro expand the text */
	if (macroExpand) {
		expanded = Com_MacroExpandString(text);
		if (expanded)
			text = expanded;
	}

	while (1) {
		/* skip whitespace up to a newline */
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
			Q_strncpyz(cmd_args, text, sizeof(cmd_args));
			Com_Chop(cmd_args);
		}

		com_token = Com_Parse(&text);
		if (!text)
			return;

		if (cmd_argc < MAX_STRING_TOKENS) {
			/* check first char of string if it is a variable */
			if (com_token[0] == '*') {
				com_token++;
				com_token = Cvar_GetString(com_token);
			}
			assert(!cmd_argv[cmd_argc]);
			cmd_argv[cmd_argc] = Mem_PoolStrDup(com_token, com_cmdSysPool, 0);
			cmd_argc++;
		}
	}
}

/**
 * @brief Returns the command description for a given command
 * @param[in] cmd_name Command id in global command array
 * @note never returns a NULL pointer
 * @todo - search alias, too
 */
const char* Cmd_GetCommandDesc (const char* cmd_name)
{
	cmd_function_t *cmd;
	char *sep = NULL;
	unsigned int hash;
	char searchName[MAX_VAR];

	/* remove parameters */
	Q_strncpyz(searchName, cmd_name, sizeof(searchName));
	sep = strstr(searchName, " ");
	if (sep)
		*sep = '\0';

	/* fail if the command already exists */
	hash = Com_HashKey(searchName, CMD_HASH_SIZE);
	for (cmd = cmd_functions_hash[hash]; cmd; cmd = cmd->hash_next) {
		if (Q_streq(searchName, cmd->name)) {
			if (cmd->description)
				return cmd->description;
			else
				return "";
		}
	}
	return "";
}

/**
 * @sa Cmd_AddParamCompleteFunction
 * @param[out] match The found entry of the list we are searching, in case of more than one entry their common suffix is returned.
 * @param[in] len The length of the already typed string (where you are searching entries for in the @c list)
 * @param[in] matches The amount of entries in the @c list parameter
 * @param[in] list The list of entries to search for possible matches
 * @returns the amount of matches
 */
int Cmd_GenericCompleteFunction (size_t len, const char **match, int matches, const char **list)
{
	static char matchString[MAX_QPATH];
	int lenResult = 0;
	int i;

	switch (matches) {
	/* exactly one match */
	case 1:
		*match = list[0];
		lenResult = strlen(list[0]);
		break;
	/* no matches */
	case 0:
		break;
	/* more than one match */
	default:
		/* get the shortest matching string of the results */
		for (lenResult = len;; lenResult++) {
			const char matchChar = list[0][lenResult];
			for (i = 1; i < matches; i++) {
				if (matchChar != list[i][lenResult])
					goto out;
			}
		}
out:
		break;
	}
	/* len is >= 1 here */
	if (matches && len != lenResult) {
		if (lenResult >= MAX_QPATH)
			lenResult = MAX_QPATH - 1;
		Q_strncpyz(matchString, list[0], lenResult + 1);
		*match = matchString;
	}
	return matches;
}

/**
 * @param[in] cmd_name The name the command we want to add the complete function
 * @param[in] function The complete function pointer
 * @sa Cmd_AddCommand
 * @sa Cmd_CompleteCommandParameters
 */
void Cmd_AddParamCompleteFunction (const char *cmd_name, int (*function)(const char *partial, const char **match))
{
	cmd_function_t *cmd;
	unsigned int hash;

	if (!cmd_name || !cmd_name[0])
		return;

	hash = Com_HashKey(cmd_name, CMD_HASH_SIZE);
	for (cmd = cmd_functions_hash[hash]; cmd; cmd = cmd->hash_next) {
		if (Q_streq(cmd_name, cmd->name)) {
			cmd->completeParam = function;
			return;
		}
	}
}

/**
 * @brief Fetches the userdata for a console command.
 * @param[in] cmd_name The name the command we want to add edit
 * @return @c NULL if no userdata was set or the command wasn't found, the userdata
 * pointer if it was found and set
 * @sa Cmd_AddCommand
 * @sa Cmd_CompleteCommandParameters
 * @sa Cmd_AddUserdata
 */
void* Cmd_GetUserdata (const char *cmd_name)
{
	cmd_function_t *cmd;
	unsigned int hash;

	if (!cmd_name || !cmd_name[0]) {
		Com_Printf("Cmd_GetUserdata: Invalide parameter\n");
		return NULL;
	}

	hash = Com_HashKey(cmd_name, CMD_HASH_SIZE);
	for (cmd = cmd_functions_hash[hash]; cmd; cmd = cmd->hash_next) {
		if (Q_streq(cmd_name, cmd->name)) {
			return cmd->userdata;
		}
	}

	Com_Printf("Cmd_GetUserdata: '%s' not found\n", cmd_name);
	return NULL;
}

/**
 * @brief Adds userdata to the console command.
 * @param[in] cmd_name The name the command we want to add edit
 * @param[in] userdata for this function
 * @sa Cmd_AddCommand
 * @sa Cmd_CompleteCommandParameters
 * @sa Cmd_GetUserdata
 */
void Cmd_AddUserdata (const char *cmd_name, void* userdata)
{
	cmd_function_t *cmd;
	unsigned int hash;

	if (!cmd_name || !cmd_name[0])
		return;

	hash = Com_HashKey(cmd_name, CMD_HASH_SIZE);
	for (cmd = cmd_functions_hash[hash]; cmd; cmd = cmd->hash_next) {
		if (Q_streq(cmd_name, cmd->name)) {
			cmd->userdata = userdata;
			return;
		}
	}
}

/**
 * @brief Add a new command to the script interface
 * @param[in] cmd_name The name the command is available via script interface
 * @param[in] function The function pointer
 * @param[in] desc A usually(?) one-line description of what the cmd does
 * @sa Cmd_RemoveCommand
 */
void Cmd_AddCommand (const char *cmd_name, xcommand_t function, const char *desc)
{
	cmd_function_t *cmd;
	unsigned int hash;

	if (!cmd_name || !cmd_name[0])
		return;

	/* fail if the command is a variable name */
	if (Cvar_GetString(cmd_name)[0]) {
		Com_Printf("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return;
	}

	/* fail if the command already exists */
	hash = Com_HashKey(cmd_name, CMD_HASH_SIZE);
	for (cmd = cmd_functions_hash[hash]; cmd; cmd = cmd->hash_next) {
		if (Q_streq(cmd_name, cmd->name)) {
			Com_DPrintf(DEBUG_COMMANDS, "Cmd_AddCommand: %s already defined\n", cmd_name);
			return;
		}
	}

	cmd = (cmd_function_t *)Mem_PoolAlloc(sizeof(*cmd), com_cmdSysPool, 0);
	cmd->name = cmd_name;
	cmd->description = desc;
	cmd->function = function;
	cmd->completeParam = NULL;
	HASH_Add(cmd_functions_hash, cmd, hash);
	cmd->next = cmd_functions;
	cmd_functions = cmd;
}

/**
 * @brief Removes a command from script interface
 * @param[in] cmd_name The script interface function name to remove
 * @sa Cmd_AddCommand
 */
void Cmd_RemoveCommand (const char *cmd_name)
{
	cmd_function_t *cmd, **back;
	unsigned int hash;
	hash = Com_HashKey(cmd_name, CMD_HASH_SIZE);
	back = &cmd_functions_hash[hash];

	while (1) {
		cmd = *back;
		if (!cmd) {
			Com_Printf("Cmd_RemoveCommand: %s not added\n", cmd_name);
			return;
		}
		if (!Q_strcasecmp(cmd_name, cmd->name)) {
			*back = cmd->hash_next;
			break;
		}
		back = &cmd->hash_next;
	}

	back = &cmd_functions;
	while (1) {
		cmd = *back;
		if (!cmd) {
			Com_Printf("Cmd_RemoveCommand: %s not added\n", cmd_name);
			return;
		}
		if (Q_streq(cmd_name, cmd->name)) {
			*back = cmd->next;
			Mem_Free(cmd);
			return;
		}
		back = &cmd->next;
	}
}

/**
 * @brief Checks whether a function exists already
 * @param[in] cmd_name The script interface function name to search for
 */
qboolean Cmd_Exists (const char *cmd_name)
{
	cmd_function_t *cmd;
	unsigned int hash;
	hash = Com_HashKey(cmd_name, CMD_HASH_SIZE);

	for (cmd = cmd_functions_hash[hash]; cmd; cmd = cmd->hash_next) {
		if (Q_streq(cmd_name, cmd->name))
			return qtrue;
	}

	return qfalse;
}

/**
 * @brief Unix like tab completion for console commands parameters
 * @param[in] command The command we try to complete the parameter for
 * @param[in] partial The beginning of the parameter we try to complete
 * @param[out] match The command we are writing back (if something was found)
 * @sa Cvar_CompleteVariable
 * @sa Key_CompleteCommand
 */
int Cmd_CompleteCommandParameters (const char *command, const char *partial, const char **match)
{
	const cmd_function_t *cmd;
	unsigned int hash;

	/* check for partial matches in commands */
	hash = Com_HashKey(command, CMD_HASH_SIZE);
	for (cmd = cmd_functions_hash[hash]; cmd; cmd = cmd->hash_next) {
		if (!Q_strcasecmp(command, cmd->name)) {
			if (!cmd->completeParam)
				return 0;
			return cmd->completeParam(partial, match);
		}
	}
	return 0;
}

/**
 * @brief Unix like tab completion for console commands
 * @param[in] partial The beginning of the command we try to complete
 * @param[out] match The found entry of the list we are searching, in case of more than one entry their common suffix is returned.
 * @sa Cvar_CompleteVariable
 * @sa Key_CompleteCommand
 */
int Cmd_CompleteCommand (const char *partial, const char **match)
{
	const cmd_function_t *cmd;
	const cmd_alias_t *a;
	const char *localMatch[MAX_COMPLETE];
	int len, matches = 0;

	len = strlen(partial);

	if (!len)
		return 0;

	/* check for partial matches in commands */
	for (cmd = cmd_functions; cmd; cmd = cmd->next) {
		if (!strncmp(partial, cmd->name, len)) {
			Com_Printf("[cmd] %s\n", cmd->name);
			if (cmd->description)
				Com_Printf(S_COLOR_GREEN "      %s\n", cmd->description);
			localMatch[matches++] = cmd->name;
			if (matches >= MAX_COMPLETE)
				break;
		}
	}

	/* and then aliases */
	if (matches < MAX_COMPLETE) {
		for (a = cmd_alias; a; a = a->next) {
			if (!strncmp(partial, a->name, len)) {
				Com_Printf("[ali] %s\n", a->name);
				localMatch[matches++] = a->name;
				if (matches >= MAX_COMPLETE)
					break;
			}
		}
	}

	return Cmd_GenericCompleteFunction(len, match, matches, localMatch);
}


/**
 * @brief A complete command line has been parsed, so try to execute it
 * @todo lookupnoadd the token to speed search?
 */
void Cmd_ExecuteString (const char *text)
{
	const cmd_function_t *cmd;
	const cmd_alias_t *a;
	const char *str;
	unsigned int hash;

	Com_DPrintf(DEBUG_COMMANDS, "ExecuteString: '%s'\n", text);

	Cmd_TokenizeString(text, qtrue);

	/* execute the command line */
	if (!Cmd_Argc())
		/* no tokens */
		return;

	str = Cmd_Argv(0);

	/* check functions */
	hash = Com_HashKey(str, CMD_HASH_SIZE);
	for (cmd = cmd_functions_hash[hash]; cmd; cmd = cmd->hash_next) {
		if (!Q_strcasecmp(str, cmd->name)) {
			if (!cmd->function) {	/* forward to server command */
				Cmd_ExecuteString(va("cmd %s", text));
			} else {
				cmd_userdata = cmd->userdata;
				cmd->function();
			}
			return;
		}
	}

	/* check alias */
	hash = Com_HashKey(str, ALIAS_HASH_SIZE);
	for (a = cmd_alias_hash[hash]; a; a = a->hash_next) {
		if (!Q_strcasecmp(str, a->name)) {
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
static void Cmd_List_f (void)
{
	const cmd_function_t *cmd;
	const cmd_alias_t *alias;
	int i = 0, j = 0, c, l = 0;
	const char *token = NULL;

	c = Cmd_Argc();

	if (c == 2) {
		token = Cmd_Argv(1);
		l = strlen(token);
	}

	for (cmd = cmd_functions; cmd; cmd = cmd->next, i++) {
		if (c == 2 && strncmp(cmd->name, token, l)) {
			i--;
			continue;
		}
		Com_Printf("[cmd] %s\n", cmd->name);
		if (cmd->description)
			Com_Printf(S_COLOR_GREEN "      %s\n", cmd->description);
	}
	/* check alias */
	for (alias = cmd_alias; alias; alias = alias->next, j++) {
		if (c == 2 && strncmp(alias->name, token, l)) {
			j--;
			continue;
		}
		Com_Printf("[ali] %s\n", alias->name);
	}
	Com_Printf("%i commands\n", i);
	Com_Printf("%i macros\n", j);
}


/**
 * @brief Autocomplete function for exec command
 * @note This function only lists all cfg files - but doesn't perform any
 * autocomplete step when hitting tab after "exec "
 * @sa Cmd_AddParamCompleteFunction
 */
static int Cmd_CompleteExecCommand (const char *partial, const char **match)
{
	const char *filename;
	size_t len;

	FS_BuildFileList("*.cfg");

	len = strlen(partial);
	if (!len) {
		while ((filename = FS_NextFileFromFileList("*.cfg")) != NULL) {
			Com_Printf("%s\n", filename);
		}
	}

	FS_NextFileFromFileList(NULL);
	return 0;
}

/**
 * @brief Dummy binding if you don't want unknown commands forwarded to the server
 */
void Cmd_Dummy_f (void)
{
}

#ifdef DEBUG
/**
 * @brief Tries to call every command (except the quit command) that's currently
 * in the command list - Use this function to test whether some console bindings
 * produce asserts or segfaults in some situations.
 */
static void Cmd_Test_f (void)
{
	cmd_function_t *cmd;

	for (cmd = cmd_functions; cmd; cmd = cmd->next) {
		if (!Q_streq(cmd->name, "quit"))
			Cmd_ExecuteString(cmd->name);
	}
}

void Cmd_PrintDebugCommands (void)
{
	const cmd_function_t *cmd;
	const char* otherCommands[] = {"mem_stats", "cl_configstrings", "cl_userinfo", "devmap"};
	int num = lengthof(otherCommands);

	Com_Printf("Debug commands:\n");
	for (cmd = cmd_functions; cmd; cmd = cmd->next) {
		if (Q_strstart(cmd->name, "debug_"))
			Com_Printf(" * %s\n   %s\n", cmd->name, cmd->description);
	}

	Com_Printf("Other useful commands:\n");
	while (num) {
		const char *desc = Cmd_GetCommandDesc(otherCommands[num - 1]);
		Com_Printf(" * %s\n   %s\n", otherCommands[num - 1], desc);
		num--;
	}
	Com_Printf(" * sv debug_showall\n"
			"   make everything visible to everyone\n"
			" * sv debug_actorinvlist\n"
			"   Show the whole inv of all actors on the server console\n"
			);
	Com_Printf("\n");
}
#endif

void Cmd_Init (void)
{
	/* register our commands */
	Cmd_AddCommand("cmdlist", Cmd_List_f, "List all commands to game console");
	Cmd_AddCommand("exec", Cmd_Exec_f, "Execute a script file");
	Cmd_AddParamCompleteFunction("exec", Cmd_CompleteExecCommand);
	Cmd_AddCommand("echo", Cmd_Echo_f, "Print to game console");
	Cmd_AddCommand("wait", Cmd_Wait_f, NULL);
	Cmd_AddCommand("alias", Cmd_Alias_f, "Creates a new command that executes a command string");
	Cmd_AddCommand("aliasa", Cmd_Alias_f, "Creates a new, persistent command that executes a command string");
	Cmd_AddCommand("cmdclose", Cmd_Close_f, "Close the command buffer");
	Cmd_AddCommand("cmdopen", Cmd_Open_f, "Open the command buffer again");
#ifdef DEBUG
	Cmd_AddCommand("debug_cmdtest", Cmd_Test_f, "Calls every command in the current list");
#endif
}

void Cmd_Shutdown (void)
{
	OBJZERO(cmd_functions_hash);
	cmd_functions = NULL;

	OBJZERO(cmd_argv);
	cmd_argc = 0;

	Mem_FreePool(com_cmdSysPool);
}
