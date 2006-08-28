/**
 * @file keys.c
 * @brief Keyboard handling routines.
 *
 * Note: Key up events are sent even if in console mode
 */

/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

15/06/06, Eddy Cullen (ScreamingWithNoSound):
	Reformatted to agreed style.
	Added doxygen file comment.
	Updated copyright notice.

Original file from Quake 2 v3.21: quake2-2.31/client/keys.c
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

#define		MAXCMDLINE	256
char key_lines[32][MAXCMDLINE];
int key_linepos;
static qboolean shift_down = qfalse;
static int anykeydown;

int edit_line = 0;
static int history_line = 0;

int msg_mode;
char msg_buffer[MAXCMDLINE];
int msg_bufferlen = 0;

static int key_waiting;
static char *keybindings[256];
static qboolean consolekeys[256];		/* if true, can't be rebound while in console */
static qboolean menubound[256];		/* if true, can't be rebound while in menu */
static int keyshift[256];				/* key to map to if shift held down in console */
static int key_repeats[256];			/* if > 1, it is autorepeating */
static qboolean keydown[256];

typedef struct {
	char *name;
	int keynum;
} keyname_t;

static keyname_t keynames[] = {
	{"TAB", K_TAB},
	{"ENTER", K_ENTER},
	{"ESCAPE", K_ESCAPE},
	{"SPACE", K_SPACE},
	{"BACKSPACE", K_BACKSPACE},
	{"UPARROW", K_UPARROW},
	{"DOWNARROW", K_DOWNARROW},
	{"LEFTARROW", K_LEFTARROW},
	{"RIGHTARROW", K_RIGHTARROW},

	{"ALT", K_ALT},
	{"CTRL", K_CTRL},
	{"SHIFT", K_SHIFT},

	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},

	{"INS", K_INS},
	{"DEL", K_DEL},
	{"PGDN", K_PGDN},
	{"PGUP", K_PGUP},
	{"HOME", K_HOME},
	{"END", K_END},

	{"MOUSE1", K_MOUSE1},
	{"MOUSE2", K_MOUSE2},
	{"MOUSE3", K_MOUSE3},

	{"AUX1", K_AUX1},
	{"AUX2", K_AUX2},
	{"AUX3", K_AUX3},
	{"AUX4", K_AUX4},
	{"AUX5", K_AUX5},
	{"AUX6", K_AUX6},
	{"AUX7", K_AUX7},
	{"AUX8", K_AUX8},
	{"AUX9", K_AUX9},
	{"AUX10", K_AUX10},
	{"AUX11", K_AUX11},
	{"AUX12", K_AUX12},
	{"AUX13", K_AUX13},
	{"AUX14", K_AUX14},
	{"AUX15", K_AUX15},
	{"AUX16", K_AUX16},
	{"AUX17", K_AUX17},
	{"AUX18", K_AUX18},
	{"AUX19", K_AUX19},
	{"AUX20", K_AUX20},
	{"AUX21", K_AUX21},
	{"AUX22", K_AUX22},
	{"AUX23", K_AUX23},
	{"AUX24", K_AUX24},
	{"AUX25", K_AUX25},
	{"AUX26", K_AUX26},
	{"AUX27", K_AUX27},
	{"AUX28", K_AUX28},
	{"AUX29", K_AUX29},
	{"AUX30", K_AUX30},
	{"AUX31", K_AUX31},
	{"AUX32", K_AUX32},

	{"KP_HOME", K_KP_HOME},
	{"KP_UPARROW", K_KP_UPARROW},
	{"KP_PGUP", K_KP_PGUP},
	{"KP_LEFTARROW", K_KP_LEFTARROW},
	{"KP_5", K_KP_5},
	{"KP_RIGHTARROW", K_KP_RIGHTARROW},
	{"KP_END", K_KP_END},
	{"KP_DOWNARROW", K_KP_DOWNARROW},
	{"KP_PGDN", K_KP_PGDN},
	{"KP_ENTER", K_KP_ENTER},
	{"KP_INS", K_KP_INS},
	{"KP_DEL", K_KP_DEL},
	{"KP_SLASH", K_KP_SLASH},
	{"KP_MINUS", K_KP_MINUS},
	{"KP_PLUS", K_KP_PLUS},

	{"MWHEELUP", K_MWHEELUP},
	{"MWHEELDOWN", K_MWHEELDOWN},

	{"PAUSE", K_PAUSE},

	{"SEMICOLON", ';'},			/* because a raw semicolon seperates commands */

	{NULL, 0}
};

