/**
 * @file cl_keys.c
 * @brief Keyboard handling routines.
 *
 * Note: Key up events are sent even if in console mode
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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
#include "cl_console.h"

char key_lines[MAXKEYLINES][MAXCMDLINE];
int key_linepos;

static int key_insert = 1;

int edit_line = 0;
int history_line = 0;

int msg_mode;
char msg_buffer[MAXCMDLINE];
size_t msg_bufferlen = 0;

/** @todo To support international keyboards nicely, we will need full
 * support for binding to either
 * - a unicode value, however achieved
 * - a key
 * - modifier + key
 * with a priority system to decide which to try first.
 * This will mean that cleverly-hidden punctuation keys will still have
 * their expected effect, even if they have to be pressed as Shift-AltGr-7
 * or something. At the same time, it allows key combinations to be bound
 * regardless of what their translated meaning is, so that for example
 * Shift-4 can do something with the 4th agent regardless of which
 * punctuation symbol is above the 4.
 */

char *keybindings[K_KEY_SIZE];
char *menukeybindings[K_KEY_SIZE];
char *battlekeybindings[K_KEY_SIZE];

static qboolean keydown[K_KEY_SIZE];

typedef struct {
	const char *name;
	int keynum;
} keyname_t;

static const keyname_t keynames[] = {
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
	{"MOUSE4", K_MOUSE4},
	{"MOUSE5", K_MOUSE5},

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

	{"APPS", K_APPS},

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

	{"JOY1", K_JOY1},
	{"JOY2", K_JOY2},
	{"JOY3", K_JOY3},
	{"JOY4", K_JOY4},
	{"JOY5", K_JOY5},
	{"JOY6", K_JOY6},
	{"JOY7", K_JOY7},
	{"JOY8", K_JOY8},
	{"JOY9", K_JOY9},
	{"JOY10", K_JOY10},
	{"JOY11", K_JOY11},
	{"JOY12", K_JOY12},
	{"JOY13", K_JOY13},
	{"JOY14", K_JOY14},
	{"JOY15", K_JOY15},
	{"JOY16", K_JOY16},
	{"JOY17", K_JOY17},
	{"JOY18", K_JOY18},
	{"JOY19", K_JOY19},
	{"JOY20", K_JOY20},
	{"JOY21", K_JOY21},
	{"JOY22", K_JOY22},
	{"JOY23", K_JOY23},
	{"JOY24", K_JOY24},
	{"JOY25", K_JOY25},
	{"JOY26", K_JOY26},
	{"JOY27", K_JOY27},
	{"JOY28", K_JOY28},
	{"JOY29", K_JOY29},
	{"JOY30", K_JOY30},
	{"JOY31", K_JOY31},
	{"JOY32", K_JOY32},

	{"PAUSE", K_PAUSE},

	{"SEMICOLON", ';'},			/* because a raw semicolon seperates commands */

	{"WINDOWS", K_SUPER},
	{"COMPOSE", K_COMPOSE},
	{"MODE", K_MODE},
	{"HELP", K_HELP},
	{"PRINT", K_PRINT},
	{"SYSREQ", K_SYSREQ},
	{"SCROLLOCK", K_SCROLLOCK},
	{"BREAK", K_BREAK},
	{"MENU", K_MENU},
	{"POWER", K_POWER},
	{"EURO", K_EURO},
	{"UNDO", K_UNDO},

	{NULL, 0}
};

/*
==============================================================================
LINE TYPING INTO THE CONSOLE
==============================================================================
*/

/**
 * @brief Interactive line editing and console scrollback
 * @param[in] key key code, either K_ value or lowercase ascii
 * @param[in] unicode translated meaning of keypress in unicode
 */
