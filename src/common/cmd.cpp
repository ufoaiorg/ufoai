/**
 * @file
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

#include "cmd.h"
#include "common.h"
#include "msg.h"
#include "../shared/parse.h"

void Cmd_ForwardToServer(void);
#define ALIAS_HASH_SIZE	32

#define	MAX_ALIAS_NAME	32

typedef struct cmd_alias_s {
	char name[MAX_ALIAS_NAME];
	char* value;
	bool archive;	/**< store the alias and reload it on the next game start */
	struct cmd_alias_s* hash_next;
	struct cmd_alias_s* next;
} cmd_alias_t;

typedef std::vector<CmdListenerPtr> CmdListeners;

static CmdListeners cmdListeners;
static cmd_alias_t* cmd_alias;
static cmd_alias_t* cmd_alias_hash[ALIAS_HASH_SIZE];
static bool cmdWait;
static bool cmdClosed;

#define	ALIAS_LOOP_COUNT	16
static int alias_count;				/* for detecting runaway loops */


/**
 * @brief Reopens the command buffer for writing
 * @sa Cmd_Close_f
 */
static void Cmd_Open_f (void)
{
	Com_DPrintf(DEBUG_COMMANDS, "Cmd_Close_f: command buffer opened again\n");
	cmdClosed = false;
}

/**
 * @brief Will no longer add any command to command buffer
 * ...until cmd_close is false again
 * @sa Cmd_Open_f
 */
static void Cmd_Close_f (void)
{
	Com_DPrintf(DEBUG_COMMANDS, "Cmd_Close_f: command buffer closed\n");
	cmdClosed = true;
}

/**
 * @brief Causes execution of the remainder of the command buffer to be delayed until
 * next frame. This allows commands like:
 * bind g "impulse 5; +attack; wait; -attack; impulse 2"
 */
static void Cmd_Wait_f (void)
{
	cmdWait = true;
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
 * @brief allocates an initial text buffer that will grow as needed
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
	SZ_Init(&cmd_text, nullptr, 0);
}

/**
 * @brief Adds command text at the end of the buffer
 * @note Normally when a command is generated from the console or keyBindings, it will be added to the end of the command buffer.
 */
void Cbuf_AddText (const char* format, ...)
{
	va_list argptr;
	static char text[CMD_BUFFER_SIZE];
	va_start(argptr, format);
	Q_vsnprintf(text, sizeof(text), format, argptr);
	va_end(argptr);

	if (cmdClosed) {
		if (!Q_strstart(text, "cmdopen")) {
			Com_DPrintf(DEBUG_COMMANDS, "Cbuf_AddText: currently closed\n");
			return;
		}
	}

	const int len = strlen(text);

	if (cmd_text.cursize + len >= cmd_text.maxsize) {
		Com_Printf("Cbuf_AddText: overflow (%i) (%s)\n", cmd_text.maxsize, text);
		Com_Printf("buffer content: %s\n", cmd_text_buf);
		return;
	}
	SZ_Write(&cmd_text, text, len);
}


/**
 * @brief Adds command text immediately after the current command
 * @note Adds a @c \\n to the text
 * @todo actually change the command buffer to do less copying
 */
