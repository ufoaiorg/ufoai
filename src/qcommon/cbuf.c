/**
 * @file cbuf.c
 * @brief Script command buffering.
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
 *
 * TODO: Double-check that the contents of this file are in the right place; they were derived from cmd.c and qcommon.h
 */

/*
 * All original material Copyright (C) 2002-2006 UFO: Alien Invasion team.
 *
 * Derived from Quake 2 v3.21: quake2-2.31/qcommon/cmd.c
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

#include "cbuf.h"
/* FIXME: Check and minimise inclusions. */
#include "cmd.h"
#include "qcommon.h"

static sizebuf_t cmd_text;
#define CMD_TEXT_BUFFER_SIZE 8196
static char cmd_text_buf[CMD_TEXT_BUFFER_SIZE];
static char defer_text_buf[CMD_TEXT_BUFFER_SIZE];

/**
 * @brief Initiliases command buffer.
 *
 * The initial buffer will grow as needed.
 */
void Cbuf_Init(void)
{
	SZ_Init(&cmd_text, cmd_text_buf, CMD_TEXT_BUFFER_SIZE);
}

/**
 * @brief Adds command text at the end of the buffer.
 *
 * Normally when a command is generated from the console or keybinings, it will be added to the end of the command buffer.
 */
void Cbuf_AddText(char *text)
{
	int l;
	char *cmdopen;

	if (Cmd_IsClosed() && (cmdopen = strstr(text, "cmdopen")) != NULL) {
		text = cmdopen;
	} else if (Cmd_IsClosed()) {
		Com_DPrintf("Cbuf_AddText: currently closed\n");
		return;
	}

	l = strlen(text);

	if (cmd_text.cursize + l >= cmd_text.maxsize) {
		Com_Printf("Cbuf_AddText: overflow\n");
		return;
	}/** @brief This can be used in place of either Cbuf_AddText or Cbuf_InsertText. */
	SZ_Write(&cmd_text, text, strlen(text));
}

/**
 * @brief Inserts a command immediately after the current command.
 *
 * Adds a \n to the text.
 * FIXME: actually change the command buffer to do less copying.
 * NOTE: Doing this will probably mean using a linked list... sounds like a lot of work.
 */
void Cbuf_InsertText(char *text)
{
	char *temp;
	int templen;

	/* copy off any commands still remaining in the exec buffer */
	templen = cmd_text.cursize;
	if (templen) {
		temp = (char*)Z_Malloc(templen);
		memcpy(temp, cmd_text.data, templen);
		SZ_Clear(&cmd_text);
	} else {
		temp = NULL;			/* shut up compiler */
	}

	/* add the entire text of the file */
	Cbuf_AddText(text);

	/* add the copied off data */
	if (templen) {
		SZ_Write(&cmd_text, temp, templen);
		Z_Free(temp);
	}
}

/**
 * @brief Adds a command to the buffer.
 *
 * @param[in] exec_when Defines when the command should be executed. One of EXEC_NOW, EXEC_INSERT or EXEC_APPEND.
 * @param[in] text The command string to execute.
 */
void Cbuf_ExecuteText(char *text, int exec_when)
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
 * Adds +set command line parameters as script statements.
 *
 * Set commands are added early, so they are guaranteed to be set before
 * the client and server initialize for the first time.
 *
 * Other commands are added late, after all initialization is complete.
 */
void Cbuf_AddEarlyCommands(bool_t clear)
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
 * @brief Adds command line parameters as script statements.
 *
 * Commands lead with a + and continue until another + or -
 * e.g. quake +vid_ref gl +map amlev1
 *
 * Returns true if any late commands were added, which
 * will keep the demoloop from immediately starting
 */
bool_t Cbuf_AddLateCommands(void)
{
	int i, j;
	int s;
	char *text, *build, c;
	int argc;
	bool_t ret;

	/* build the combined string to parse from */
	s = 0;
	argc = COM_Argc();
	for (i = 1; i < argc; i++) {
		s += strlen(COM_Argv(i)) + 1;
	}
	if (!s)
		return false;

	text = (char*)Z_Malloc(s + 1);
	text[0] = 0;
	for (i = 1; i < argc; i++) {
		Q_strcat(text, COM_Argv(i), s);
		if (i != argc - 1)
			Q_strcat(text, " ", s);
	}

	/* pull out the commands */
	build = (char*)Z_Malloc(s + 1);
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

/**
 * @brief Defers any outstanding commands.
 *
 * Used when loading a map, for example.
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
 * @brief Executes the command buffer.
 *
 * Pulls off \n terminated lines of text from the command buffer and sends them through Cmd_ExecuteString, stopping when the buffer is empty.
 * Normally called once per frame, but may be explicitly invoked.
 * Do not call inside a command function!
 */
void Cbuf_Execute(void)
{
	int i;
	char *text;
	char line[1024];
	int quotes;

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

		if (Cmd_IsWaiting()) {
			/* skip out while text still remains in buffer, leaving it */
			/* for next frame */
			Cmd_Continue();
			break;
		}
	}
}