static void Key_Console (int key, int unicode)
{
	int i;

	if (keydown[K_CTRL]) {
		switch (toupper(key)) {
		/* ctrl-L clears screen */
		case 'L':
			Cbuf_AddText("clear\n");
			return;
		/* jump to beginning of line */
		case 'A':
			key_linepos = 1;
			return;
		/* end of line */
		case 'E':
			key_linepos = strlen(key_lines[edit_line]);
			return;
		}
	}

	if (key == K_ENTER || key == K_KP_ENTER) {	/* backslash text are commands, else chat */
		if (key_lines[edit_line][1] == '\\' || key_lines[edit_line][1] == '/')
			Cbuf_AddText(key_lines[edit_line] + 2);	/* skip the > */
		/* no command - just enter */
		else if (!key_lines[edit_line][1])
			return;
		else
			Cbuf_AddText(key_lines[edit_line] + 1);	/* valid command */

		Cbuf_AddText("\n");
		Com_Printf("%s\n", key_lines[edit_line] + 1);
		edit_line = (edit_line + 1) & (MAXKEYLINES - 1);
		history_line = edit_line;
		key_lines[edit_line][0] = CONSOLE_PROMPT_CHAR | CONSOLE_COLORED_TEXT_MASK;
		/* maybe MAXKEYLINES was reached - we don't want to spawn 'random' strings
		 * from history buffer in our console */
		key_lines[edit_line][1] = '\0';
		key_linepos = 1;

		/* force an update, because the command may take some time */
		if (cls.state == ca_disconnected)
			SCR_UpdateScreen();

		return;
	}

	/* command completion */
	if (key == K_TAB) {
		Com_ConsoleCompleteCommand(&key_lines[edit_line][1], key_lines[edit_line], MAXCMDLINE, &key_linepos, 1);
		return;
	}

	if (key == K_BACKSPACE || (key == 'h' && keydown[K_CTRL])) {
		if (key_linepos > 1) {
			strcpy(key_lines[edit_line] + key_linepos - 1, key_lines[edit_line] + key_linepos);
			key_linepos--;
		}
		return;
	}
	/* delete char on cursor */
	if (key == K_DEL) {
		if (key_linepos < strlen(key_lines[edit_line]))
			strcpy(key_lines[edit_line] + key_linepos, key_lines[edit_line] + key_linepos + 1);
		return;
	}

	if (key == K_UPARROW || key == K_KP_UPARROW || (tolower(key) == 'p' && keydown[K_CTRL])) {
		do {
			history_line = (history_line - 1) & (MAXKEYLINES - 1);
		} while (history_line != edit_line && !key_lines[history_line][1]);

		if (history_line == edit_line)
			history_line = (edit_line + 1) & (MAXKEYLINES - 1);

		Q_strncpyz(key_lines[edit_line], key_lines[history_line], MAXCMDLINE);
		key_linepos = strlen(key_lines[edit_line]);
		return;
	} else if (key == K_DOWNARROW || key == K_KP_DOWNARROW || (tolower(key) == 'n' && keydown[K_CTRL])) {
		if (history_line == edit_line)
			return;
		do {
			history_line = (history_line + 1) & (MAXKEYLINES - 1);
		} while (history_line != edit_line && !key_lines[history_line][1]);

		if (history_line == edit_line) {
			key_lines[edit_line][0] = CONSOLE_PROMPT_CHAR | CONSOLE_COLORED_TEXT_MASK;
			/* fresh edit line */
			key_lines[edit_line][1] = '\0';
			key_linepos = 1;
		} else {
			Q_strncpyz(key_lines[edit_line], key_lines[history_line], MAXCMDLINE);
			key_linepos = strlen(key_lines[edit_line]);
		}
		return;
	}

	if (key == K_LEFTARROW) {  /* move cursor left */
		if (keydown[K_CTRL]) { /* by a whole word */
			while (key_linepos > 1 && key_lines[edit_line][key_linepos - 1] == ' ')
				key_linepos--;  /* get off current word */
			while (key_linepos > 1 && key_lines[edit_line][key_linepos - 1] != ' ')
				key_linepos--;  /* and behind previous word */
			return;
		}

		if (key_linepos > 1)  /* or just a char */
			key_linepos--;
		return;
	} else if (key == K_RIGHTARROW) {  /* move cursor right */
		if ((i = strlen(key_lines[edit_line])) == key_linepos)
			return; /* no character to get */
		if (keydown[K_CTRL]) {  /* by a whole word */
			while (key_linepos < i && key_lines[edit_line][key_linepos + 1] == ' ')
				key_linepos++;  /* get off current word */
			while (key_linepos < i && key_lines[edit_line][key_linepos + 1] != ' ')
				key_linepos++;  /* and in front of next word */
			if (key_linepos < i)  /* all the way in front */
				key_linepos++;
			return;
		}
		key_linepos++;  /* or just a char */
		return;
	}

	/* toggle insert mode */
	if (key == K_INS) {
		key_insert ^= 1;
		return;
	}

	if (key == K_PGUP || key == K_KP_PGUP || key == K_MWHEELUP) {
		Con_Scroll(-2);
		return;
	}

	if (key == K_PGDN || key == K_KP_PGDN || key == K_MWHEELDOWN) {
		Con_Scroll(2);
		return;
	}

	if (key == K_HOME || key == K_KP_HOME) {
		key_linepos = 1;
		return;
	}

	if (key == K_END || key == K_KP_END) {
		key_linepos = strlen(key_lines[edit_line]);
		return;
	}

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
	default:
		key = unicode;
		break;
	}

	/** @todo use isprint here? */
	if (key < 32 || key > 127)
		return;					/* non printable */

	if (key_linepos < MAXCMDLINE - 1) {
		if (key_insert) {  /* can't do strcpy to move string to right */
			i = strlen(key_lines[edit_line]) - 1;

			if (i == MAXCMDLINE - 2)
				i--;
			for (; i >= key_linepos; i--)
				key_lines[edit_line][i + 1] = key_lines[edit_line][i];
		}
		i = key_lines[edit_line][key_linepos];
		key_lines[edit_line][key_linepos] = key;
		key_linepos++;
		if (!i)  /* only null terminate if at the end */
			key_lines[edit_line][key_linepos] = 0;
	}
}