/*
==============================================================================
LINE TYPING INTO THE CONSOLE
==============================================================================
*/

/**
 * @brief Console completion for command and variables
 * @sa Key_Console
 * @sa Cmd_CompleteCommand
 * @sa Cvar_CompleteVariable
 */
static void Key_CompleteCommand(void)
{
	char *cmd = NULL, *cvar = NULL, *use = NULL, *s;
	int cntCmd = 0, cntCvar = 0;

	s = key_lines[edit_line] + 1;
	if (*s == '\\' || *s == '/')
		s++;

	cntCmd = Cmd_CompleteCommand(s, &cmd);
	cntCvar = Cvar_CompleteVariable(s, &cvar);

	if (cntCmd == 1 && !cntCvar)
		use = cmd;
	else if (!cntCmd && cntCvar == 1)
		use = cvar;
	else
		Com_Printf("\n");

	if (use) {
		key_lines[edit_line][1] = '/';
		Q_strncpyz(key_lines[edit_line] + 2, use, MAXCMDLINE - 2);
		key_linepos = strlen(use) + 2;
		key_lines[edit_line][key_linepos] = ' ';
		key_linepos++;
		key_lines[edit_line][key_linepos] = 0;
		return;
	}
}

/**
 * @brief Interactive line editing and console scrollback
 * @param[in] key
 */
static void Key_Console(int key)
{
	switch (key) {
	case K_KP_SLASH:
		key = '/';
		break;
	case K_KP_MINUS:
		key = '-';
		break;
	case K_KP_PLUS:
		key = '+';
		break;
	case K_KP_HOME:
		key = '7';
		break;
	case K_KP_UPARROW:
		key = '8';
		break;
	case K_KP_PGUP:
		key = '9';
		break;
	case K_KP_LEFTARROW:
		key = '4';
		break;
	case K_KP_5:
		key = '5';
		break;
	case K_KP_RIGHTARROW:
		key = '6';
		break;
	case K_KP_END:
		key = '1';
		break;
	case K_KP_DOWNARROW:
		key = '2';
		break;
	case K_KP_PGDN:
		key = '3';
		break;
	case K_KP_INS:
		key = '0';
		break;
	case K_KP_DEL:
		key = '.';
		break;
	}

	/* ctrl-V gets clipboard data */
	if ((toupper(key) == 'V' && keydown[K_CTRL]) || (((key == K_INS) || (key == K_KP_INS)) && keydown[K_SHIFT])) {
		char *cbd;

		if ((cbd = Sys_GetClipboardData()) != 0) {
			int i;

			strtok(cbd, "\n\r\b");

			i = strlen(cbd);
			if (i + key_linepos >= MAXCMDLINE)
				i = MAXCMDLINE - key_linepos;

			if (i > 0) {
				cbd[i] = 0;
				Q_strcat(key_lines[edit_line], cbd, MAXCMDLINE);
				key_linepos += i;
			}
			free(cbd);
		}

		return;
	}

	/* ctrl-L clears screen */
	if (key == 'l' && keydown[K_CTRL]) {
		Cbuf_AddText("clear\n");
		return;
	}

	if (key == K_ENTER || key == K_KP_ENTER) {	/* backslash text are commands, else chat */
		if (key_lines[edit_line][1] == '\\' || key_lines[edit_line][1] == '/')
			Cbuf_AddText(key_lines[edit_line] + 2);	/* skip the > */
		else
			Cbuf_AddText(key_lines[edit_line] + 1);	/* valid command */

		Cbuf_AddText("\n");
		Com_Printf("%s\n", key_lines[edit_line]);
		edit_line = (edit_line + 1) & 31;
		history_line = edit_line;
		key_lines[edit_line][0] = ']';
		key_linepos = 1;
		if (cls.state == ca_disconnected)
			SCR_UpdateScreen();	/* force an update, because the command */
		/* may take some time */
		return;
	}

	/* command completion */
	if (key == K_TAB) {
		Key_CompleteCommand();
		return;
	}

	if (key == K_BACKSPACE || key == K_LEFTARROW || key == K_KP_LEFTARROW || ((key == 'h') && (keydown[K_CTRL]))) {
		if (key_linepos > 1)
			key_linepos--;
		return;
	}

	if (key == K_UPARROW || key == K_KP_UPARROW || ((tolower(key) == 'p') && keydown[K_CTRL])) {
		do {
			history_line = (history_line - 1) & 31;
		} while (history_line != edit_line && !key_lines[history_line][1]);
		if (history_line == edit_line)
			history_line = (edit_line + 1) & 31;
		Q_strncpyz(key_lines[edit_line], key_lines[history_line], MAXCMDLINE);
		key_linepos = strlen(key_lines[edit_line]);
		return;
	}

	if (key == K_DOWNARROW || key == K_KP_DOWNARROW || ((tolower(key) == 'n') && keydown[K_CTRL])) {
		if (history_line == edit_line)
			return;
		do {
			history_line = (history_line + 1) & 31;
		}
		while (history_line != edit_line && !key_lines[history_line][1]);
		if (history_line == edit_line) {
			key_lines[edit_line][0] = ']';
			key_linepos = 1;
		} else {
			Q_strncpyz(key_lines[edit_line], key_lines[history_line], MAXCMDLINE);
			key_linepos = strlen(key_lines[edit_line]);
		}
		return;
	}

	if (key == K_PGUP || key == K_KP_PGUP || key == K_MWHEELUP) {
		con.display -= 2;
		return;
	}

	if (key == K_PGDN || key == K_KP_PGDN || key == K_MWHEELDOWN) {
		con.display += 2;
		if (con.display > con.current)
			con.display = con.current;
		return;
	}

	if (key == K_HOME || key == K_KP_HOME) {
		con.display = con.current - con.totallines + 10;
		return;
	}

	if (key == K_END || key == K_KP_END) {
		con.display = con.current;
		return;
	}

	if (key < 32 || key > 127)
		return;					/* non printable */

	if (key_linepos < MAXCMDLINE - 1) {
		key_lines[edit_line][key_linepos] = key;
		key_linepos++;
		key_lines[edit_line][key_linepos] = 0;
	}
}

