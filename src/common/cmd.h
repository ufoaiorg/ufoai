/**
 * @file
 * @brief Command text buffering and command execution header
 * @note Any number of commands can be added in a frame, from several different sources.
 * Most commands come from either keyBindings or console line input, but remote
 * servers can also send across commands and entire text files can be executed.
 * @note The + command line options are also added to the command buffer.
 * @note Command execution takes a null terminated string, breaks it into tokens,
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

#pragma once

#include "filesys.h"
#include "../shared/cxx.h"
#include "../shared/sharedptr.h"
#include <stdarg.h>

void Cbuf_Init(void);
void Cbuf_Shutdown(void);
void Cbuf_AddText(const char* format, ...) __attribute__((format(__printf__, 1, 2)));

/**
 * @brief when a command wants to issue other commands immediately, the text is */
/* inserted at the beginning of the buffer, before any remaining unexecuted */
/* commands. */
void Cbuf_InsertText(const char* text);

/**
 * @brief adds all the +set commands from the command line */
void Cbuf_AddEarlyCommands(bool clear);

/**
 * @brief adds all the remaining + commands from the command line
 * @return true if any late commands were added, which
 * will keep the demoloop from immediately starting
 */
bool Cbuf_AddLateCommands(void);

/**
 * @brief Pulls off \n terminated lines of text from the command buffer and sends
 * them through Cmd_ExecuteString.  Stops when the buffer is empty.
 * Normally called once per frame, but may be explicitly invoked.
 * Do not call inside a command function!
 */
void Cbuf_Execute(void);

/**
 * @brief These two functions are used to defer any pending commands while a map
 * is being loaded
 */
void Cbuf_CopyToDefer(void);
void Cbuf_InsertFromDefer(void);

#ifdef DEBUG
void Cmd_PrintDebugCommands(void);
#endif


/*=========================================================================== */


typedef void (*xcommand_t) (void);

typedef struct cmdList_s {
	const char* name;
	xcommand_t function;
	const char* description;
} cmdList_t;

/**
 * @brief Listener for command changes
 * @param cmdName The name of the command that was created.
 */
class CmdListener {
public:
	virtual ~CmdListener() {}
	virtual void onAdd (const char* cmdName) = 0;
	virtual void onRemove (const char* cmdName) = 0;
};

typedef SharedPtr<CmdListener> CmdListenerPtr;

void Cmd_Init(void);
void Cmd_Shutdown(void);

/**
 * @brief called by the init functions of other parts of the program to
 * register commands and functions to call for them.
 * The cmd_name is referenced later, so it should not be in temp memory
 * if function is nullptr, the command will be forwarded to the server
 * as a clc_stringcmd instead of executed locally
 */
void Cmd_AddCommand(const char* cmd_name, xcommand_t function, const char* desc = nullptr);

void Cmd_RemoveCommand(const char* cmd_name);
void Cmd_TableCheck(void);
void Cmd_TableAddList(const cmdList_t* cmdList);
void Cmd_TableRemoveList(const cmdList_t* cmdList);

void Cmd_AddParamCompleteFunction(const char* cmd_name, int (*function)(const char* partial, const char** match));

void Cmd_AddUserdata(const char* cmd_name, void* userdata);
void* Cmd_GetUserdata (const char* cmd_name);

/**
 * Tests whether candidate has partial as prefix and determines the common prefix of all candidates passed to it.
 * @sa Cmd_AddParamCompleteFunction
 * @param[out] match      The found entry of the list we are searching, in case of more than one entry their common suffix is returned.
 * @param[in]  candidate  A completion candidate.
 * @param[in]  partial    The prefix a candidate must have to be considered a match.
 * @returns    true if the candidate matches partial.
 */
bool Cmd_GenericCompleteFunction(char const* candidate, char const* partial, char const** match);

/**
 * @brief used by the cvar code to check for cvar / command name overlap
 */
bool Cmd_Exists(const char* cmd_name);

/**
 * @brief Registers a command listener
 * @param listener The listener callback to register
 */
void Cmd_RegisterCmdListener(CmdListenerPtr listener);

/**
 * @brief Unregisters a command listener
 * @param listener The listener callback to unregister
 */
void Cmd_UnRegisterCmdListener(CmdListenerPtr listener);

/**
 * @brief attempts to match a partial command for automatic command line completion
 * returns nullptr if nothing fits
 */
int Cmd_CompleteCommandParameters(const char* command, const char* partial, const char** match);
int Cmd_CompleteCommand(const char* partial, const char** match);

/**
 * @brief The functions that execute commands get their parameters with these
 * functions. Cmd_Argv() will return an empty string, not a nullptr
 * if arg > argc, so string operations are always safe.
 */
int Cmd_Argc(void);
const char* Cmd_Argv(int arg);
const char* Cmd_Args(void);
void* Cmd_Userdata(void);

void Cmd_TokenizeString(const char* text, bool macroExpand, bool replaceWhitespaces = true);

/**
 * @brief Parses a single line of text into arguments and tries to execute it
 * as if it was typed at the console
 */
void Cmd_ExecuteString(const char* text, ...) __attribute__((format(__printf__, 1, 2)));
void Cmd_vExecuteString(const char* fmt, va_list ap);

/**
 * @brief adds the current command line as a clc_stringcmd to the client message.
 * things like godmode, noclip, etc, are commands directed to the server,
 * so when they are typed in at the console, they will need to be forwarded.
 */
void Cmd_ForwardToServer(void);

/**
 * @brief Searches for the description of a given command
 */
const char* Cmd_GetCommandDesc(const char* command);

/**
 * @brief Clears the command execution buffer
 */
void Cmd_BufClear(void);

/**
 * @brief Writes the persistent aliases to the given filehandle
 * @param f Filehandle to write the aliases to
 */
void Cmd_WriteAliases(qFILE* f);

/**
 * @brief Dummy binding if you don't want unknown commands forwarded to the server
 */
void Cmd_Dummy_f(void);

#include "ScopedCommand.h"
