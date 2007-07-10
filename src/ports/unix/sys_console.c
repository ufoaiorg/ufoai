/**
 * @file sys_console.c
 * @brief console functions for *nix ports
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

#include "../../qcommon/qcommon.h"

#ifndef __linux__
#undef HAVE_CURSES
#endif

/* only dedicated servers with curses input */
#ifndef DEDICATED_ONLY
#undef HAVE_CURSES
#endif

/*#define CURSES_COLOR*/

#ifdef HAVE_CURSES
#include <ncurses.h>
#define MAXKEYLINES 32
static WINDOW *win_log, *win_cmd;
static int history_line = 0;
static int edit_line = 0;

#define BUF_LEN 256
static char cmdbuf[MAXKEYLINES][BUF_LEN];
static int cmdbuf_pos = 0;
#define COLOR_MAX 7
#define COLOR_DEFAULT 7
#endif /* HAVE_CURSES */

qboolean stdin_active = qtrue;
cvar_t *nostdout;

#ifdef HAVE_CURSES
/**
 * @brief reset the input field
 */
static void Sys_ConsoleInputReset (void)
{
	werase(win_cmd);
	wmove(win_cmd, 0, 0);
	waddch(win_cmd, ']');

	wrefresh(win_cmd);
}
#endif

/**
 * @brief Shutdown the console
 */
void Sys_ConsoleInputShutdown (void)
{
#ifdef HAVE_CURSES
	delwin(win_log);
	delwin(win_cmd);
	endwin();
#endif
}

/**
 * @brief initialise the console
 */
void Sys_ConsoleInputInit (void)
{
#ifdef HAVE_CURSES
	initscr();

#ifdef CURSES_COLOR
	/* Add colors */
	start_color();

	/* Initialise our palette */
	use_default_colors();
	init_pair(1, COLOR_RED, -1);
	init_pair(2, COLOR_GREEN, -1);
	init_pair(3, COLOR_YELLOW, -1);
	init_pair(4, COLOR_BLUE, -1);
	init_pair(5, COLOR_CYAN, -1);
	init_pair(6, COLOR_MAGENTA, -1);
	init_pair(7, -1, -1);
#endif /* CURSES_COLOR */

	/* Add the log window */
	win_log = newwin(LINES - 1, 0, 0, 0);
	scrollok(win_log, TRUE);

	/* Add the command prompt */
	win_cmd = newwin(1, 0, LINES - 1, 0);
	nodelay(win_cmd, TRUE);
	noecho();
	keypad(win_cmd, TRUE);
	Sys_ConsoleInputReset();
	Com_Printf("use curses\n");
#endif /* HAVE_CURSES */
}

#ifdef HAVE_CURSES
/**
 * @brief refresh the console (after window resizement)
 */
static void Sys_ConsoleRefresh (void)
{
	wrefresh(win_log);
	wrefresh(win_cmd);
}
#endif /* HAVE_CURSES */

#ifdef HAVE_CURSES

/**
 * @brief
 */
void Sys_ConsoleAddHistory (int line)
{
	Sys_ConsoleInputReset();
	Q_strncpyz(cmdbuf[edit_line], cmdbuf[history_line], BUF_LEN);
	cmdbuf_pos = 0;
	while (cmdbuf[edit_line][cmdbuf_pos] != '\0') {
		waddch(win_cmd, cmdbuf[edit_line][cmdbuf_pos]);
		cmdbuf_pos++;
	}
	wrefresh(win_cmd);
}

/**
 * @brief Console completion for command and variables
 * @sa Key_CompleteCommand
 * @sa Cmd_CompleteCommand
 * @sa Cvar_CompleteVariable
 */
static void Sys_ConsoleCompleteCommand (void)
{
	const char *cmd = NULL, *cvar = NULL, *use = NULL, *s;
	int cntCmd = 0, cntCvar = 0, cntParams = 0;
	char cmdLine[BUF_LEN] = "";
	qboolean append = qtrue;
	char *tmp;

	s = cmdbuf[edit_line];
	if (!*s || *s == ' ')
		return;

	/* don't try to search a command or cvar if we are already in the
	 * parameter stage */
	if (strstr(s, " ")) {
		tmp = cmdLine;
		while (*s != ' ')
			*tmp++ = *s++;
		/* terminate the string at whitespace position to seperate the cmd */
		*tmp++ = '\0';
		/* get rid of the whitespace */
		s++;
		cntParams = Cmd_CompleteCommandParameters(cmdLine, s, &cmd);
		if (cntParams == 1) {
			/* append the found parameter */
			Q_strcat(cmdLine, " ", sizeof(cmdLine));
			Q_strcat(cmdLine, cmd, sizeof(cmdLine));
			append = qfalse;
			use = cmdLine;
		} else if (cntParams > 1) {
			Com_Printf("\n");
		} else
			return;
	} else {
		cntCmd = Cmd_CompleteCommand(s, &cmd);
		cntCvar = Cvar_CompleteVariable(s, &cvar);

		if (cntCmd == 1 && !cntCvar)
			use = cmd;
		else if (!cntCmd && cntCvar == 1)
			use = cvar;
		else
			Com_Printf("\n");
	}

	if (use) {
		Sys_ConsoleInputReset();
		Q_strncpyz(cmdbuf[edit_line], use, BUF_LEN);
		cmdbuf_pos = strlen(use);
		if (append) {
			cmdbuf[edit_line][cmdbuf_pos] = ' ';
			cmdbuf_pos++;
		}
		cmdbuf[edit_line][cmdbuf_pos] = 0;
		/* and now add this to the curses command line */
		cmdbuf_pos = 0;
		while (cmdbuf[edit_line][cmdbuf_pos] != '\0') {
			waddch(win_cmd, cmdbuf[edit_line][cmdbuf_pos]);
			cmdbuf_pos++;
		}
		wrefresh(win_cmd);
		return;
	}
}