/**
 * @brief Handles input when cls.key_dest == key_message
 * @note Used for chatting and cvar editing via menu
 * @sa Key_Event
 */
void Key_Message(int key)
{
	if (key == K_ENTER || key == K_KP_ENTER) {
		qboolean send = qtrue;

		switch (msg_mode) {
		case MSG_SAY:
			if (msg_buffer[0])
				Cbuf_AddText("say \"");
			else
				send = qfalse;
			break;
		case MSG_SAY_TEAM:
			if (msg_buffer[0])
				Cbuf_AddText("say_team \"");
			else
				send = qfalse;
			break;
		case MSG_MENU:
			Cbuf_AddText("msgmenu \":");
			break;
		}
		if (send) {
			Cbuf_AddText(msg_buffer);
			Cbuf_AddText("\"\n");
		}

		cls.key_dest = key_game;
		msg_bufferlen = 0;
		msg_buffer[0] = 0;
		return;
	}

	if (key == K_ESCAPE) {
		cls.key_dest = key_game;
		msg_bufferlen = 0;
		msg_buffer[0] = 0;
		if (msg_mode == MSG_MENU)
			Cbuf_AddText("msgmenu !");
		return;
	}

	if (key < 32 || key > 127)
		return;					/* non printable */

	if (key == K_BACKSPACE) {
		if (msg_bufferlen) {
			msg_bufferlen--;
			msg_buffer[msg_bufferlen] = 0;
			if (msg_mode == MSG_MENU)
				Cbuf_AddText(va("msgmenu \"%s\"\n", msg_buffer));
		}
		return;
	}

	if (msg_bufferlen == sizeof(msg_buffer) - 1)
		return;					/* all full */

	msg_buffer[msg_bufferlen++] = key;
	msg_buffer[msg_bufferlen] = 0;

	if (msg_mode == MSG_MENU)
		Cbuf_AddText(va("msgmenu \"%s\"\n", msg_buffer));
}


