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

static cvar_t *con_notifytime;
static cvar_t *con_history;
cvar_t *con_fontHeight;
cvar_t *con_fontWidth;
cvar_t *con_fontShift;

extern char key_lines[MAXKEYLINES][MAXCMDLINE];
extern int edit_line;
extern int key_linepos;

/**
 * @brief
 */
static void DisplayString (int x, int y, char *s)
{
	while (*s) {
		re.DrawChar(x, y, *s);
		x += con_fontWidth->integer;
		s++;
	}
}

/**
 * @brief
 */
static void Key_ClearTyping (void)
{
	key_lines[edit_line][1] = 0;	/* clear any typing */
	key_linepos = 1;
}

/**
 * @brief
 */
void Con_ToggleConsole_f (void)
{
	int maxclients;

	if (cl.attractloop) {
		Cbuf_AddText("killserver\n");
		return;
	}
	maxclients = Cvar_VariableInteger("maxclients");

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

/**
 * @brief
 */
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
		Com_Printf("usage: condump <filename>\n");
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


/**
 * @brief
 */
void Con_ClearNotify (void)
{
	int i;

	for (i = 0; i < NUM_CON_TIMES; i++)
		con.times[i] = 0;
}


/**
 * @brief
 */
void Con_MessageModeSay_f (void)
{
	msg_mode = MSG_SAY;
	Key_SetDest(key_message);
}

/**
 * @brief
 */
static void Con_MessageModeSayTeam_f (void)
{
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
extern void Con_CheckResize (void)
{
	int i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char tbuf[CON_TEXTSIZE];

	width = (viddef.width >> 3) - 2;

	if (width == con.linewidth)
		return;

	if (width < 1) {	/* video hasn't been initialized yet */
		width = 80;
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
	char *f, *f2, *cmdStart, *cmdEnd;
	int len, i = 0;
	char filename[MAX_OSPATH];

	assert(path);

	if (!con_history->integer)
		return;

	Com_sprintf(filename, sizeof(filename), "%s", CONSOLE_HISTORY_FILENAME);

	len = FS_LoadFile(filename, (void **) &f);
	if (!f) {
		Com_Printf("couldn't load %s\n", filename);
		return;
	}

	/* the file doesn't have a trailing 0, so we need to copy it off */
	f2 = Mem_Alloc(len + 1);
	memcpy(f2, f, len);
	f2[len] = 0;

	Com_DPrintf("...load console history\n");

	cmdStart = strstr(f2, "\n");
	if (cmdStart != NULL) {
		*cmdStart++ = '\0';

		do {
			cmdEnd = strstr(cmdStart, "\n");
			if (cmdEnd)
				*cmdEnd++ = '\0';
			if (*cmdStart) {
				Q_strncpyz(&key_lines[i][1], cmdStart, MAXCMDLINE-1);
				Com_DPrintf("....command: '%s'\n", cmdStart);
				i++;
			}
			cmdStart = cmdEnd;
		} while (cmdEnd && strstr(cmdEnd, "\n"));
	}
	/* only the do not modify comment */
	Mem_Free(f2);
	FS_FreeFile(f);
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

	assert(path);

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
			Com_DPrintf("Don't save '%s' again\n", lastLine);
#endif
		} else {
			lastLine = &(key_lines[i][1]);
			fprintf(f, "%s\n", lastLine);
		}
	}
	fclose(f);
}

/**
 * @brief
 */
extern void Con_Init (void)
{
	con.linewidth = -1;

	Con_CheckResize();

	Com_Printf("Console initialized.\n");

	/* register our commands and cvars */
	con_notifytime = Cvar_Get("con_notifytime", "10", CVAR_ARCHIVE, "How many seconds console messages should be shown before they fade away");
	con_history = Cvar_Get("con_history", "1", CVAR_ARCHIVE, "Permanent console history");

	Cmd_AddCommand("toggleconsole", Con_ToggleConsole_f, _("Bring up the in-game console"));
	Cmd_AddCommand("togglechat", Con_ToggleChat_f, NULL);
	Cmd_AddCommand("messagesay", Con_MessageModeSay_f, _("Send a message to all players"));
	Cmd_AddCommand("messagesayteam", Con_MessageModeSayTeam_f, _("Send a message to allied team members"));
	Cmd_AddCommand("messagemenu", Con_MessageModeMenu_f, NULL);
	Cmd_AddCommand("clear", Con_Clear_f, NULL);
	Cmd_AddCommand("condump", Con_Dump_f, NULL);

	/* load console history if con_history is true */
	Con_LoadConsoleHistory(FS_Gamedir());

	con.initialized = qtrue;
}


/**
 * @brief
 */
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


/**
 * @brief Centers the text to print on console
 * @param[in] text
 * @sa Con_Print
 * @note not used atm
 */
void Con_CenteredPrint (const char *text)
{
	int l;
	char buffer[MAX_STRING_CHARS];

	l = strlen(text);
	l = (con.linewidth - l) / 2;
	if (l < 0)
		l = 0;
	memset(buffer, ' ', l);
	Q_strcat(buffer, text, sizeof(buffer));
	Q_strcat(buffer, "\n", sizeof(buffer));
	Con_Print(buffer);
}

/*
==============================================================================
DRAWING
==============================================================================
*/


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
	y = con.vislines - con_fontHeight->integer;

	for (i = 0; i < con.linewidth; i++)
		re.DrawChar((i + 1) << con_fontShift->integer, con.vislines - con_fontHeight->integer - CONSOLE_CHAR_ALIGN, text[i]);
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
				re.DrawChar(l + (x << con_fontShift->integer), v, text[x]);
				draw = qtrue;
			}
		}
		if (draw)
			v += con_fontHeight->integer;
	}

	if (cls.key_dest == key_message && (msg_mode == MSG_SAY_TEAM || msg_mode == MSG_SAY)) {
		if (msg_mode == MSG_SAY) {
			DisplayString(l, v, "say:");
			skip = 4;
		} else {
			DisplayString(l, v, "say_team:");
			skip = 10;
		}

		s = msg_buffer;
		if (msg_bufferlen > (viddef.width >> con_fontShift->integer) - (skip + 1))
			s += msg_bufferlen - ((viddef.width >> con_fontShift->integer) - (skip + 1));
		x = 0;
		while (s[x]) {
			re.DrawChar(l + ((x + skip) << con_fontShift->integer), v, s[x]);
			x++;
		}
		re.DrawChar(l + ((x + skip) << con_fontShift->integer), v, 10 + ((cls.realtime >> 8) & 1));
		v += con_fontHeight->integer;
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
	/*re.DrawFill(0, 0, viddef.width, viddef.height, ALIGN_UL, consoleBG);*/
	re.DrawNormPic(0, lines - (int) viddef.height, VID_NORM_WIDTH, VID_NORM_HEIGHT, 0, 0, 0, 0, ALIGN_UL, qfalse, "conback");

	Com_sprintf(version, sizeof(version), "v%s", UFO_VERSION);
	len = strlen(version);
	for (x = 0; x < len; x++)
		re.DrawChar(viddef.width - (len * con_fontWidth->integer) + x * con_fontWidth->integer - CONSOLE_CHAR_ALIGN, lines - (con_fontHeight->integer + CONSOLE_CHAR_ALIGN), version[x] | COLORED_TEXT_MASK);

	/* draw the text */
	con.vislines = lines;

	rows = (lines - con_fontHeight->integer * 2) >> con_fontShift->integer;	/* rows of text to draw */

	y = lines - con_fontHeight->integer * 3;

	/* draw from the bottom up */
	if (con.display != con.current) {
		/* draw arrows to show the buffer is backscrolled */
		for (x = 0; x < con.linewidth; x += 4)
			re.DrawChar((x + 1) << con_fontShift->integer, y, '^');

		y -= con_fontHeight->integer;
		rows--;
	}

	row = con.display;
	for (i = 0; i < rows; i++, y -= con_fontHeight->integer, row--) {
		if (row < 0)
			break;
		if (con.current - row >= con.totallines)
			break;				/* past scrollback wrap point */

		text = con.text + (row % con.totallines) * con.linewidth;

		for (x = 0; x < con.linewidth; x++)
			re.DrawChar((x + 1) << con_fontShift->integer, y, text[x]);
	}

	/* draw the input prompt, user text, and cursor if desired */
	Con_DrawInput();
}
