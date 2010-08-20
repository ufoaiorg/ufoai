/**
 * @file unix_curses.c
 * @brief curses console functions for *nix ports
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

#include "../../common/common.h"

#ifdef HAVE_CURSES
#include "unix_curses.h"
#endif

#ifdef HAVE_CURSES
#include <ncurses.h>
#include "../../server/server.h"
#include "../system.h"

#define CURSES_HISTORYSIZE 64
#define CURSES_SIZE 32768

#define COLOR_DEFAULT 7
#define COLOR_ALT 2

#define CURSES_SCROLL 4

static WINDOW *stdwin;

static int historyline;
static int inputline;
/* indactes what part needs to be redrawn */
static int curses_redraw;
static int curses_scroll, curses_lastline;

static char input[CURSES_HISTORYSIZE][MAXCMDLINE];
static int inputpos = 0;
static char curses_text[CURSES_SIZE];

static char versionstring[MAX_VAR];
static int inputInsert = 0;

/**
 * @brief refresh the console (after window resizement)
 */
static void Curses_Refresh (void)
{
	curses_redraw |= 2;
}

/**
 * @brief check for console input and return a command when a newline is detected
 */
char *Curses_Input (void)
{
	int key;
	int line;

	key = wgetch(stdwin);
	/* No input */
	while (key != ERR) {
		switch (key) {
		/* Return - flush command buffer */
		case '\n':
		case KEY_ENTER:
			/* Mark the end of our input */
			input[inputline][inputpos] = '\0';

			Com_Printf(COLORED_GREEN "]%s\n", input[historyline]);

			/* Flush the command */
			inputpos = 0;
			line = inputline;
			inputline = (inputline + 1) & (CURSES_HISTORYSIZE - 1);
			historyline = inputline;

			Curses_Refresh();

			return input[line];

		case '\t':
		case KEY_STAB:
			if (Com_ConsoleCompleteCommand(input[inputline], input[inputline], MAXCMDLINE, &inputpos, 0))
				curses_redraw |= 1;
			break;

		/* Page Down and Page Up - scrolling */
		case KEY_PPAGE:
			if (curses_scroll < curses_lastline - stdwin->_maxy) {
				/* scroll up */
				curses_scroll += CURSES_SCROLL;
				if (curses_scroll > curses_lastline)
					curses_scroll = curses_lastline;
				Curses_Refresh();
			}
			break;

		case KEY_NPAGE:
			if (curses_scroll > 0) {
				/* scroll down */
				curses_scroll -= CURSES_SCROLL;
				if (curses_scroll < 0)
					curses_scroll = 0;
				Curses_Refresh();
			}
			break;

		case KEY_END:
			while (input[inputline][inputpos]) {
				inputpos++;
			}
			curses_redraw |= 1;
			break;

		case KEY_HOME:
			if (inputpos > 0) {
				inputpos = 0;
				curses_redraw |= 1;
			}
			break;

		case KEY_UP:
			if (*input[(historyline + CURSES_HISTORYSIZE - 1) % CURSES_HISTORYSIZE]) {
				historyline = (historyline + CURSES_HISTORYSIZE - 1) % CURSES_HISTORYSIZE;
				inputpos = 0;
				Com_sprintf(input[inputline], sizeof(input[inputline]), "%s", input[historyline]);
				while (input[inputline][inputpos])
					inputpos++;
				curses_redraw |= 1;
			}
			break;

		case KEY_DOWN:
			if (historyline != inputline) {
				historyline = (historyline + 1) % CURSES_HISTORYSIZE;
				inputpos = 0;
				Com_sprintf(input[inputline], sizeof(input[inputline]), "%s", input[historyline]);
				while (input[inputline][inputpos])
					inputpos++;
				curses_redraw |= 1;
			}
			break;

		case KEY_LEFT:
			if (*input[inputline] && inputpos > 0) {
				inputpos--;
				curses_redraw |= 1;
			}
			break;

		case KEY_RIGHT:
			if (*input[inputline] && inputpos < MAXCMDLINE - 1 && input[inputline][inputpos]) {
				inputpos++;
				curses_redraw |= 1;
			}
			break;

		case KEY_DC:
			if (inputpos < strlen(input[inputline])) {
				strcpy(input[inputline] + inputpos, input[inputline] + inputpos + 1);
				curses_redraw |= 1;
			}
			break;

		/* Backspace - remove character */
		case '\b':
		case 127:
		case KEY_BACKSPACE:
			if (inputpos) {
				strcpy(input[inputline] + inputpos - 1, input[inputline] + inputpos);
				inputpos--;
				curses_redraw |= 1;
			}
			break;

		case KEY_IC:
			inputInsert ^= 1;
			break;

		default:
			/* Basic character input */
			if ((key >= ' ') && (key <= '~') && (inputpos < MAXCMDLINE - 1)) {
				input[inputline][inputpos++] = key;
				curses_redraw |= 1;
			}
		}

		key = wgetch(stdwin);
	}

	Curses_Draw();

	/* Return nothing by default */
	return NULL;
}

void Curses_Resize (void)
{
	endwin();
	refresh();

	Curses_Refresh();
	Curses_Draw();
}


/**
 * @brief Set the curses drawing color
 */
static void Curses_SetColor (int color)
{
	if (!has_colors())
		return;

	color_set(color, NULL);
	if (color == 3 || color == 0)
		attron(A_BOLD);
	else
		attroff(A_BOLD);
}

