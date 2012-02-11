/**
 * @file cl_console.c
 * @brief Console related code.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/client/console.c
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

#include "cl_console.h"
#include "client.h"
#include "cgame/cl_game.h"
#include "input/cl_keys.h"
#include "renderer/r_draw.h"
#include "../shared/utf8.h"

#define ColorIndex(c)	(((c) - '0') & 0x07)

/* set ABGR color values */
static const uint32_t g_color_table[] =
{
	0xFF000000,
	0xFF0000FF,
	0xFF00FF00,
	0xFF00FFFF,
	0xFFFF0000,
	0xFFFFFF00,
	0xFFFF00FF,
	0xFFFFFFFF
};

#define CONSOLE_CHAR_ALIGN 4
#define NUM_CON_TIMES 8
#define CON_TEXTSIZE 32768
#define CONSOLE_CURSOR_CHAR 11
#define CONSOLE_HISTORY_FILENAME "history"

typedef struct {
	qboolean initialized;

	short text[CON_TEXTSIZE];
	int currentLine;			/**< line where next message will be printed */
	int pos;					/**< offset in current line for next print */
	int displayLine;			/**< bottom of console displays this line */

	int lineWidth;				/**< characters across screen */
	int totalLines;				/**< total lines in console scrollback */

	int visLines;
} console_t;

static console_t con;
static cvar_t *con_notifytime;
static cvar_t *con_history;
static cvar_t *con_background;
const int con_fontHeight = 12;
const int con_fontWidth = 10;
const int con_fontShift = 3;

static void Con_Clear (void)
{
	unsigned int i;
	const size_t size = lengthof(con.text);

	for (i = 0; i < size; i++)
		con.text[i] = (CON_COLOR_WHITE << 8) | ' ';
}

/**
 * @param text The character buffer to draw - color encoded
 * @param x The x coordinate on the screen
 * @param y The y coordinate on the screen
 * @param width Characters to draw
 */
static void Con_DrawText (const short *text, int x, int y, size_t width)
{
	unsigned int xPos;
	for (xPos = 0; xPos < width; xPos++) {
		const int currentColor = (text[xPos] >> 8) & 7;
		R_DrawChar(x + ((xPos + 1) << con_fontShift), y, text[xPos], g_color_table[currentColor]);
	}
}

/**
 * @param txt The character buffer to draw
 * @param x The x coordinate on the screen
 * @param y The y coordinate on the screen
 * @param width Characters to draw
 */
void Con_DrawString (const char *txt, int x, int y, unsigned int width)
{
	short buf[512], *pos;
	const size_t size = lengthof(buf);
	char c;
	int color;

	if (width > size || strlen(txt) > size)
		Sys_Error("Overflow in Con_DrawString");

	color = CON_COLOR_WHITE;
	pos = buf;

	while ((c = *txt) != 0) {
		if (Q_IsColorString(txt) ) {
			color = ColorIndex(*(txt + 1));
			txt += 2;
			continue;
		}

		*pos = (color << 8) | c;

		txt++;
		pos++;
	}
	Con_DrawText(buf, x, y, width);
}

static void Key_ClearTyping (void)
{
	keyLines[editLine][1] = 0;	/* clear any typing */
	keyLinePos = 1;
}

void Con_ToggleConsole_f (void)
{
	Key_ClearTyping();

	if (cls.keyDest == key_console) {
		Key_SetDest(key_game);
	} else {
		Key_SetDest(key_console);
	}
}

static void Con_ToggleChat_f (void)
{
	Key_ClearTyping();

	if (cls.keyDest == key_console) {
		if (cls.state == ca_active)
			Key_SetDest(key_game);
	} else
		Key_SetDest(key_console);
}

/**
 * @brief Clears the console buffer
 */
static void Con_Clear_f (void)
{
	Con_Clear();
}

/**
 * @brief Scrolls the console
 * @param[in] scroll Lines to scroll
 */
void Con_Scroll (int scroll)
{
	con.displayLine += scroll;
	if (con.displayLine > con.currentLine)
		con.displayLine = con.currentLine;
	else if (con.displayLine < 0)
		con.displayLine = 0;
}

/**
 * @brief If the line width has changed, reformat the buffer.
 */
