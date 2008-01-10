/**
 * @file console.c
 * @brief Console related code.
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#include "client.h"

console_t con;

#define CONSOLE_CHAR_ALIGN 4

static cvar_t *con_notifytime;
static cvar_t *con_history;
cvar_t *con_font;
int con_fontHeight;
int con_fontWidth;
int con_fontShift;

extern char key_lines[MAXKEYLINES][MAXCMDLINE];
extern int edit_line;
extern int key_linepos;

static void Con_DisplayString (int x, int y, const char *s)
{
	while (*s) {
		R_DrawChar(x, y, *s);
		x += con_fontWidth;
		s++;
	}
}

static void Key_ClearTyping (void)
{
	key_lines[edit_line][1] = 0;	/* clear any typing */
	key_linepos = 1;
}

void Con_ToggleConsole_f (void)
{
	Key_ClearTyping();
	Con_ClearNotify();

	if (cls.key_dest == key_console) {
		Key_SetDest(key_game);
	} else {
		Key_SetDest(key_console);
		/* make sure that we end all input buffers when opening the console */
		if (msg_mode == MSG_MENU)
			Cbuf_AddText("msgmenu !\n");
	}
}

static void Con_ToggleChat_f (void)
{
	Key_ClearTyping();

	if (cls.key_dest == key_console) {
		if (cls.state == ca_active)
			Key_SetDest(key_game);
	} else
		Key_SetDest(key_console);

	Con_ClearNotify();
}

/**
 * @brief Clears the console buffer
 */
static void Con_Clear_f (void)
{
	memset(con.text, ' ', sizeof(con.text));
}


/**
 * @brief Save the console contents out to a file
 */
static void Con_Dump_f (void)
{
	int l, x;
	char *line;
	FILE *f;
	char buffer[MAX_STRING_CHARS];
	char name[MAX_OSPATH];

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <filename>\n", Cmd_Argv(0));
		return;
	}

	Com_sprintf(name, sizeof(name), "%s/%s.txt", FS_Gamedir(), Cmd_Argv(1));

	Com_Printf("Dumped console text to %s.\n", name);
	FS_CreatePath(name);
	f = fopen(name, "w");
	if (!f) {
		Com_Printf("ERROR: couldn't open.\n");
		return;
	}

	/* skip empty lines */
	for (l = con.current - con.totallines + 1; l <= con.current; l++) {
		line = con.text + (l % con.totallines) * con.linewidth;
		for (x = 0; x < con.linewidth; x++)
			if (line[x] != ' ')
				break;
		if (x != con.linewidth)
			break;
	}

	/* write the remaining lines */
	buffer[con.linewidth] = 0;
	for (; l <= con.current; l++) {
		line = con.text + (l % con.totallines) * con.linewidth;
		strncpy(buffer, line, con.linewidth);
		for (x = con.linewidth - 1; x >= 0; x--) {
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}
		for (x = 0; buffer[x]; x++)
			buffer[x] &= SCHAR_MAX;

		fprintf(f, "%s\n", buffer);
	}

	fclose(f);
}


void Con_ClearNotify (void)
{
	int i;

	for (i = 0; i < NUM_CON_TIMES; i++)
		con.times[i] = 0;
}


static void Con_MessageModeSay_f (void)
{
	/* chatting makes only sense in battle field mode */
	if (!CL_OnBattlescape() || ccs.singleplayer)
		return;

	msg_mode = MSG_SAY;
	Key_SetDest(key_message);
}

static void Con_MessageModeSayTeam_f (void)
{
	/* chatting makes only sense in battle field mode */
	if (!CL_OnBattlescape() || ccs.singleplayer)
		return;

	msg_mode = MSG_SAY_TEAM;
	Key_SetDest(key_message);
}

/**
 * @brief Activated the inline cvar editing
 * @note E.g. used in our saving dialog to enter the save game comment
 */
static void Con_MessageModeMenu_f (void)
{
	if (msg_mode != MSG_IRC)
		msg_mode = MSG_MENU;
	Key_SetDest(key_input);
}

/**
 * @brief If the line width has changed, reformat the buffer.
 */