/**
 * @brief check for console input and return a command when a newline is detected
 */
char *Sys_ConsoleInput (void)
{
	int key = wgetch(win_cmd);
	int line;

	/* No input */
	if (key == ERR)
		return NULL;

	/* Basic character input */
	if ((key >= ' ') && (key <= '~') && (cmdbuf_pos < BUF_LEN - 1)) {
		cmdbuf[edit_line][cmdbuf_pos++] = key;
		waddch(win_cmd, key);
		return NULL;
	}

	wnoutrefresh(win_log);
	wnoutrefresh(win_cmd);

	switch (key) {
	/* Return - flush command buffer */
	case '\n':
		/* Mark the end of our input */
		cmdbuf[edit_line][cmdbuf_pos] = '\0';

		/* Reset our prompt */
		Sys_ConsoleInputReset();

		/* Flush the command */
		cmdbuf_pos = 0;
		line = edit_line;
		edit_line = (edit_line + 1) & (MAXKEYLINES-1);
		history_line = edit_line;

		return cmdbuf[line];

	case '\t':
		Sys_ConsoleCompleteCommand();
		break;

	/* Page Down and Page Up - scrolling */
	case KEY_NPAGE:
		wscrl(win_log, 4);
		wrefresh(win_log);
		break;

	case KEY_PPAGE:
		wscrl(win_log, -4);
		wrefresh(win_log);
		break;

	case KEY_HOME:
		break;

	case KEY_UP:
		do {
			history_line = (history_line - 1) & (MAXKEYLINES-1);
		} while (history_line != edit_line && !cmdbuf[history_line][1]);

		if (history_line == edit_line)
			history_line = (edit_line + 1) & (MAXKEYLINES-1);

		Sys_ConsoleAddHistory(history_line);
		break;

	case KEY_DOWN:
		if (history_line == edit_line)
			break;
		do {
			history_line = (history_line + 1) & (MAXKEYLINES-1);
		} while (history_line != edit_line && !cmdbuf[history_line][0]);

		Sys_ConsoleAddHistory(history_line);
		break;

	case KEY_LEFT:
	case KEY_RIGHT:
		break;

	/* Backspace - remove character */
	case '\b':
	case 127:
	case KEY_BACKSPACE:
		if (cmdbuf_pos) {
			cmdbuf_pos--;
			cmdbuf[edit_line][cmdbuf_pos] = '\0';
			waddstr(win_cmd, "\b \b");
			wrefresh(win_cmd);
		}
		break;

#if 0
	default:
		if ((key < ' ') || (key > '~')) {
			Com_Printf("key: %i\n", key);
		}
#endif
	}

	/* Return nothing by default */
	return NULL;
}

/**
 * @brief print a message to the curses console
 */
void Sys_ConsoleOutput (const char *msg)
{
	int i;
#ifdef CURSES_COLOR
	char col;

	/* Set the default color at the beginning of the line */
	wattron(win_log, COLOR_PAIR(COLOR_DEFAULT));
#endif /* CURSES_COLOR */

	for (i = 0; i < strlen(msg); i++) {
		if (msg[i] == '^') {
#ifdef CURSES_COLOR
			/* A color modifier */
			col = (msg[i+1] - '0') % COLOR_MAX;

			/* Wrap 0 (black) to the default color */
			col = col ? col : COLOR_DEFAULT;

			wattron(win_log, COLOR_PAIR (col));
#endif /* CURSES_COLOR */

			/* Takes one character extra */
			i++;
		} else
			/* A regular character */
			waddch(win_log, msg[i]);
	}

	Sys_ConsoleRefresh();
}

#else /* HAVE_CURSES */

/**
 * @brief
 */
char *Sys_ConsoleInput (void)
{
	static char text[256];
	int len;
	fd_set fdset;
	struct timeval timeout;

	if (!dedicated || !dedicated->integer)
		return NULL;

	if (!stdin_active)
		return NULL;

	FD_ZERO(&fdset);
	FD_SET(0, &fdset); /* stdin */
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if (select (1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET(0, &fdset))
		return NULL;

	len = read (0, text, sizeof(text));
	if (len == 0) { /* eof! */
		stdin_active = qfalse;
		return NULL;
	}

	if (len < 1)
		return NULL;
	text[len-1] = 0;    /* rip off the /n and terminate */

	return text;
}

/**
 * @brief
 */
void Sys_ConsoleOutput (const char *string)
{
	char text[2048];
	int i, j;

	if (nostdout && nostdout->value)
		return;

	i = j = 0;

	/* strip high bits */
	while (string[j]) {
		text[i] = string[j] & SCHAR_MAX;

		/* strip low bits */
		if (text[i] >= 32 || text[i] == '\n' || text[i] == '\t')
			i++;

		j++;

		if (i == sizeof(text) - 2) {
			text[i++] = '\n';
			break;
		}
	}
	text[i] = 0;

	fputs(string, stdout);
}
#endif /* HAVE_CURSES */