/**
 * @brief Handles input when cls.key_dest == key_message
 * @note Used for chatting and cvar editing via menu
 * @sa Key_Event
 * @sa MN_LeftClick
 * @sa CL_MessageMenu_f
 */
static void Key_Message (int key)
{
	int utf8len;

	if (key == K_ENTER || key == K_KP_ENTER) {
		qboolean send = qtrue;

		switch (msg_mode) {
		case MSG_SAY:
			if (msg_buffer[0])
				Cbuf_AddText("say \"");
			else
				send = qfalse;
			break;
		case MSG_IRC:
			/* end the editing (don't cancel) */
			Cbuf_AddText("irc_chanmsg \"");
			break;
		case MSG_SAY_TEAM:
			if (msg_buffer[0])
				Cbuf_AddText("say_team \"");
			else
				send = qfalse;
			break;
		case MSG_MENU:
			/* end the editing (don't cancel) */
			Cbuf_AddText("mn_msgedit \":");
			break;
		default:
			Com_Printf("Invalid msg_mode\n");
			return;
		}
		if (send) {
			Com_DPrintf(DEBUG_CLIENT, "msg_buffer: %s\n", msg_buffer);
			Cbuf_AddText(msg_buffer);
			Cbuf_AddText("\"\n");
		}

		if (msg_mode != MSG_IRC)
			Key_SetDest(key_game);
		msg_bufferlen = 0;
		msg_buffer[0] = 0;
		return;
	}

	if (key == K_ESCAPE) {
		if (cls.state != ca_active) {
			/* If connecting or loading a level, disconnect */
			if (cls.state != ca_disconnected)
				Com_Error(ERR_DROP, "Disconnected from server");
		}

		Key_SetDest(key_game);
		msg_bufferlen = 0;
		msg_buffer[0] = 0;
		/* cancel the inline cvar editing */
		switch (msg_mode) {
		case MSG_IRC:
			/** @todo Why not use MN_PopMenu directly here? Is there a need to
			 * execute this in the next frame? And pending command before it? */
			Cbuf_AddText("mn_pop esc;");
			/* fall through */
			Irc_Input_Deactivate();
		case MSG_MENU:
			Cbuf_AddText("mn_msgedit !\n");
			break;
		}

		return;
	}

	if (key == K_BACKSPACE) {
		if (msg_bufferlen) {
			msg_bufferlen = UTF8_delete_char(msg_buffer, msg_bufferlen - 1);
			if (msg_mode == MSG_MENU || msg_mode == MSG_IRC)
				Cbuf_AddText(va("mn_msgedit \"%s\"\n", msg_buffer));
		}
		return;
	}

	utf8len = UTF8_encoded_len(key);

	/** @todo figure out which unicode codes to accept */
	if (utf8len == 0 || key < 32 || (key >= 127 && key < 192))
		return;					/* non printable */


	if (msg_bufferlen + utf8len >= sizeof(msg_buffer))
		return;					/* all full */

	/* limit the length for cvar inline editing */
	if ((msg_mode == MSG_MENU || msg_mode == MSG_IRC) && msg_bufferlen + utf8len > mn_inputlength->integer) {
		Com_Printf("Input buffer length exceeded\n");
		return;
	}

	msg_bufferlen += UTF8_insert_char(msg_buffer, sizeof(msg_buffer),  msg_bufferlen, key);

	if (msg_mode == MSG_MENU || msg_mode == MSG_IRC)
		Cbuf_AddText(va("mn_msgedit \"%s\"\n", msg_buffer));
}


