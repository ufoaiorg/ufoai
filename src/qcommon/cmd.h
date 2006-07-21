/**
 * @brief Console commands.
 * @file cmd.h
 *
 * Command execution. Any number of commands can be added in a frame, from several different sources.
 * Most commands come from either keybindings or console line input, but remote
 * servers can also send across commands and entire text files can be execessed.
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

#ifndef COMMON_CMD_H
#define COMMON_CMD_H

#include "../common/ufotypes.h"

typedef void (*xcommand_t) (void);

typedef struct cmdList_s {
	char *name;
	xcommand_t function;
} cmdList_t;

/** @brief Initialise commands. */
bool_t Cmd_Init(void);

/** @brief Registers a command with the console interface. */
bool_t Cmd_AddCommand(char *cmd_name, xcommand_t function);

bool_t Cmd_RemoveCommand(char *cmd_name);

/* used by the cvar code to check for cvar / command name overlap */
bool_t Cmd_Exists(char *cmd_name);

/* attempts to match a partial command for automatic command line completion */
/* returns NULL if nothing fits */

const char *Cmd_CompleteCommand(char *partial);

/* The functions that execute commands get their parameters with these */
/* functions. Cmd_Argv () will return an empty string, not a NULL */
/* if arg > argc, so string operations are always safe. */

int Cmd_Argc(void);
char *Cmd_Argv(int arg);
char *Cmd_Args(void);

/* Takes a null terminated string.  Does not need to be /n terminated. */
/* breaks the string up into arg tokens. */
void Cmd_TokenizeString(char *text, bool_t macroExpand);

/* Parses a single line of text into arguments and tries to execute it */
/* as if it was typed at the console */
void Cmd_ExecuteString(char *text);

/* adds the current command line as a clc_stringcmd to the client message. */
/* things like godmode, noclip, etc, are commands directed to the server, */
/* so when they are typed in at the console, they will need to be forwarded. */
void Cmd_ForwardToServer(void);

/* Returns the status of the command buffer? */
bool_t Cmd_IsClosed(void);

void Cmd_Continue(void);

bool_t Cmd_IsWaiting(void);

#endif /* COMMON_CMD_H */