/**
 * @brief Convert to given string to keynum
 * @param[in] str The keystring to convert to keynum
 * @return a key number to be used to index keybindings[] by looking at
 * the given string.  Single ascii characters return themselves, while
 * the K_* names are matched up.
 */
int Key_StringToKeynum(char *str)
{
	keyname_t *kn;

	if (!str || !str[0])
		return -1;
	if (!str[1])
		return str[0];

	for (kn = keynames; kn->name; kn++) {
		if (!Q_strcasecmp(str, kn->name))
			return kn->keynum;
	}
	return -1;
}

/**
 * @brief Convert a given keynum to string
 * @param[in] keynum The keynum to convert to string
 * @return a string (either a single ascii char, or a K_* name) for the given keynum.
 * FIXME: handle quote special (general escape sequence?)
 */
char *Key_KeynumToString(int keynum)
{
	keyname_t *kn;
	static char tinystr[2];

	if (keynum == -1)
		return "<KEY NOT FOUND>";
	if (keynum > 32 && keynum < 127) {	/* printable ascii */
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}

	for (kn = keynames; kn->name; kn++)
		if (keynum == kn->keynum)
			return kn->name;

	return "<UNKNOWN KEYNUM>";
}


/**
 * @brief Bind a keynum to script command
 * @param[in] keynum Converted from string to keynum
 * @param[in] binding The script command to bind keynum to
 * @sa Key_Bind_f
 * @sa Key_StringToKeynum
 */
void Key_SetBinding(int keynum, char *binding)
{
	char *new;
	int l;

	if (keynum == -1)
		return;

	/* free old bindings */
	if (keybindings[keynum]) {
		Z_Free(keybindings[keynum]);
		keybindings[keynum] = NULL;
	}

	/* allocate memory for new binding */
	l = strlen(binding);
	new = Z_Malloc(l + 1);
	Q_strncpyz(new, binding, l + 1);
	keybindings[keynum] = new;
}

/**
 * @brief Unbind a given key binding
 * @sa Key_SetBinding
 */
void Key_Unbind_f(void)
{
	int b;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: unbind <key> : remove commands from a key\n");
		return;
	}

	b = Key_StringToKeynum(Cmd_Argv(1));
	if (b == -1) {
		Com_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	Key_SetBinding(b, "");
}

/**
 * @brief Unbind all key bindings
 * @sa Key_SetBinding
 */
void Key_Unbindall_f(void)
{
	int i;

	for (i = 0; i < 256; i++)
		if (keybindings[i])
			Key_SetBinding(i, "");
}


/**
 * @brief Binds a key to a given script command
 * @sa Key_SetBinding
 */