/**
 * @brief Clear and draw background objects
 */
static void Curses_DrawBackground (void)
{
	Curses_SetColor(COLOR_DEFAULT);
	bkgdset(' ');
	clear();

	/* Draw the frame */
	box(stdwin, ACS_VLINE , ACS_HLINE);

	/* Draw the header */
	Curses_SetColor(COLOR_ALT);
	mvaddstr(0, 2, versionstring);
}

static void Curses_DrawInput (void)
{
	int x;

	Curses_SetColor(COLOR_ALT);
	for (x = 2; x < stdwin->_maxx - 1; x++)
		mvaddstr(stdwin->_maxy, x, " ");

	mvaddnstr(stdwin->_maxy, 3, input[inputline], stdwin->_maxx - 5 < MAXCMDLINE ? stdwin->_maxx - 5 : MAXCMDLINE - 1);

	/* move the cursor to input position */
	wmove(stdwin, stdwin->_maxy, 3 + inputpos);
}

static void Curses_DrawConsole (void)
{
	const int w = stdwin->_maxx;
	const int h = stdwin->_maxy;
	int x, y, firstLine;
	const char *pos;

	if (w < 3 && h < 3)
		return;

	Curses_SetColor(COLOR_DEFAULT);

	y = 1;
	x = 1;
	pos = curses_text;

	firstLine = max(0, curses_lastline - curses_scroll - h);

	while (*pos) {
		if (*pos == 1) {
			if (y >= abs(curses_scroll))
				Curses_SetColor(COLOR_ALT);
			pos++;
		} else if (*pos == 2) {
			if (y >= abs(curses_scroll))
				Curses_SetColor(COLOR_ALT);
			pos++;
		} else if (*pos == '\n') {
			x = 1;
			y++;
			Curses_SetColor(COLOR_DEFAULT);
		} else if (*pos == '\r') {
			/* skip \r */
			x++;
		} else {
			if (x < w && y > firstLine + 1) {
				if (y > firstLine + h)
					break;
				mvaddnstr(y - (firstLine + 1), x, pos, 1);
				x++;
			}
		}
		pos++;
	}

	/* draw a scroll indicator */
	if (curses_lastline > 0) {
		Curses_SetColor(COLOR_ALT);
		mvaddnstr(1 + ((curses_lastline - curses_scroll) * (h - 2) / curses_lastline), w, "O", 1);
	}

	/* reset drawing colors */
	Curses_SetColor(COLOR_DEFAULT);
}

void Curses_Draw (void)
{
	if (curses_redraw) {
		if (curses_redraw & 2) {
			/* Refresh screen */
			Curses_DrawBackground();
			Curses_DrawConsole();
			Curses_DrawInput();
		} else if (curses_redraw & 1) {
			/* Refresh input only */
			Curses_DrawInput();
		}

		wrefresh(stdwin);

		curses_redraw = 0;
	}
}

/**
 * @brief print a message to the curses console
 */
void Curses_Output (const char *msg)
{
	const char *pos;

	/* prevent overflow, text should still have a reasonable size */
	if (strlen(curses_text) + strlen(msg) >= CURSES_SIZE) {
		memcpy(curses_text, curses_text + (sizeof(curses_text) >> 1), sizeof(curses_text) >> 1);
		memset(curses_text + (sizeof(curses_text) >> 1), 0, sizeof(curses_text) >> 1);
		curses_lastline = 0;
		pos = curses_text;
		while (pos) {
			pos = strstr(pos, "\n");
			if (pos) {
				pos++;
				curses_lastline++;
			}
		}
	}

	pos = msg;
	while (pos) {
		pos = strstr(pos, "\n");
		if (pos) {
			pos++;
			curses_lastline++;
		}
	}

	Q_strcat(curses_text, msg, sizeof(curses_text));

	Curses_Refresh();
}

/**
 * @sa Curses_Init
 */
void Curses_Shutdown (void)
{
	endwin();
}

/**
 * @sa Curses_Shutdown
 */
void Curses_Init (void)
{
	int i;

	stdwin = initscr();			/* initialize the ncurses window */
	cbreak();					/* disable input line buffering */
	noecho();					/* don't show type characters */
	keypad(stdwin, TRUE);		/* enable special keys */
	nodelay(stdwin, TRUE); 		/* non-blocking input */
	curs_set(1);				/* enable the cursor */

	curses_scroll = 0;

	if (has_colors() == TRUE) {
		start_color();
		/* this is ncurses-specific */
		use_default_colors();
		/* COLOR_PAIR(0) is terminal default */
		init_pair(1, COLOR_RED, -1);
		init_pair(2, COLOR_GREEN, -1);
		init_pair(3, COLOR_YELLOW, -1);
		init_pair(4, COLOR_BLUE, -1);
		init_pair(5, COLOR_CYAN, -1);
		init_pair(6, COLOR_MAGENTA, -1);
		init_pair(7, -1, -1);
	}

	/* fill up the version string */
	snprintf(versionstring, sizeof(versionstring), " "GAME_TITLE" %s %s %s %s ", UFO_VERSION, CPUSTRING, __DATE__, BUILDSTRING);

	/* clear the input box */
	inputpos = 0;
	inputline = 0;
	historyline = 0;
	for (i = 0; i < CURSES_HISTORYSIZE; i++)
		memset(input[i], 0, sizeof(input[i]));

	refresh();

	Curses_Draw();
}

#endif /* HAVE_CURSES */