void Con_CheckResize (void)
{
	int i, j, oldWidth, oldTotalLines, numLines, numChars;
	short tbuf[CON_TEXTSIZE];
	const int width = (viddef.context.width >> con_fontShift);

	if (width < 1 || width == con.lineWidth)
		return;

	oldWidth = con.lineWidth;
	con.lineWidth = width;
	oldTotalLines = con.totalLines;
	con.totalLines = CON_TEXTSIZE / con.lineWidth;
	numLines = oldTotalLines;

	if (con.totalLines < numLines)
		numLines = con.totalLines;

	numChars = oldWidth;

	if (con.lineWidth < numChars)
		numChars = con.lineWidth;

	memcpy(tbuf, con.text, sizeof(tbuf));
	Con_Clear();

	for (i = 0; i < numLines; i++) {
		for (j = 0; j < numChars; j++) {
			con.text[(con.totalLines - 1 - i) * con.lineWidth + j] = tbuf[((con.currentLine - i + oldTotalLines) % oldTotalLines) * oldWidth + j];
		}
	}

	con.currentLine = con.totalLines - 1;
	con.displayLine = con.currentLine;
}

/**
 * @brief Load the console history
 * @sa Con_SaveConsoleHistory
 */
void Con_LoadConsoleHistory (void)
{
	qFILE f;
	char line[MAXCMDLINE];

	if (!con_history->integer)
		return;

	OBJZERO(f);

	FS_OpenFile(CONSOLE_HISTORY_FILENAME, &f, FILE_READ);
	if (!f.f)
		return;

	/* we have to skip the initial line char and the string end char */
	while (fgets(line, MAXCMDLINE - 2, f.f)) {
		if (line[strlen(line) - 1] == '\n')
			line[strlen(line) - 1] = 0;
		Q_strncpyz(&keyLines[editLine][1], line, MAXCMDLINE - 1);
		editLine = (editLine + 1) % MAXKEYLINES;
		historyLine = editLine;
		keyLines[editLine][1] = 0;
	}

	FS_CloseFile(&f);
}

/**
 * @brief Stores the console history
 * @sa Con_LoadConsoleHistory
 */
void Con_SaveConsoleHistory (void)
{
	int i;
	qFILE f;
	const char *lastLine = NULL;

	/* maybe con_history is not initialized here (early Sys_Error) */
	if (!con_history || !con_history->integer)
		return;

	FS_OpenFile(CONSOLE_HISTORY_FILENAME, &f, FILE_WRITE);
	if (!f.f) {
		Com_Printf("Can not open "CONSOLE_HISTORY_FILENAME" for writing\n");
		return;
	}

	for (i = 0; i < historyLine; i++) {
		if (lastLine && !strncmp(lastLine, &(keyLines[i][1]), MAXCMDLINE - 1))
			continue;

		lastLine = &(keyLines[i][1]);
		if (*lastLine) {
			FS_Write(lastLine, strlen(lastLine), &f);
			FS_Write("\n", 1, &f);
		}
	}
	FS_CloseFile(&f);
}

void Con_Init (void)
{
	Com_Printf("\n----- console initialization -------\n");

	/* register our commands and cvars */
	con_notifytime = Cvar_Get("con_notifytime", "10", CVAR_ARCHIVE, "How many seconds console messages should be shown before they fade away");
	con_history = Cvar_Get("con_history", "1", CVAR_ARCHIVE, "Permanent console history");
	con_background = Cvar_Get("con_background", "1", CVAR_ARCHIVE, "Console is rendered with background image");

	Cmd_AddCommand("toggleconsole", Con_ToggleConsole_f, N_("Show/hide ufoconsole."));
	Cmd_AddCommand("togglechat", Con_ToggleChat_f, NULL);
	Cmd_AddCommand("clear", Con_Clear_f, "Clear console text");

	/* load console history if con_history is true */
	Con_LoadConsoleHistory();

	OBJZERO(con);
	con.lineWidth = VID_NORM_WIDTH / con_fontWidth;
	con.totalLines = lengthof(con.text) / con.lineWidth;
	con.initialized = qtrue;

	Com_Printf("Console initialized.\n");
}


static void Con_Linefeed (void)
{
	int i;

	con.pos = 0;
	if (con.displayLine == con.currentLine)
		con.displayLine++;
	con.currentLine++;

	for (i = 0; i < con.lineWidth; i++)
		con.text[(con.currentLine % con.totalLines) * con.lineWidth + i] = (CON_COLOR_WHITE << 8) | ' ';
}

/**
 * @brief Handles cursor positioning, line wrapping, etc
 * All console printing must go through this in order to be logged to disk
 * If no console is visible, the text will appear at the top of the game window
 * @sa Sys_ConsoleOutput
 */