void Con_CheckResize (void)
{
	int i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char tbuf[CON_TEXTSIZE];

	if (con_font->modified) {
		if (con_font->integer == 0) {
			con_fontWidth = 16;
			con_fontHeight = 32;
			con_fontShift = 4;
			con_font->modified = qfalse;
		} else if (con_font->integer == 1) {
			con_fontWidth = 8;
			con_fontHeight = 8;
			con_fontShift = 3;
			con_font->modified = qfalse;
		} else
			Cvar_ForceSet("con_font", "1");
	}

	width = (viddef.width >> con_fontShift);

	if (width == con.linewidth)
		return;

	if (width < 1) {	/* video hasn't been initialized yet */
		width = VID_NORM_WIDTH / con_fontWidth;
		con.linewidth = width;
		con.totallines = sizeof(con.text) / con.linewidth;
		memset(con.text, ' ', sizeof(con.text));
	} else {
		oldwidth = con.linewidth;
		con.linewidth = width;
		oldtotallines = con.totallines;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		numlines = oldtotallines;

		if (con.totallines < numlines)
			numlines = con.totallines;

		numchars = oldwidth;

		if (con.linewidth < numchars)
			numchars = con.linewidth;

		memcpy(tbuf, con.text, CON_TEXTSIZE);
		memset(con.text, ' ', CON_TEXTSIZE);

		for (i = 0; i < numlines; i++) {
			for (j = 0; j < numchars; j++) {
				con.text[(con.totallines - 1 - i) * con.linewidth + j] = tbuf[((con.current - i + oldtotallines) % oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify();
	}

	con.current = con.totallines - 1;
	con.display = con.current;
}

#define CONSOLE_HISTORY_FILENAME "history"

/**
 * @brief Load the console history from cl_consoleHistory
 */
void Con_LoadConsoleHistory (const char* path)
{
	byte *buf;
	char *f2;
	const char *text, *token;
	int len, i = 0;
	char filename[MAX_OSPATH];

	assert(path);

	if (!con_history->integer)
		return;

	Com_sprintf(filename, sizeof(filename), "%s", CONSOLE_HISTORY_FILENAME);

	len = FS_LoadFile(filename, &buf);
	if (!buf) {
		Com_Printf("couldn't load %s\n", filename);
		return;
	}

	/* the file doesn't have a trailing 0, so we need to copy it off */
	f2 = Mem_Alloc(len + 1);
	memcpy(f2, buf, len);
	f2[len] = 0;
	text = f2;

	Com_DPrintf(DEBUG_COMMANDS, "...load console history\n");

	do {
		token = COM_Parse(&text);
		if (!*token || !text)
			break;
		Q_strncpyz(&key_lines[i][1], token, MAXCMDLINE-1);
		Com_DPrintf(DEBUG_COMMANDS, "....command: '%s'\n", token);
		i++;
	} while (token);
	/* only the do not modify comment */
	Mem_Free(f2);
	FS_FreeFile(buf);
	history_line = i;
	edit_line = i;
}

/**
 * @brief Stores the console history
 * @param[in] path path to store the history
 */
void Con_SaveConsoleHistory (const char *path)
{
	int i;
	FILE *f;
	char filename[MAX_OSPATH];
	const char *lastLine = NULL;

	/* in case of a very early sys error */
	if (!path)
		return;

	/* maybe con_history is not initialized here (early Sys_Error) */
	if (!con_history || !con_history->integer)
		return;

	Com_sprintf(filename, sizeof(filename), "%s/%s", path, CONSOLE_HISTORY_FILENAME);

	f = fopen(filename, "w");
	if (!f) {
		Com_Printf("Can not open %s/%s for writing\n", path, CONSOLE_HISTORY_FILENAME);
		return;
	}

	fprintf(f, "// generated by ufo, do not modify\n");
	for (i = 0; i < history_line; i++) {
		if (lastLine && !Q_strncmp(lastLine, &(key_lines[i][1]), MAXCMDLINE)) {
#if 0
			Com_DPrintf(DEBUG_CLIENT, "Don't save '%s' again\n", lastLine);
#endif
		} else {
			lastLine = &(key_lines[i][1]);
			if (*lastLine)
				fprintf(f, "\"%s\"\n", lastLine);
		}
	}
	fclose(f);
}

void Con_Init (void)
{
	Com_Printf("\n----- console initialization -------\n");
	con.linewidth = -1;

	/* register our commands and cvars */
	con_notifytime = Cvar_Get("con_notifytime", "10", CVAR_ARCHIVE, "How many seconds console messages should be shown before they fade away");
	con_history = Cvar_Get("con_history", "1", CVAR_ARCHIVE, "Permanent console history");
	con_font = Cvar_Get("con_font", "1", CVAR_ARCHIVE, "Change the console font - 0 and 1 are valid values");

	con_fontWidth = 16;
	con_fontHeight = 32;
	con_fontShift = 4;

	Cmd_AddCommand("toggleconsole", Con_ToggleConsole_f, _("Bring up the in-game console"));
	Cmd_AddCommand("togglechat", Con_ToggleChat_f, NULL);
	Cmd_AddCommand("messagesay", Con_MessageModeSay_f, _("Send a message to all players"));
	Cmd_AddCommand("messagesayteam", Con_MessageModeSayTeam_f, _("Send a message to allied team members"));
	Cmd_AddCommand("messagemenu", Con_MessageModeMenu_f, NULL);
	Cmd_AddCommand("clear", Con_Clear_f, "Clear console text");
	Cmd_AddCommand("condump", Con_Dump_f, "Dump console text to textfile");

	/* load console history if con_history is true */
	Con_LoadConsoleHistory(FS_Gamedir());

	Con_CheckResize();
	con.initialized = qtrue;

	Com_Printf("Console initialized.\n");
}


static void Con_Linefeed (void)
{
	con.x = 0;
	if (con.display == con.current)
		con.display++;
	con.current++;
	memset(&con.text[(con.current % con.totallines) * con.linewidth],' ', con.linewidth);
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
	static int cr;
	int mask;

	if (!con.initialized)
		return;

	if (txt[0] == 1 || txt[0] == 2) {
		mask = COLORED_TEXT_MASK; /* go to colored text */
		txt++;
	} else
		mask = 0;

	while ((c = *txt) != 0) {
		/* count word length */
		for (l = 0; l < con.linewidth; l++)
			if (txt[l] <= ' ')
				break;

		/* word wrap */
		if (l != con.linewidth && (con.x + l > con.linewidth))
			con.x = 0;

		txt++;

		if (cr) {
			con.current--;
			cr = qfalse;
		}

		if (!con.x) {
			Con_Linefeed();
			/* mark time for transparent overlay */
			if (con.current >= 0)
				con.times[con.current % NUM_CON_TIMES] = cls.realtime;
		}

		switch (c) {
		case '\n':
			con.x = 0;
			break;

		case '\r':
			con.x = 0;
			cr = 1;
			break;

		default:	/* display character and advance */
#if 0
			if (!isprint(c))
				continue;
#endif
			y = con.current % con.totallines;
			con.text[y * con.linewidth + con.x] = c | mask;
			con.x++;
			if (con.x >= con.linewidth)
				con.x = 0;
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
	if (cls.key_dest == key_console)
		Key_SetDest(key_game);
}

/**
 * @brief The input line scrolls horizontally if typing goes beyond the right edge
 */
static void Con_DrawInput (void)
{
	int y;
	int i;
	char editlinecopy[MAXCMDLINE], *text;

	if (cls.key_dest != key_console && cls.state == ca_active)
		return;					/* don't draw anything (always draw if not active) */

	Q_strncpyz(editlinecopy, key_lines[edit_line], sizeof(editlinecopy));
	text = editlinecopy;
	y = strlen(text);

	/* add the cursor frame */
	if ((int)(cls.realtime >> 8) & 1) {
		text[key_linepos] = 11;
		if (key_linepos == y)
			y++;
	}

	/* fill out remainder with spaces */
	for (i = y; i < MAXCMDLINE; i++)
		text[i] = ' ';

	/* prestep if horizontally scrolling */
	if (key_linepos >= con.linewidth)
		text += 1 + key_linepos - con.linewidth;

	/* draw it */
	y = con.vislines - con_fontHeight;

	for (i = 0; i < con.linewidth; i++)
		R_DrawChar((i + 1) << con_fontShift, con.vislines - con_fontHeight - CONSOLE_CHAR_ALIGN, text[i]);
}


/**
 * @brief Draws the last few lines of output transparently over the game top
 * @sa SCR_DrawConsole
 */
void Con_DrawNotify (void)
{
	int x, l, v;
	char *text, *s;
	int i, time, skip;
	qboolean draw;

	v = 60 * viddef.rx;
	l = 120 * viddef.ry;
	for (i = con.current - NUM_CON_TIMES + 1; i <= con.current; i++) {
		draw = qfalse;
		if (i < 0)
			continue;
		time = con.times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = cls.realtime - time;
		if (time > con_notifytime->integer * 1000)
			continue;
		text = con.text + (i % con.totallines) * con.linewidth;

		for (x = 0; x < con.linewidth; x++) {
			/* only draw chat or check for developer mode */
			if (developer->integer || text[x] & COLORED_TEXT_MASK) {
				R_DrawChar(l + (x << con_fontShift), v, text[x]);
				draw = qtrue;
			}
		}
		if (draw)
			v += con_fontHeight;
	}

	if (cls.key_dest == key_message && (msg_mode == MSG_SAY_TEAM || msg_mode == MSG_SAY)) {
		if (msg_mode == MSG_SAY) {
			Con_DisplayString(l, v, "say:");
			skip = 4;
		} else {
			Con_DisplayString(l, v, "say_team:");
			skip = 10;
		}

		s = msg_buffer;
		if (msg_bufferlen > (viddef.width >> con_fontShift) - (skip + 1))
			s += msg_bufferlen - ((viddef.width >> con_fontShift) - (skip + 1));
		x = 0;
		while (s[x]) {
			R_DrawChar(l + ((x + skip) << con_fontShift), v, s[x]);
			x++;
		}
		R_DrawChar(l + ((x + skip) << con_fontShift), v, 10 + ((cls.realtime >> 8) & 1));
		v += con_fontHeight;
	}
}

/**
 * @brief Draws the console with the solid background
 * @param[in] frac
 */
void Con_DrawConsole (float frac)
{
	int i, x, y, len;
	int rows, row, lines;
	char *text;
	char version[MAX_VAR];
	/*const vec4_t consoleBG = {0.1, 0.1, 0.1, 0.9};*/

	lines = viddef.height * frac;
	if (lines <= 0)
		return;

	if (lines > viddef.height)
		lines = viddef.height;

	/* draw the background */
	/*R_DrawFill(0, 0, viddef.width, viddef.height, ALIGN_UL, consoleBG);*/
	R_DrawNormPic(0, lines - (int) viddef.height, VID_NORM_WIDTH, VID_NORM_HEIGHT, 0, 0, 0, 0, ALIGN_UL, qfalse, "conback");

	Com_sprintf(version, sizeof(version), "v%s", UFO_VERSION);
	len = strlen(version);
	for (x = 0; x < len; x++)
		R_DrawChar(viddef.width - (len * con_fontWidth) + x * con_fontWidth - CONSOLE_CHAR_ALIGN, lines - (con_fontHeight + CONSOLE_CHAR_ALIGN), version[x] | COLORED_TEXT_MASK);

	/* draw the text */
	con.vislines = lines;

	rows = (lines - con_fontHeight * 2) >> con_fontShift;	/* rows of text to draw */

	y = lines - con_fontHeight * 3;

	/* draw from the bottom up */
	if (con.display != con.current) {
		/* draw arrows to show the buffer is backscrolled */
		for (x = 0; x < con.linewidth; x += 4)
			R_DrawChar((x + 1) << con_fontShift, y, '^');

		y -= con_fontHeight;
		rows--;
	}

	row = con.display;
	for (i = 0; i < rows; i++, y -= con_fontHeight, row--) {
		if (row < 0)
			break;
		if (con.current - row >= con.totallines)
			break;				/* past scrollback wrap point */

		text = con.text + (row % con.totallines) * con.linewidth;

		for (x = 0; x < con.linewidth; x++)
			R_DrawChar((x + 1) << con_fontShift, y, text[x]);
	}

	/* draw the input prompt, user text, and cursor if desired */
	Con_DrawInput();
}