void Key_Bind_f(void)
{
	int i, c, b;
	char cmd[1024];

	c = Cmd_Argc();

	if (c < 2) {
		Com_Printf("Usage: bind <key> [command] : attach a command to a key\n");
		return;
	}
	b = Key_StringToKeynum(Cmd_Argv(1));
	if (b == -1) {
		Com_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	if (c == 2) {
		if (keybindings[b])
			Com_Printf("\"%s\" = \"%s\"\n", Cmd_Argv(1), keybindings[b]);
		else
			Com_Printf("\"%s\" is not bound\n", Cmd_Argv(1));
		return;
	}

	/* copy the rest of the command line */
	cmd[0] = 0;					/* start out with a null string */
	for (i = 2; i < c; i++) {
		strcat(cmd, Cmd_Argv(i));
		if (i != (c - 1))
			strcat(cmd, " ");
	}

	Key_SetBinding(b, cmd);
}

/**
 * @brief Writes lines containing "bind key value"
 * @param[in] f Filehandle to print the keybinding too
 */
void Key_WriteBindings(FILE * f)
{
	int i;

	for (i = 0; i < 256; i++)
		if (keybindings[i] && keybindings[i][0])
			fprintf(f, "bind %s \"%s\"\n", Key_KeynumToString(i), keybindings[i]);
}


/**
 * @brief List all binded keys with its function
 */
void Key_Bindlist_f(void)
{
	int i;

	for (i = 0; i < 256; i++)
		if (keybindings[i] && keybindings[i][0])
			Com_Printf("%s \"%s\"\n", Key_KeynumToString(i), keybindings[i]);
}


/**
 * @brief
 * @todo Fix this crappy shift key assignment for win32 and sdl (no _)
 */
void Key_Init(void)
{
	int i;

	for (i = 0; i < 32; i++) {
		key_lines[i][0] = ']';
		key_lines[i][1] = 0;
	}
	key_linepos = 1;

	/* init ascii characters in console mode */
	for (i = 32; i < 128; i++)
		consolekeys[i] = qtrue;
	consolekeys[K_ENTER] = qtrue;
	consolekeys[K_KP_ENTER] = qtrue;
	consolekeys[K_TAB] = qtrue;
	consolekeys[K_LEFTARROW] = qtrue;
	consolekeys[K_KP_LEFTARROW] = qtrue;
	consolekeys[K_RIGHTARROW] = qtrue;
	consolekeys[K_KP_RIGHTARROW] = qtrue;
	consolekeys[K_UPARROW] = qtrue;
	consolekeys[K_KP_UPARROW] = qtrue;
	consolekeys[K_DOWNARROW] = qtrue;
	consolekeys[K_KP_DOWNARROW] = qtrue;
	consolekeys[K_BACKSPACE] = qtrue;
	consolekeys[K_HOME] = qtrue;
	consolekeys[K_KP_HOME] = qtrue;
	consolekeys[K_END] = qtrue;
	consolekeys[K_KP_END] = qtrue;
	consolekeys[K_PGUP] = qtrue;
	consolekeys[K_KP_PGUP] = qtrue;
	consolekeys[K_PGDN] = qtrue;
	consolekeys[K_KP_PGDN] = qtrue;
	consolekeys[K_SHIFT] = qtrue;
	consolekeys[K_INS] = qtrue;
	consolekeys[K_KP_INS] = qtrue;
	consolekeys[K_KP_DEL] = qtrue;
	consolekeys[K_KP_SLASH] = qtrue;
	consolekeys[K_KP_PLUS] = qtrue;
	consolekeys[K_KP_MINUS] = qtrue;
	consolekeys[K_KP_5] = qtrue;
	consolekeys[K_MWHEELUP] = qtrue;
	consolekeys[K_MWHEELDOWN] = qtrue;

	consolekeys['`'] = qfalse;
	consolekeys['~'] = qfalse;

	for (i = 0; i < 256; i++)
		keyshift[i] = i;
	for (i = 'a'; i <= 'z'; i++)
		keyshift[i] = i - 'a' + 'A';

#if _WIN32
	keyshift['1'] = '!';
	keyshift['2'] = '@';
	keyshift['3'] = '#';
	keyshift['4'] = '$';
	keyshift['5'] = '%';
	keyshift['6'] = '^';
	keyshift['7'] = '&';
	keyshift['8'] = '*';
	keyshift['9'] = '(';
	keyshift['0'] = ')';
	keyshift['-'] = '_';
	keyshift['='] = '+';
	keyshift[','] = '<';
	keyshift['.'] = '>';
	keyshift['/'] = '?';
	keyshift[';'] = ':';
	keyshift['\''] = '"';
	keyshift['['] = '{';
	keyshift[']'] = '}';
	keyshift['`'] = '~';
	keyshift['\\'] = '|';
#endif

	menubound[K_ESCAPE] = qtrue;
	for (i = 0; i < 12; i++)
		menubound[K_F1 + i] = qtrue;

	/* register our functions */
	Cmd_AddCommand("bind", Key_Bind_f);
	Cmd_AddCommand("unbind", Key_Unbind_f);
	Cmd_AddCommand("unbindall", Key_Unbindall_f);
	Cmd_AddCommand("bindlist", Key_Bindlist_f);
}

/**
 * @brief Called by the system between frames for both key up and key down events
 * @note Should NOT be called during an interrupt!
 * @sa Key_Message
 */
void Key_Event(int key, qboolean down, unsigned time)
{
	char *kb;
	char cmd[1024];

	/* hack for modal presses */
	if (key_waiting == -1) {
		if (down)
			key_waiting = key;
		return;
	}

	/* update auto-repeat status */
	if (down) {
		key_repeats[key]++;
		if (key != K_BACKSPACE && key != K_PAUSE && key != K_PGUP && key != K_KP_PGUP && key != K_PGDN && key != K_KP_PGDN && key_repeats[key] > 1)
			return;				/* ignore most autorepeats */

		if (key > 255 && !keybindings[key])
			Com_Printf("%s is unbound, hit F4 to set.\n", Key_KeynumToString(key));
	} else {
		key_repeats[key] = 0;
	}

	if (key == K_SHIFT)
		shift_down = down;

	/* console key is hardcoded, so the user can never unbind it */
	if (key == '`' || key == '~') {
		if (!down)
			return;
		Con_ToggleConsole_f();
		return;
	}

	/* any key during the attract mode will bring up the menu */
	if (cl.attractloop && !(key >= K_F1 && key <= K_F12))
		key = K_ESCAPE;

	/* menu key is hardcoded, so the user can never unbind it */
	if (key == K_ESCAPE) {
		if (!down)
			return;

		switch (cls.key_dest) {
		case key_message:
			Key_Message(key);
			break;
		case key_game:
			Cbuf_AddText("mn_pop esc;");
			break;
		case key_console:
			Con_ToggleConsole_f();
			break;
		default:
			Com_Error(ERR_FATAL, "Bad cls.key_dest");
		}
		return;
	}

	/* track if any key is down for BUTTON_ANY */
	keydown[key] = down;
	if (down) {
		if (key_repeats[key] == 1)
			anykeydown++;
	} else {
		anykeydown--;
		if (anykeydown < 0)
			anykeydown = 0;
	}

	/* key up events only generate commands if the game key binding is */
	/* a button command (leading + sign).  These will occur even in console mode, */
	/* to keep the character from continuing an action started before a console */
	/* switch.  Button commands include the kenum as a parameter, so multiple */
	/* downs can be matched with ups */
	if (!down) {
		kb = keybindings[key];
		if (kb && kb[0] == '+') {
			Com_sprintf(cmd, sizeof(cmd), "-%s %i %i\n", kb + 1, key, time);
			Cbuf_AddText(cmd);
		}
		if (keyshift[key] != key) {
			kb = keybindings[keyshift[key]];
			if (kb && kb[0] == '+') {
				Com_sprintf(cmd, sizeof(cmd), "-%s %i %i\n", kb + 1, key, time);
				Cbuf_AddText(cmd);
			}
		}
		return;
	}

	/* if not a consolekey, send to the interpreter no matter what mode is */
	if (cls.key_dest == key_game) {	/*&& !consolekeys[key] ) */
		kb = keybindings[key];
		if (kb) {
			if (kb[0] == '+') {	/* button commands add keynum and time as a parm */
				Com_sprintf(cmd, sizeof(cmd), "%s %i %i\n", kb, key, time);
				Cbuf_AddText(cmd);
			} else {
				Cbuf_AddText(kb);
				Cbuf_AddText("\n");
			}
		}
		return;
	}

	if (!down)
		return;					/* other systems only care about key down events */

	if (shift_down)
		key = keyshift[key];

	switch (cls.key_dest) {
	case key_message:
		Key_Message(key);
		break;
	case key_game:
	case key_console:
		Key_Console(key);
		break;
	default:
		Com_Error(ERR_FATAL, "Bad cls.key_dest");
	}
}

/**
 * @brief
 */
void Key_ClearStates(void)
{
	int i;

	anykeydown = qfalse;

	for (i = 0; i < 256; i++) {
		if (keydown[i] || key_repeats[i])
			Key_Event(i, qfalse, 0);
		keydown[i] = 0;
		key_repeats[i] = 0;
	}
}


/**
 * @brief
 */
int Key_GetKey(void)
{
	key_waiting = -1;

	while (key_waiting == -1)
		Sys_SendKeyEvents();

	return key_waiting;
}