/**
 * @brief Convert to given string to keynum
 * @param[in] str The keystring to convert to keynum
 * @return a key number to be used to index keybindings[] by looking at
 * the given string.  Single ascii characters return themselves, while
 * the K_* names are matched up.
 * @sa Key_KeynumToString
 */
static int Key_StringToKeynum (const char *str)
{
	const keyname_t *kn;

	if (!str || !str[0])
		return -1;

	/* single char? */
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
 * @todo handle quote special (general escape sequence?)
 * @sa Key_StringToKeynum
 */
const char *Key_KeynumToString (int keynum)
{
	const keyname_t *kn;
	static char tinystr[2];

	if (keynum == -1)
		return "<KEY NOT FOUND>";
	/** @todo use isprint here? */
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
 * @brief Return the key binding for a given script command
 * @param[in] binding The script command to bind keynum to
 * @sa Key_SetBinding
 * @return the binded key or empty string if not found
 */
const char* Key_GetBinding (const char *binding, keyBindSpace_t space)
{
	int i;
	char **keySpace = NULL;

	switch (space) {
	case KEYSPACE_MENU:
		keySpace = menukeybindings;
		break;
	case KEYSPACE_GAME:
		keySpace = keybindings;
		break;
	case KEYSPACE_BATTLE:
		keySpace = battlekeybindings;
		break;
	default:
		return "";
	}

	for (i = K_FIRST_KEY; i < K_LAST_KEY; i++)
		if (keySpace[i] && *keySpace[i] && !Q_strncmp(keySpace[i], binding, strlen(binding))) {
			return Key_KeynumToString(i);
		}

	/* not found */
	return "";
}

/**
 * @brief Bind a keynum to script command
 * @param[in] keynum Converted from string to keynum
 * @param[in] binding The script command to bind keynum to
 * @param[in] space The key space to bind the key for (menu, game or battle)
 * @sa Key_Bind_f
 * @sa Key_StringToKeynum
 * @note If command is empty, this function will only remove the actual key binding instead of setting empty string.
 */
static void Key_SetBinding (int keynum, const char *binding, keyBindSpace_t space)
{
	char *new;
	char **keySpace = NULL;

	if (keynum == -1 || keynum >= K_KEY_SIZE)
		return;

	Com_DPrintf(DEBUG_CLIENT, "Binding for '%s' for space ", binding);
	switch (space) {
	case KEYSPACE_MENU:
		keySpace = &menukeybindings[keynum];
		Com_DPrintf(DEBUG_CLIENT, "menu\n");
		break;
	case KEYSPACE_GAME:
		keySpace = &keybindings[keynum];
		Com_DPrintf(DEBUG_CLIENT, "game\n");
		break;
	case KEYSPACE_BATTLE:
		keySpace = &battlekeybindings[keynum];
		Com_DPrintf(DEBUG_CLIENT, "battle\n");
		break;
	default:
		Com_DPrintf(DEBUG_CLIENT, "failure\n");
		return;
	}

	/* free old bindings */
	if (*keySpace) {
		Mem_Free(*keySpace);
		*keySpace = NULL;
	}

	/* allocate memory for new binding, but don't set empty commands*/
	if (binding)
	{
		new = Mem_PoolStrDup(binding, com_genericPool, CL_TAG_NONE);
		*keySpace = new;
	}
}

/**
 * @brief Unbind a given key binding
 * @sa Key_SetBinding
 */
static void Key_Unbind_f (void)
{
	int b;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <key> : remove commands from a key\n", Cmd_Argv(0));
		return;
	}

	b = Key_StringToKeynum(Cmd_Argv(1));
	if (b == -1) {
		Com_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	if (!Q_strncmp(Cmd_Argv(0), "unbindmenu", MAX_VAR))
		Key_SetBinding(b, "", KEYSPACE_MENU);
	else if (!Q_strncmp(Cmd_Argv(0), "unbindbattle", MAX_VAR))
		Key_SetBinding(b, "", KEYSPACE_BATTLE);
	else
		Key_SetBinding(b, "", KEYSPACE_GAME);
}

/**
 * @brief Unbind all key bindings
 * @sa Key_SetBinding
 */
static void Key_Unbindall_f (void)
{
	int i;

	for (i = K_FIRST_KEY; i < K_LAST_KEY; i++)
		if (keybindings[i]) {
			if (!Q_strncmp(Cmd_Argv(0), "unbindallmenu", MAX_VAR))
				Key_SetBinding(i, "", KEYSPACE_MENU);
			else
				Key_SetBinding(i, "", KEYSPACE_GAME);
		}
}


/**
 * @brief Binds a key to a given script command
 * @sa Key_SetBinding
 */
static void Key_Bind_f (void)
{
	int i, c, b;
	char cmd[1024];

	c = Cmd_Argc();

	if (c < 2) {
		Com_Printf("Usage: %s <key> [command] : attach a command to a key\n", Cmd_Argv(0));
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
	cmd[0] = '\0';					/* start out with a null string */
	for (i = 2; i < c; i++) {
		Q_strcat(cmd, Cmd_Argv(i), sizeof(cmd));
		if (i != (c - 1))
			Q_strcat(cmd, " ", sizeof(cmd));
	}

	if (!Q_strncmp(Cmd_Argv(0), "bindmenu", MAX_VAR))
		Key_SetBinding(b, cmd, KEYSPACE_MENU);
	else if (!Q_strncmp(Cmd_Argv(0), "bindbattle", MAX_VAR))
		Key_SetBinding(b, cmd, KEYSPACE_BATTLE);
	else
		Key_SetBinding(b, cmd, KEYSPACE_GAME);
}

/**
 * @brief Writes lines containing "bind key value"
 * @param[in] filename Path to print the keybinding too
 * @sa Com_WriteConfigToFile
 */
void Key_WriteBindings (const char* filename)
{
	int i;
	/* this gets true in case of an error */
	qboolean delete = qfalse;
	qFILE f;

	memset(&f, 0, sizeof(f));
	FS_OpenFileWrite(va("%s/%s", FS_Gamedir(), filename), &f);
	if (!f.f) {
		Com_Printf("Couldn't write %s.\n", filename);
		return;
	}

	fprintf(f.f, "// generated by ufo, do not modify\n");
	fprintf(f.f, "// If you want to know the keyname of a specific key - set in_debug cvar to 1 and press the key\n");
	fprintf(f.f, "unbindallmenu\n");
	fprintf(f.f, "unbindall\n");
	fprintf(f.f, "unbindbattle\n");
	/* failfast, stops loop for first occurred error in fprintf */
	for (i = 0; i < K_LAST_KEY && delete == qfalse; i++)
		if (menukeybindings[i] && menukeybindings[i][0])
			if (fprintf(f.f, "bindmenu %s \"%s\"\n", Key_KeynumToString(i), menukeybindings[i]) < 0)
				delete = qtrue;
	for (i = 0; i < K_LAST_KEY && delete == qfalse; i++)
		if (keybindings[i] && keybindings[i][0])
			if (fprintf(f.f, "bind %s \"%s\"\n", Key_KeynumToString(i), keybindings[i]) < 0)
				delete = qtrue;
	for (i = 0; i < K_LAST_KEY && delete == qfalse; i++)
		if (battlekeybindings[i] && battlekeybindings[i][0])
			if (fprintf(f.f, "bindbattle %s \"%s\"\n", Key_KeynumToString(i), battlekeybindings[i]) < 0)
				delete = qtrue;
	FS_CloseFile(&f);
	if (!delete)
		Com_Printf("Wrote %s\n", filename);
	else
		/* error in writing the keys.cfg - remove the file again */
		FS_RemoveFile(va("%s/%s", FS_Gamedir(), filename));
}

/**
 * @sa Com_WriteConfigToFile
 */
static void Key_WriteBindings_f (void)
{
	char filename[MAX_QPATH];

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <filename>\n", Cmd_Argv(0));
		return;
	}

	Q_strncpyz(filename, Cmd_Argv(1), sizeof(filename));
	COM_DefaultExtension(filename, sizeof(filename), ".cfg");
	Key_WriteBindings(filename);
}

/**
 * @brief List all binded keys with its function
 */
static void Key_Bindlist_f (void)
{
	int i;

	Com_Printf("key space: game\n");
	for (i = K_FIRST_KEY; i < K_LAST_KEY; i++)
		if (keybindings[i] && keybindings[i][0])
			Com_Printf("- %s \"%s\"\n", Key_KeynumToString(i), keybindings[i]);
	Com_Printf("key space: menu\n");
	for (i = K_FIRST_KEY; i < K_LAST_KEY; i++)
		if (menukeybindings[i] && menukeybindings[i][0])
			Com_Printf("- %s \"%s\"\n", Key_KeynumToString(i), menukeybindings[i]);
	Com_Printf("key space: battle\n");
	for (i = 0; i < K_LAST_KEY; i++)
		if (battlekeybindings[i] && battlekeybindings[i][0])
			Com_Printf("- %s \"%s\"\n", Key_KeynumToString(i), battlekeybindings[i]);

}

static int Key_CompleteKeyName (const char *partial, const char **match)
{
	int matches = 0;
	const char *localMatch[MAX_COMPLETE];
	size_t len;
	const keyname_t *kn;

	len = strlen(partial);
	if (!len) {
		for (kn = keynames; kn->name; kn++) {
			Com_Printf("%s\n", kn->name);
		}
		return 0;
	}

	/* check for partial matches */
	for (kn = keynames; kn->name; kn++) {
		if (!Q_strncmp(partial, kn->name, len)) {
			Com_Printf("%s\n", kn->name);
			localMatch[matches++] = kn->name;
			if (matches >= MAX_COMPLETE)
				break;
		}
	}

	return Cmd_GenericCompleteFunction(len, match, matches, localMatch);
}

void Key_Init (void)
{
	int i;

	for (i = 0; i < MAXKEYLINES; i++) {
		key_lines[i][0] = CONSOLE_PROMPT_CHAR | CONSOLE_COLORED_TEXT_MASK;
		key_lines[i][1] = 0;
	}
	key_linepos = 1;

	/* register our functions */
	Cmd_AddCommand("bindmenu", Key_Bind_f, "Bind a key to a console command - only executed when hovering a menu");
	Cmd_AddCommand("bind", Key_Bind_f, "Bind a key to a console command");
	Cmd_AddCommand("bindbattle", Key_Bind_f, "Bind a key to a console command - only executed when in battlescape");
	Cmd_AddCommand("unbindmenu", Key_Unbind_f, "Unbind a key");
	Cmd_AddCommand("unbind", Key_Unbind_f, "Unbind a key");
	Cmd_AddCommand("unbindbattle", Key_Unbind_f, "Unbind a key");
	Cmd_AddParamCompleteFunction("bind", Key_CompleteKeyName);
	Cmd_AddParamCompleteFunction("unbind", Key_CompleteKeyName);
	Cmd_AddParamCompleteFunction("bindmenu", Key_CompleteKeyName);
	Cmd_AddParamCompleteFunction("unbindmenu", Key_CompleteKeyName);
	Cmd_AddParamCompleteFunction("bindbattle", Key_CompleteKeyName);
	Cmd_AddParamCompleteFunction("unbindbattle", Key_CompleteKeyName);
	Cmd_AddCommand("unbindallmenu", Key_Unbindall_f, "Delete all key bindings for the menu");
	Cmd_AddCommand("unbindall", Key_Unbindall_f, "Delete all key bindings");
	Cmd_AddCommand("unbindallbattle", Key_Unbindall_f, "Delete all key bindings for battlescape");
	Cmd_AddCommand("bindlist", Key_Bindlist_f, "Show all bindings on the game console");
	Cmd_AddCommand("savebind", Key_WriteBindings_f, "Saves key bindings to keys.cfg");
}

/**
 * @brief Sets the key_dest in cls
 * @param[in] key_dest see keydest_t
 */
void Key_SetDest (int key_dest)
{
	cls.key_dest_old = cls.key_dest;
	cls.key_dest = key_dest;
}

/**
 * @brief Called by the system between frames for both key up and key down events
 * @note Should NOT be called during an interrupt!
 * @sa Key_Message
 */
void Key_Event (unsigned int key, unsigned short unicode, qboolean down, unsigned time)
{
	const char *kb = NULL;
	char cmd[MAX_STRING_CHARS];

	/* unbindable key */
	if (key >= K_KEY_SIZE)
		return;

	if (cls.key_dest != key_console && down) {
		switch (key) {
		case K_ESCAPE:
			MN_FocusRemove();
			break;
		case K_TAB:
			if (MN_FocusNextActionNode())
				return;
			break;
		case K_ENTER:
		case K_KP_ENTER:
			if (MN_FocusExecuteActionNode())
				return;
			break;
		}
	}

	/* any key (except F1-F12) during the sequence mode will bring up the menu */
	if (cls.state == ca_sequence && !(key >= K_F1 && key <= K_F12))
		key = K_ESCAPE;

	/* menu key is hardcoded, so the user can never unbind it */
	if (key == K_ESCAPE) {
		if (!down)
			return;

		if (cls.playingCinematic == CIN_STATUS_FULLSCREEN)
			CIN_StopCinematic();

		switch (cls.key_dest) {
		case key_input:
		case key_message:
			Key_Message(unicode);
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
	if (!down) {
		int i;
		/* key up events only generate commands if the game key binding is
		 * a button command (leading + sign).  These will occur even in console mode,
		 * to keep the character from continuing an action started before a console
		 * switch.  Button commands include the kenum as a parameter, so multiple
		 * downs can be matched with ups */
		kb = menukeybindings[key];
		/* this loop ensures, that every down event reaches it's proper kbutton_t */
		for (i = 0; i < 2; i++) {
			if (kb && kb[0] == '+') {
				/* '-' means we have released the key
				 * the key number is used to determine whether the kbutton_t is really
				 * released or whether any other bound key will still ensure that the
				 * kbutton_t is pressed
				 * the time is the msec value when the key was released */
				Com_sprintf(cmd, sizeof(cmd), "-%s %i %i\n", kb + 1, key, time);
				Cbuf_AddText(cmd);
			}
			kb = keybindings[key];
		}
		/*@todo check how battlekeybindings must be taken in account here */
		return;
	}

	/* if not a consolekey, send to the interpreter no matter what mode is */
	if (cls.key_dest == key_game ||
		(cls.key_dest == key_input && key >= K_MOUSE1 && key <= K_MWHEELUP)) {
		/* Some keyboards need modifiers to access key values that are
		 * present as bare keys on other keyboards. Smooth over the difference
		 * here by using the translated value if there is a binding for it. */
		if (mouseSpace == MS_MENU && unicode >= 32 && unicode < 127)
			kb = menukeybindings[unicode];
		if (!kb && mouseSpace == MS_MENU)
			kb = menukeybindings[key];
		if (!kb && unicode >= 32 && unicode < 127)
			kb = keybindings[unicode];
		if (!kb)
			kb = keybindings[key];
		if (!kb && CL_OnBattlescape() == qtrue)
			kb = battlekeybindings[key];
		if (kb) {
			if (kb[0] == '+') {	/* button commands add keynum and time as a parm */
				/* '+' means we have pressed the key
				 * the key number is used because the kbutton_t can be 'pressed' by several keys
				 * the time is the msec value when the key was pressed */
				Com_sprintf(cmd, sizeof(cmd), "%s %i %i\n", kb, key, time);
				Cbuf_AddText(cmd);
			} else {
				Cbuf_AddText(kb);
				Cbuf_AddText("\n");
			}
			if (cls.key_dest == key_game)
				return;
		}
	}

	if (!down)
		return;	/* other systems only care about key down events */

	switch (cls.key_dest) {
	case key_input:
	case key_message:
		Key_Message(unicode);
		break;
	case key_game:
	case key_console:
		Key_Console(key, unicode);
		break;
	default:
		Com_Error(ERR_FATAL, "Bad cls.key_dest");
	}
}