void Con_Print (const char *txt)
{
	int y;
	int c, l;
	static qboolean cr;
	int color;

	if (!con.initialized)
		return;

	color = CON_COLOR_WHITE;

	while ((c = *txt) != 0) {
		const int charLength = UTF8_char_len(c);
		if (Q_IsColorString(txt) ) {
			color = ColorIndex(*(txt + 1));
			txt += 2;
			continue;
		}

		/* count word length */
		for (l = 0; l < con.lineWidth; l++)
			if (txt[l] <= ' ')
				break;

		/* word wrap */
		if (l != con.lineWidth && (con.pos + l > con.lineWidth))
			con.pos = 0;

		txt += charLength;

		if (cr) {
			con.currentLine--;
			cr = qfalse;
		}

		if (!con.pos) {
			Con_Linefeed();
		}

		if (charLength > 1)
			c = '.';

		switch (c) {
		case '\n':
			con.pos = 0;
			color = CON_COLOR_WHITE;
			break;

		case '\r':
			con.pos = 0;
			color = CON_COLOR_WHITE;
			cr = qtrue;
			break;

		default:	/* display character and advance */
			y = con.currentLine % con.totalLines;
			con.text[y * con.lineWidth + con.pos] = (color << 8) | c;
			con.pos++;
			if (con.pos >= con.lineWidth)
				con.pos = 0;
			break;
		}
	}
}

/*
==============================================================================
DRAWING
==============================================================================
*/

/**
 * @brief Hide the gameconsole if active
 */
void Con_Close (void)
{
	if (cls.keyDest == key_console)
		Key_SetDest(key_game);
}

/**
 * @brief The input line scrolls horizontally if typing goes beyond the right edge
 */
static void Con_DrawInput (void)
{
	int y;
	unsigned int i;
	short editlinecopy[MAXCMDLINE], *text;
	const size_t size = lengthof(editlinecopy);

	if (cls.keyDest != key_console && cls.state == ca_active)
		return;					/* don't draw anything (always draw if not active) */

	y = 0;
	for (i = 0; i < size; i++) {
		editlinecopy[i] = (CON_COLOR_WHITE << 8) | keyLines[editLine][i];
		if (keyLines[editLine][i] == '\0')
			break;
		y++;
	}
	text = editlinecopy;

	/* add the cursor frame */
	if ((int)(CL_Milliseconds() >> 8) & 1) {
		text[keyLinePos] = (CON_COLOR_WHITE << 8) | CONSOLE_CURSOR_CHAR;
		if (keyLinePos == y)
			y++;
	}

	/* fill out remainder with spaces */
	for (i = y; i < size; i++)
		text[i] = (CON_COLOR_WHITE << 8) | ' ';

	/* prestep if horizontally scrolling */
	if (keyLinePos >= con.lineWidth)
		text += 1 + keyLinePos - con.lineWidth;

	/* draw it */
	y = con.visLines - con_fontHeight;

	Con_DrawText(text, 0, y - CONSOLE_CHAR_ALIGN, con.lineWidth);
}

/**
 * @brief Draws the console with the solid background
 */
void Con_DrawConsole (float frac)
{
	int i, x, y;
	int rows, row;
	unsigned int lines;
	short *text;
	char consoleMessage[128];

	lines = viddef.context.height * frac;
	if (lines == 0)
		return;

	if (lines > viddef.context.height)
		lines = viddef.context.height;

	/* draw the background */
	if (con_background->integer)
		R_DrawStretchImage(0, viddef.virtualHeight * (frac - 1) , viddef.virtualWidth, viddef.virtualHeight, R_FindImage("pics/background/conback", it_pic));

	Com_sprintf(consoleMessage, sizeof(consoleMessage), "Hit esc to close - v%s", UFO_VERSION);
	{
		const int len = strlen(consoleMessage);
		const int versionX = viddef.context.width - (len * con_fontWidth) - CONSOLE_CHAR_ALIGN;
		const int versionY = lines - (con_fontHeight + CONSOLE_CHAR_ALIGN);
		const uint32_t color = g_color_table[CON_COLOR_WHITE];

		for (x = 0; x < len; x++)
			R_DrawChar(versionX + x * con_fontWidth, versionY, consoleMessage[x], color);
	}

	/* draw the text */
	con.visLines = lines;

	rows = (lines - con_fontHeight * 2) >> con_fontShift;	/* rows of text to draw */

	y = lines - con_fontHeight * 3;

	/* draw from the bottom up */
	if (con.displayLine != con.currentLine) {
		const uint32_t color = g_color_table[CON_COLOR_GREEN];
		/* draw arrows to show the buffer is backscrolled */
		for (x = 0; x < con.lineWidth; x += 4)
			R_DrawChar((x + 1) << con_fontShift, y, '^', color);

		y -= con_fontHeight;
		rows--;
	}

	row = con.displayLine;
	for (i = 0; i < rows; i++, y -= con_fontHeight, row--) {
		if (row < 0)
			break;
		if (con.currentLine - row >= con.totalLines)
			break;				/* past scrollback wrap point */

		text = con.text + (row % con.totalLines) * con.lineWidth;

		Con_DrawText(text, 0, y, con.lineWidth);
	}

	/* draw the input prompt, user text, and cursor if desired */
	Con_DrawInput();
}
