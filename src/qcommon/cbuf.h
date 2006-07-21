/**
 * @brief Command buffer management.
 * @file cbuf.h
 *
 * Command text buffering. Any number of commands can be added in a frame, from several different sources.
 * Most commands come from either keybindings or console line input, but remote
 * servers can also send across commands and entire text files can be accessed.
 *
 * The + command line options are also added to the command buffer.
 *
 * The game starts with a Cbuf_AddText ("exec quake.rc\n"); Cbuf_Execute ();
 *
 * Command execution takes a null terminated string, breaks it into tokens,
 * then searches for a command or variable that matches the first token.
 */

/*
 * All original material Copyright (C) 2002-2006 UFO: Alien Invasion team.
 *
 * Derived from Quake 2 v3.21: quake2-2.31/qcommon/qcommon.h
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

#ifndef COMMON_CBUF_H
#define COMMON_CBUF_H

#include "../common/ufotypes.h"

#define	EXEC_NOW	0			/* don't return until completed */
#define	EXEC_INSERT	1			/* insert at current position, but don't run yet */
#define	EXEC_APPEND	2			/* add to end of the command buffer */


/**
 * @brief Initiliases command buffer.
 *
 * The initial buffer will grow as needed.
 */
void Cbuf_Init(void);

/**
 * @brief Adds command text at the end of the buffer.
 *
 * Normally when a command is generate from the console or keybinings, it will be added to the end of the command buffer.
 */
/* I think Cbuf_ExecuteText() should be used, instead of using this directly. */
/* Unfortunately, this is used by the server code at the mo. */
void Cbuf_AddText(char *text);


/*
 * @brief Inserts a command immediately after the current command.
 *
 * Adds a \n to the text.
 * FIXME: actually change the command buffer to do less copying.
 * NOTE: Doing this will probably mean using a linked list... sounds like a lot of work.
 */
/*
Disable this for now, as Cbuf_ExecuteText() should be used.

void Cbuf_InsertText(char *text);
*/

/**
 * @brief Adds a command to the buffer.
 *
 * @param[in] text The command string to execute.
 * @param[in] exec_when Defines when the command should be executed. One of EXEC_NOW, EXEC_INSERT or EXEC_APPEND.
 * @sa EXEC_NOW
 * @sa EXEC_INSERT
 * @sa EXEC_APPEND
 */
void Cbuf_ExecuteText(char *text, int exec_when);

/**
 * Adds +set command line parameters as script statements.
 * 
 * Set commands are added early, so they are guaranteed to be set before
 * the client and server initialize for the first time.
 * 
 * Other commands are added late, after all initialization is complete.
 */
void Cbuf_AddEarlyCommands(bool_t clear);

/**
 * @brief Adds command line parameters as script statements.
 *
 * Commands lead with a + and continue until another + or -
 * e.g. quake +vid_ref gl +map amlev1
 *
 * Returns true if any late commands were added, which
 * will keep the demoloop from immediately starting
 */
bool_t Cbuf_AddLateCommands(void);

/**
 * @brief Defers any outstanding commands.
 *
 * Used when loading a map, for example.
 * Copies then clears the command buffer to a temporary area.
 */
void Cbuf_CopyToDefer(void);

/**
 * @brief Copies back any deferred commands.
 */
void Cbuf_InsertFromDefer(void);

/**
 * @brief Executes the command buffer.
 *
 * Pulls off \n terminated lines of text from the command buffer and sends them through Cmd_ExecuteString, stopping when the buffer is empty.
 * Normally called once per frame, but may be explicitly invoked.
 * Do not call inside a command function!
 */
void Cbuf_Execute(void);

#endif /* COMMON_CBUF_H */