void Cbuf_InsertText (const char* text)
{
	char* temp;
	int templen;

	if (Q_strnull(text))
		return;

	/* copy off any commands still remaining in the exec buffer */
	templen = cmd_text.cursize;
	if (templen) {
		temp = Mem_AllocTypeN(char, templen);
		memcpy(temp, cmd_text.data, templen);
		SZ_Clear(&cmd_text);
	} else {
		temp = nullptr;			/* shut up compiler */
	}

	/* add the entire text of the file */
	Cbuf_AddText("%s\n", text);

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
	/* don't allow infinite alias loops */
	alias_count = 0;

	while (cmd_text.cursize) {
		unsigned int i;
		char line[1024];

		/* find a \n or ; line break */
		char* text = (char*) cmd_text.data;
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
		Cmd_ExecuteString("%s", line);

		if (cmdWait) {
			/* skip out while text still remains in buffer, leaving it
			 * for next frame */
			cmdWait = false;
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
void Cbuf_AddEarlyCommands (bool clear)
{
	const int argc = Com_Argc();
	for (int i = 1; i < argc; i++) {
		const char* s = Com_Argv(i);
		if (!Q_streq(s, "+set"))
			continue;
		Cbuf_AddText("set %s %s\n", Com_Argv(i + 1), Com_Argv(i + 2));
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
bool Cbuf_AddLateCommands (void)
{
	/* build the combined string to parse from */
	int s = 0;
	const int argc = Com_Argc();
	for (int i = 1; i < argc; i++) {
		s += strlen(Com_Argv(i)) + 1;
	}
	if (!s)
		return false;

	char* text = Mem_AllocTypeN(char, s + 1);
	for (int i = 1; i < argc; i++) {
		Q_strcat(text, s, "%s", Com_Argv(i));
		if (i != argc - 1)
			Q_strcat(text, s, " ");
	}

	/* pull out the commands */
	char* build = Mem_AllocTypeN(char, s + 1);

	for (int i = 0; i < s - 1; i++) {
		if (text[i] != '+')
			continue;
		i++;

		int j;
		for (j = i; text[j] != '+' && text[j] != '-' && text[j] != '\0'; j++) {}

		const char c = text[j];
		text[j] = '\0';

		Q_strcat(build, s, "%s\n", text + i);
		text[j] = c;
		i = j - 1;
	}

	const bool ret = build[0] != '\0';
	if (ret)
		Cbuf_AddText("%s", build);

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
	byte* f;
	char* f2;
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
	f2 = Mem_AllocTypeN(char, len + 1);
	memcpy(f2, f, len);
	/* make really sure that there is a newline */
	f2[len] = 0;

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
	cmd_alias_t* a;

	if (Cmd_Argc() == 1) {
		Com_Printf("Current alias commands:\n");
		for (a = cmd_alias; a; a = a->next)
			Com_Printf("%s : %s\n", a->name, a->value);
		return;
	}

	const char* s = Cmd_Argv(1);
	const size_t len = strlen(s);
	if (len == 0)
		return;

	if (len >= MAX_ALIAS_NAME) {
		Com_Printf("Alias name is too long\n");
		return;
	}

	/* if the alias already exists, reuse it */
	unsigned int hash = Com_HashKey(s, ALIAS_HASH_SIZE);
	for (a = cmd_alias_hash[hash]; a; a = a->hash_next) {
		if (Q_streq(s, a->name)) {
			Mem_Free(a->value);
			break;
		}
	}

	if (!a) {
		a = Mem_PoolAllocType(cmd_alias_t, com_aliasSysPool);
		a->next = cmd_alias;
		/* cmd_alias_hash should be null on the first run */
		a->hash_next = cmd_alias_hash[hash];
		cmd_alias_hash[hash] = a;
		cmd_alias = a;
	}
	Q_strncpyz(a->name, s, sizeof(a->name));

	/* copy the rest of the command line */
	char cmd[MAX_STRING_CHARS];
	cmd[0] = 0;					/* start out with a null string */
	int c = Cmd_Argc();
	for (int i = 2; i < c; i++) {
		Q_strcat(cmd, sizeof(cmd), "%s", Cmd_Argv(i));
		if (i != (c - 1))
			Q_strcat(cmd, sizeof(cmd), " ");
	}

	if (Q_streq(Cmd_Argv(0), "aliasa"))
		a->archive = true;

	a->value = Mem_PoolStrDup(cmd, com_aliasSysPool, 0);
}

/**
 * @brief Write lines containing "aliasa alias value" for all aliases
 * with the archive flag set to true
 * @param f Filehandle to write the aliases to
 */
void Cmd_WriteAliases (qFILE* f)
{
	for (cmd_alias_t* a = cmd_alias; a; a = a->next)
		if (a->archive) {
			FS_Printf(f, "aliasa %s \"", a->name);
			for (int i = 0; i < strlen(a->value); i++) {
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
	struct cmd_function_s* next;
	struct cmd_function_s* hash_next;
	const char* name;
	const char* description;
	xcommand_t function;
	int (*completeParam) (const char* partial, const char** match);
	void* userdata;

	inline const char* getName() const {
		return name;
	}
} cmd_function_t;

static int cmd_argc;
static char* cmd_argv[MAX_STRING_TOKENS];
static char cmd_args[MAX_STRING_CHARS];
static void* cmd_userdata;

static cmd_function_t* cmd_functions;	/* possible commands to execute */
static cmd_function_t* cmd_functions_hash[CMD_HASH_SIZE];

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
const char* Cmd_Argv (int arg)
{
	if (arg >= cmd_argc)
		return "";
	return cmd_argv[arg];
}

/**
 * @brief Returns a single string containing argv(1) to argv(argc()-1)
 */
const char* Cmd_Args (void)
{
	return cmd_args;
}

/**
 * @brief Return the userdata of the called command
 */
void* Cmd_Userdata (void)
{
	return cmd_userdata;
}

/**
 * @brief Clears the argv vector and set argc to zero
 * @sa Cmd_TokenizeString
 */
void Cmd_BufClear (void)
{
	/* clear the args from the last string */
	for (int i = 0; i < cmd_argc; i++) {
		Mem_Free(cmd_argv[i]);
		cmd_argv[i] = nullptr;
	}

	cmd_argc = 0;
	cmd_args[0] = 0;
	cmd_userdata = nullptr;
}

/**
 * @brief Parses the given string into command line tokens.
 * @note @c cmd_argv and @c cmd_argv are filled and set here
 * @note *cvars will be expanded unless they are in a quoted token
 * @sa Com_MacroExpandString
 * @param[in] text The text to parse and tokenize. Must be null terminated. Does not need to be "\n" terminated.
 * @param[in] macroExpand expand cvar string with their values
 * @param[in] replaceWhitespaces Replace "\\t" and "\\n" to "\t" and "\n"
 */
void Cmd_TokenizeString (const char* text, bool macroExpand, bool replaceWhitespaces)
{
	const char* expanded;

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

		const char* com_token = Com_Parse(&text, nullptr, 0, replaceWhitespaces);
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

static cmd_function_t* Cmd_TableFind (const char* cmdName)
{
	unsigned int hash = Com_HashKey(cmdName, CMD_HASH_SIZE);
	for (cmd_function_t* cmd = cmd_functions_hash[hash]; cmd; cmd = cmd->hash_next) {
		if (Q_streq(cmdName, cmd->getName()))
			return cmd;
	}
	return nullptr;
}

/**
 * @brief Returns the command description for a given command
 * @param[in] cmdName Command id in global command array
 * @note never returns a nullptr pointer
 * @todo - search alias, too
 */
const char* Cmd_GetCommandDesc (const char* cmdName)
{
	char searchName[MAX_VAR];

	/* remove parameters */
	Q_strncpyz(searchName, cmdName, sizeof(searchName));
	char* sep = strstr(searchName, " ");
	if (sep)
		*sep = '\0';

	cmd_function_t* cmd = Cmd_TableFind(searchName);
	if (cmd) {
		if (cmd->description)
			return cmd->description;
		return "";
	}
	return "";
}

bool Cmd_GenericCompleteFunction(char const* candidate, char const* partial, char const** match)
{
	static char matchString[MAX_QPATH];

	if (!Q_strstart(candidate, partial))
		return false;

	if (!*match) {
		/* First match. */
		Q_strncpyz(matchString, candidate, sizeof(matchString));
		*match = matchString;
	} else {
		/* Subsequent match, determine common prefix with previous match(es). */
		char*		dst = matchString;
		char const* src = candidate;
		while (*dst == *src) {
			++dst;
			++src;
		}
		*dst = '\0';
	}

	return true;
}

/**
 * @param[in] cmdName The name the command we want to add the complete function
 * @param[in] function The complete function pointer
 * @sa Cmd_AddCommand
 * @sa Cmd_CompleteCommandParameters
 */
void Cmd_AddParamCompleteFunction (const char* cmdName, int (*function)(const char* partial, const char** match))
{
	if (!cmdName || !cmdName[0])
		return;

	cmd_function_t* cmd = Cmd_TableFind(cmdName);
	if (cmd) {
		cmd->completeParam = function;
		return;
	}
}

/**
 * @brief Fetches the userdata for a console command.
 * @param[in] cmdName The name the command we want to add edit
 * @return @c nullptr if no userdata was set or the command wasn't found, the userdata
 * pointer if it was found and set
 * @sa Cmd_AddCommand
 * @sa Cmd_CompleteCommandParameters
 * @sa Cmd_AddUserdata
 */
void* Cmd_GetUserdata (const char* cmdName)
{
	if (!cmdName || !cmdName[0]) {
		Com_Printf("Cmd_GetUserdata: Invalide parameter\n");
		return nullptr;
	}

	cmd_function_t* cmd = Cmd_TableFind(cmdName);
	if (cmd) {
		return cmd->userdata;
	}

	Com_Printf("Cmd_GetUserdata: '%s' not found\n", cmdName);
	return nullptr;
}

/**
 * @brief Adds userdata to the console command.
 * @param[in] cmdName The name the command we want to add edit
 * @param[in] userdata for this function
 * @sa Cmd_AddCommand
 * @sa Cmd_CompleteCommandParameters
 * @sa Cmd_GetUserdata
 */
void Cmd_AddUserdata (const char* cmdName, void* userdata)
{
	if (!cmdName || !cmdName[0])
		return;

	cmd_function_t* cmd = Cmd_TableFind(cmdName);
	if (cmd) {
		cmd->userdata = userdata;
		return;
	}
}

/**
 * @brief Add a new command to the script interface
 * @param[in] cmdName The name the command is available via script interface
 * @param[in] function The function pointer
 * @param[in] desc A short description of what the cmd does. It is shown for e.g. the tab
 * completion or the command list.
 * @sa Cmd_RemoveCommand
 */
void Cmd_AddCommand (const char* cmdName, xcommand_t function, const char* desc)
{
	if (!Q_strvalid(cmdName))
		return;

	/* fail if the command is a variable name */
	if (Cvar_GetString(cmdName)[0]) {
		Com_Printf("Cmd_AddCommand: %s already defined as a var\n", cmdName);
		return;
	}

	/* fail if the command already exists */
	const cmd_function_t* const cmdOld = Cmd_TableFind(cmdName);
	if (cmdOld) {
		Com_DPrintf(DEBUG_COMMANDS, "Cmd_AddCommand: %s already defined\n", cmdName);
		return;
	}

	/* create the new command entry ... */
	cmd_function_t* const cmd = Mem_PoolAllocType(cmd_function_t, com_cmdSysPool);
	cmd->name = cmdName;
	cmd->description = desc;
	cmd->function = function;
	cmd->completeParam = nullptr;

	/* ... add it to the hashtable */
	const unsigned int hash = Com_HashKey(cmdName, CMD_HASH_SIZE);
	HASH_Add(cmd_functions_hash, cmd, hash);
	/* ... and to the cmd table */
	cmd->next = cmd_functions;
	cmd_functions = cmd;

	for (CmdListeners::const_iterator i = cmdListeners.begin(); i != cmdListeners.end(); ++i) {
		(*i)->onAdd(cmdName);
	}
}

/**
 * @brief Removes a command from script interface
 * @param[in] cmdName The script interface function name to remove
 * @sa Cmd_AddCommand
 */
void Cmd_RemoveCommand (const char* cmdName)
{
	cmd_function_t* cmd, **back;
	const unsigned int hash = Com_HashKey(cmdName, CMD_HASH_SIZE);
	back = &cmd_functions_hash[hash];

	while (1) {
		cmd = *back;
		if (!cmd) {
			Com_Printf("Cmd_RemoveCommand: %s not added\n", cmdName);
			return;
		}
		if (Q_streq(cmdName, cmd->getName())) {
			*back = cmd->hash_next;
			break;
		}
		back = &cmd->hash_next;
	}

	back = &cmd_functions;
	while (1) {
		cmd = *back;
		if (!cmd) {
			Com_Printf("Cmd_RemoveCommand: %s not added\n", cmdName);
			return;
		}
		if (Q_streq(cmdName, cmd->getName())) {
			*back = cmd->next;
			Mem_Free(cmd);
			for (CmdListeners::const_iterator i = cmdListeners.begin(); i != cmdListeners.end(); ++i) {
				(*i)->onRemove(cmdName);
			}
			return;
		}
		back = &cmd->next;
	}
}

/**
 * @brief Check both the functiontable and the associated hashtable for invalid entries.
 */
void Cmd_TableCheck (void)
{
#ifdef PARANOID
	int cmdCount = 0;
	for (cmd_function_t* cmd = cmd_functions; cmd; cmd = cmd->next) {
		cmdCount++;
		if (Q_streq("hugo", cmd->getName())) {
			Com_Printf("Cmd_TableCheck: found bad entry\n");
			return;
		}
	}
	int hashCount = 0;
	for (int i = 0; i < CMD_HASH_SIZE; i++) {
		for (cmd_function_t* cmd = cmd_functions_hash[i]; cmd; cmd = cmd->hash_next) {
			hashCount++;
			if (Q_streq("hugo", cmd->getName())) {
				Com_Printf("Cmd_TableCheck: found bad hash entry\n");
				return;
			}
		}
	}
	/* This causes a <path/to/ufo>/(null)/ufoconsole.log to be created just to log this line
	 * file system shutdown has already run */
	Com_Printf("cmdCount: %i hashCount: %i\n", cmdCount, hashCount);
#endif
}

void Cmd_TableAddList (const cmdList_t* cmdList)
{
	for (const cmdList_t* cmd = cmdList; cmd->name; cmd++)
		Cmd_AddCommand(cmd->name, cmd->function, cmd->description);
}

void Cmd_TableRemoveList (const cmdList_t* cmdList)
{
	for (const cmdList_t* cmd = cmdList; cmd->name; cmd++)
		Cmd_RemoveCommand(cmd->name);
}

/**
 * @brief Registers a command listener
 * @param listener The listener callback to register
 */
void Cmd_RegisterCmdListener (CmdListenerPtr listener)
{
	cmdListeners.push_back(listener);
}

/**
 * @brief Unregisters a command listener
 * @param listener The listener callback to unregister
 */
void Cmd_UnRegisterCmdListener (CmdListenerPtr listener)
{
	cmdListeners.erase(std::remove(cmdListeners.begin(), cmdListeners.end(), listener), cmdListeners.end());
}

/**
 * @brief Checks whether a function exists already
 * @param[in] cmdName The script interface function name to search for
 */
bool Cmd_Exists (const char* cmdName)
{
	cmd_function_t* cmd = Cmd_TableFind(cmdName);
	if (cmd)
		return true;
	return false;
}

/**
 * @brief Unix like tab completion for console commands parameters
 * @param[in] command The command we try to complete the parameter for
 * @param[in] partial The beginning of the parameter we try to complete
 * @param[out] match The command we are writing back (if something was found)
 * @sa Cvar_CompleteVariable
 * @sa Key_CompleteCommand
 */
int Cmd_CompleteCommandParameters (const char* command, const char* partial, const char** match)
{
	/* check for partial matches in commands */
	unsigned int hash = Com_HashKey(command, CMD_HASH_SIZE);
	for (const cmd_function_t* cmd = cmd_functions_hash[hash]; cmd; cmd = cmd->hash_next) {
		if (!Q_strcasecmp(command, cmd->getName())) {
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
int Cmd_CompleteCommand (const char* partial, const char** match)
{
	if (partial[0] == '\0')
		return 0;

	int n = 0;

	/* check for partial matches in commands */
	for (cmd_function_t const* cmd = cmd_functions; cmd; cmd = cmd->next) {
		if (Cmd_GenericCompleteFunction(cmd->getName(), partial, match)) {
			Com_Printf("[cmd] %s\n", cmd->getName());
			if (cmd->description)
				Com_Printf(S_COLOR_GREEN "      %s\n", cmd->description);
			++n;
		}
	}

	/* and then aliases */
	for (cmd_alias_t const* a = cmd_alias; a; a = a->next) {
		if (Cmd_GenericCompleteFunction(a->name, partial, match)) {
			Com_Printf("[ali] %s\n", a->name);
			++n;
		}
	}

	return n;
}

void Cmd_vExecuteString (const char* fmt, va_list ap)
{
	char text[1024];
	Q_vsnprintf(text, sizeof(text), fmt, ap);

	Com_DPrintf(DEBUG_COMMANDS, "ExecuteString: '%s'\n", text);

	Cmd_TokenizeString(text, true);

	/* execute the command line */
	if (!Cmd_Argc())
		/* no tokens */
		return;

	const char* str = Cmd_Argv(0);

	/* check functions */
	unsigned int hash = Com_HashKey(str, CMD_HASH_SIZE);
	for (const cmd_function_t* cmd = cmd_functions_hash[hash]; cmd; cmd = cmd->hash_next) {
		if (!Q_strcasecmp(str, cmd->getName())) {
			if (!cmd->function) {	/* forward to server command */
				Cmd_ExecuteString("cmd %s", text);
			} else {
				cmd_userdata = cmd->userdata;
				cmd->function();
			}
			return;
		}
	}

	/* check alias */
	hash = Com_HashKey(str, ALIAS_HASH_SIZE);
	for (const cmd_alias_t* a = cmd_alias_hash[hash]; a; a = a->hash_next) {
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
 * @brief A complete command line has been parsed, so try to execute it
 * @todo lookupnoadd the token to speed search?
 */
void Cmd_ExecuteString (const char* text, ...)
{
	va_list ap;

	va_start(ap, text);
	Cmd_vExecuteString(text, ap);
	va_end(ap);
}

/**
 * @brief Display some help about cmd and cvar usage
 */
static void Cmd_Help_f (void)
{
	Com_Printf(S_COLOR_GREEN "      Use cmdlist and cvarlist. Both accept a partial name.\n");
	Com_Printf(S_COLOR_GREEN "      Note that both lists can vary depending on where in the game you are.\n");
}

/**
 * @brief List all available script interface functions
 */
static void Cmd_List_f (void)
{
	const cmd_function_t* cmd;
	const cmd_alias_t* alias;
	int i = 0, j = 0, c, len = 0;
	const char* token = nullptr;

	c = Cmd_Argc();

	if (c == 2) {
		token = Cmd_Argv(1);
		len = strlen(token);
	}

	for (cmd = cmd_functions; cmd; cmd = cmd->next, i++) {
		if (c == 2 && strncmp(cmd->getName(), token, len)) {
			i--;
			continue;
		}
		Com_Printf("[cmd] %s\n", cmd->getName());
		if (cmd->description)
			Com_Printf(S_COLOR_GREEN "      %s\n", cmd->description);
	}
	/* check alias */
	for (alias = cmd_alias; alias; alias = alias->next, j++) {
		if (c == 2 && strncmp(alias->name, token, len)) {
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
 * @sa Cmd_AddParamCompleteFunction
 */
static int Cmd_CompleteExecCommand (const char* partial, const char** match)
{
	int n = 0;
	while (char const* filename = FS_NextFileFromFileList("*.cfg")) {
		if (Cmd_GenericCompleteFunction(filename, partial, match)) {
			Com_Printf("%s\n", filename);
			++n;
		}
	}
	FS_NextFileFromFileList(nullptr);

	return n;
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
	for (cmd_function_t* cmd = cmd_functions; cmd; cmd = cmd->next) {
		if (!Q_streq(cmd->getName(), "quit"))
			Cmd_ExecuteString("%s", cmd->getName());
	}
}

void Cmd_PrintDebugCommands (void)
{
	const char* otherCommands[] = {"mem_stats", "cl_configstrings", "cl_userinfo", "devmap"};
	int num = lengthof(otherCommands);

	Com_Printf("Debug commands:\n");
	for (const cmd_function_t* cmd = cmd_functions; cmd; cmd = cmd->next) {
		if (Q_strstart(cmd->getName(), "debug_"))
			Com_Printf(" * %s\n   %s\n", cmd->getName(), cmd->description);
	}

	Com_Printf("Other useful commands:\n");
	while (num) {
		const char* desc = Cmd_GetCommandDesc(otherCommands[num - 1]);
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
	Cmd_AddCommand("help", Cmd_Help_f, "Display some help about cmd and cvar usage");
	Cmd_AddCommand("cmdlist", Cmd_List_f, "List all commands to game console");
	Cmd_AddCommand("exec", Cmd_Exec_f, "Execute a script file");
	Cmd_AddParamCompleteFunction("exec", Cmd_CompleteExecCommand);
	Cmd_AddCommand("echo", Cmd_Echo_f, "Print to game console");
	Cmd_AddCommand("wait", Cmd_Wait_f);
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
	cmd_functions = nullptr;

	OBJZERO(cmd_alias_hash);
	cmd_alias = nullptr;
	alias_count = 0;

	OBJZERO(cmd_argv);
	cmd_argc = 0;

	cmdWait = false;
	cmdClosed = false;

	Mem_FreePool(com_cmdSysPool);
}
