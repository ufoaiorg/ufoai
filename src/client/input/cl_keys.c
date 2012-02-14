/**
 * @file cl_keys.c
 * @brief Keyboard handling routines.
 *
 * Note: Key up events are sent even if in console mode
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "../client.h"
#include "../cl_console.h"
#include "../ui/ui_input.h"
#include "../ui/ui_nodes.h"
#include "../../shared/utf8.h"

char keyLines[MAXKEYLINES][MAXCMDLINE];
uint32_t keyLinePos;

static int keyInsert = 1;

int editLine = 0;
int historyLine = 0;

int msgMode;
char msgBuffer[MAXCMDLINE];
size_t msgBufferLen = 0;

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

char *keyBindings[K_KEY_SIZE];
char *menuKeyBindings[K_KEY_SIZE];
char *battleKeyBindings[K_KEY_SIZE];

static qboolean keyDown[K_KEY_SIZE];

typedef struct {
	const char *name;
	int keynum;
} keyName_t;

#define M(x) {#x, K_##x}
static const keyName_t keyNames[] = {
	M(TAB),
	M(ENTER),
	M(ESCAPE),
	M(SPACE),
	M(BACKSPACE),
	M(UPARROW),
	M(DOWNARROW),
	M(LEFTARROW),
	M(RIGHTARROW),

	M(ALT),
	M(CTRL),
	M(SHIFT),

	M(F1),
	M(F2),
	M(F3),
	M(F4),
	M(F5),
	M(F6),
	M(F7),
	M(F8),
	M(F9),
	M(F10),
	M(F11),
	M(F12),

	M(INS),
	M(DEL),
	M(PGDN),
	M(PGUP),
	M(HOME),
	M(END),

	M(MOUSE1),
	M(MOUSE2),
	M(MOUSE3),
	M(MOUSE4),
	M(MOUSE5),

	M(AUX1),
	M(AUX2),
	M(AUX3),
	M(AUX4),
	M(AUX5),
	M(AUX6),
	M(AUX7),
	M(AUX8),
	M(AUX9),
	M(AUX10),
	M(AUX11),
	M(AUX12),
	M(AUX13),
	M(AUX14),
	M(AUX15),
	M(AUX16),

	M(APPS),

	M(KP_HOME),
	M(KP_UPARROW),
	M(KP_PGUP),
	M(KP_LEFTARROW),
	M(KP_5),
	M(KP_RIGHTARROW),
	M(KP_END),
	M(KP_DOWNARROW),
	M(KP_PGDN),
	M(KP_ENTER),
	M(KP_INS),
	M(KP_DEL),
	M(KP_SLASH),
	M(KP_MINUS),
	M(KP_PLUS),

	M(MWHEELUP),
	M(MWHEELDOWN),

	M(JOY1),
	M(JOY2),
	M(JOY3),
	M(JOY4),
	M(JOY5),
	M(JOY6),
	M(JOY7),
	M(JOY8),
	M(JOY9),
	M(JOY10),
	M(JOY11),
	M(JOY12),
	M(JOY13),
	M(JOY14),
	M(JOY15),
	M(JOY16),
	M(JOY17),
	M(JOY18),
	M(JOY19),
	M(JOY20),
	M(JOY21),
	M(JOY22),
	M(JOY23),
	M(JOY24),
	M(JOY25),
	M(JOY26),
	M(JOY27),
	M(JOY28),
	M(JOY29),
	M(JOY30),
	M(JOY31),
	M(JOY32),

	M(PAUSE),

	{"SEMICOLON", ';'},			/* because a raw semicolon seperates commands */

	M(SUPER),
	M(COMPOSE),
	M(MODE),
	M(HELP),
	M(PRINT),
	M(SYSREQ),
	M(SCROLLOCK),
	M(BREAK),
	M(MENU),
	M(POWER),
	M(EURO),
	M(UNDO),

	{NULL, 0}
};
#undef M

/**
 * @brief Checks whether a given key is currently pressed
 * @param[in] key The key to check, @sa @c keyNum_t
 * @return @c true if the key is pressed, @c false otherwise
 */
qboolean Key_IsDown (unsigned int key)
{
	if (key >= K_KEY_SIZE)
		return qfalse;
	return keyDown[key];
}

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

	if (keyDown[K_CTRL]) {
		switch (toupper(key)) {
		/* ctrl-L clears screen */
		case 'L':
			Cbuf_AddText("clear\n");
			return;
		/* jump to beginning of line */
		case 'A':
			keyLinePos = 1;
			return;
		/* end of line */
		case 'E':
			keyLinePos = strlen(keyLines[editLine]);
			return;
		}
	}

	if (key == K_ENTER || key == K_KP_ENTER) {	/* backslash text are commands, else chat */
		if (keyLines[editLine][1] == '\\' || keyLines[editLine][1] == '/')
			Cbuf_AddText(keyLines[editLine] + 2);	/* skip the > */
		/* no command - just enter */
		else if (!keyLines[editLine][1])
			return;
		else
			Cbuf_AddText(keyLines[editLine] + 1);	/* valid command */

		Cbuf_AddText("\n");
		Com_Printf("%s\n", keyLines[editLine] + 1);
		editLine = (editLine + 1) & (MAXKEYLINES - 1);
		historyLine = editLine;
		keyLines[editLine][0] = CONSOLE_PROMPT_CHAR;
		/* maybe MAXKEYLINES was reached - we don't want to spawn 'random' strings
		 * from history buffer in our console */
		keyLines[editLine][1] = '\0';
		keyLinePos = 1;

		return;
	}

	/* command completion */
	if (key == K_TAB) {
		Com_ConsoleCompleteCommand(&keyLines[editLine][1], keyLines[editLine], MAXCMDLINE, &keyLinePos, 1);
		return;
	}

	if (key == K_BACKSPACE || (key == 'h' && keyDown[K_CTRL])) {
		if (keyLinePos > 1) {
			strcpy(keyLines[editLine] + keyLinePos - 1, keyLines[editLine] + keyLinePos);
			keyLinePos--;
		}
		return;
	}
	/* delete char on cursor */
	if (key == K_DEL) {
		if (keyLinePos < strlen(keyLines[editLine]))
			strcpy(keyLines[editLine] + keyLinePos, keyLines[editLine] + keyLinePos + 1);
		return;
	}

	if (key == K_UPARROW || key == K_KP_UPARROW || (tolower(key) == 'p' && keyDown[K_CTRL])) {
		do {
			historyLine = (historyLine - 1) & (MAXKEYLINES - 1);
		} while (historyLine != editLine && !keyLines[historyLine][1]);

		if (historyLine == editLine)
			historyLine = (editLine + 1) & (MAXKEYLINES - 1);

		Q_strncpyz(keyLines[editLine], keyLines[historyLine], MAXCMDLINE);
		keyLinePos = strlen(keyLines[editLine]);
		return;
	} else if (key == K_DOWNARROW || key == K_KP_DOWNARROW || (tolower(key) == 'n' && keyDown[K_CTRL])) {
		if (historyLine == editLine)
			return;
		do {
			historyLine = (historyLine + 1) & (MAXKEYLINES - 1);
		} while (historyLine != editLine && !keyLines[historyLine][1]);

		if (historyLine == editLine) {
			keyLines[editLine][0] = CONSOLE_PROMPT_CHAR;
			/* fresh edit line */
			keyLines[editLine][1] = '\0';
			keyLinePos = 1;
		} else {
			Q_strncpyz(keyLines[editLine], keyLines[historyLine], MAXCMDLINE);
			keyLinePos = strlen(keyLines[editLine]);
		}
		return;
	}

	if (key == K_LEFTARROW) {  /* move cursor left */
		if (keyDown[K_CTRL]) { /* by a whole word */
			while (keyLinePos > 1 && keyLines[editLine][keyLinePos - 1] == ' ')
				keyLinePos--;  /* get off current word */
			while (keyLinePos > 1 && keyLines[editLine][keyLinePos - 1] != ' ')
				keyLinePos--;  /* and behind previous word */
			return;
		}

		if (keyLinePos > 1)  /* or just a char */
			keyLinePos--;
		return;
	} else if (key == K_RIGHTARROW) {  /* move cursor right */
		if ((i = strlen(keyLines[editLine])) == keyLinePos)
			return; /* no character to get */
		if (keyDown[K_CTRL]) {  /* by a whole word */
			while (keyLinePos < i && keyLines[editLine][keyLinePos + 1] == ' ')
				keyLinePos++;  /* get off current word */
			while (keyLinePos < i && keyLines[editLine][keyLinePos + 1] != ' ')
				keyLinePos++;  /* and in front of next word */
			if (keyLinePos < i)  /* all the way in front */
				keyLinePos++;
			return;
		}
		keyLinePos++;  /* or just a char */
		return;
	}

	/* toggle insert mode */
	if (key == K_INS) {
		keyInsert ^= 1;
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
		keyLinePos = 1;
		return;
	}

	if (key == K_END || key == K_KP_END) {
		keyLinePos = strlen(keyLines[editLine]);
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

	if (keyLinePos < MAXCMDLINE - 1) {
		if (keyInsert) {  /* can't do strcpy to move string to right */
			i = strlen(keyLines[editLine]) - 1;

			if (i == MAXCMDLINE - 2)
				i--;
			for (; i >= keyLinePos; i--)
				keyLines[editLine][i + 1] = keyLines[editLine][i];
		}
		i = keyLines[editLine][keyLinePos];
		keyLines[editLine][keyLinePos] = key;
		keyLinePos++;
		if (!i)  /* only null terminate if at the end */
			keyLines[editLine][keyLinePos] = 0;
	}
}

/**
 * @brief Convert to given string to keynum
 * @param[in] str The keystring to convert to keynum
 * @return a key number to be used to index keyBindings[] by looking at
 * the given string.  Single ascii characters return themselves, while
 * the K_* names are matched up.
 * @sa Key_KeynumToString
 */
int Key_StringToKeynum (const char *str)
{
	const keyName_t *kn;

	if (Q_strnull(str))
		return -1;

	/* single char? */
	if (str[1] == '\0')
		return str[0];

	for (kn = keyNames; kn->name; kn++) {
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
	const keyName_t *kn;
	static char tinystr[2];

	if (keynum == -1)
		return "<KEY NOT FOUND>";
	/** @todo use isprint here? */
	if (keynum > 32 && keynum < 127) {	/* printable ascii */
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}

	for (kn = keyNames; kn->name; kn++)
		if (keynum == kn->keynum)
			return kn->name;

	return "<UNKNOWN KEYNUM>";
}

/**
 * @brief Return the key binding for a given script command
 * @param[in] binding The script command to bind keynum to
 * @param space Namespace of the key binding
 * @sa Key_SetBinding
 * @return the binded key or empty string if not found
 */
const char* Key_GetBinding (const char *binding, keyBindSpace_t space)
{
	int i;
	char **keySpace = NULL;

	switch (space) {
	case KEYSPACE_UI:
		keySpace = menuKeyBindings;
		break;
	case KEYSPACE_GAME:
		keySpace = keyBindings;
		break;
	case KEYSPACE_BATTLE:
		keySpace = battleKeyBindings;
		break;
	default:
		Sys_Error("Unknown key space (%i) given", space);
	}

	for (i = K_FIRST_KEY; i < K_LAST_KEY; i++)
		if (keySpace[i] && *keySpace[i] && Q_streq(keySpace[i], binding)) {
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
void Key_SetBinding (int keynum, const char *binding, keyBindSpace_t space)
{
	char **keySpace = NULL;

	if (keynum == -1 || keynum >= K_KEY_SIZE)
		return;

	Com_DPrintf(DEBUG_CLIENT, "Binding for '%s' for space ", binding);
	switch (space) {
	case KEYSPACE_UI:
		keySpace = &menuKeyBindings[keynum];
		Com_DPrintf(DEBUG_CLIENT, "menu\n");
		break;
	case KEYSPACE_GAME:
		keySpace = &keyBindings[keynum];
		Com_DPrintf(DEBUG_CLIENT, "game\n");
		break;
	case KEYSPACE_BATTLE:
		keySpace = &battleKeyBindings[keynum];
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
		*keySpace = Mem_PoolStrDup(binding, com_genericPool, 0);
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

	if (Q_streq(Cmd_Argv(0), "unbindmenu"))
		Key_SetBinding(b, "", KEYSPACE_UI);
	else if (Q_streq(Cmd_Argv(0), "unbindbattle"))
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
		if (keyBindings[i]) {
			if (Q_streq(Cmd_Argv(0), "unbindallmenu"))
				Key_SetBinding(i, "", KEYSPACE_UI);
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
	int c, b;

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
		if (keyBindings[b])
			Com_Printf("\"%s\" = \"%s\"\n", Cmd_Argv(1), keyBindings[b]);
		else
			Com_Printf("\"%s\" is not bound\n", Cmd_Argv(1));
		return;
	}


	if (Q_streq(Cmd_Argv(0), "bindui"))
		UI_SetKeyBinding(Cmd_Argv(2), b, Cmd_Argv(3));
	else if (Q_streq(Cmd_Argv(0), "bindmenu"))
		Key_SetBinding(b, Cmd_Argv(2), KEYSPACE_UI);
	else if (Q_streq(Cmd_Argv(0), "bindbattle"))
		Key_SetBinding(b, Cmd_Argv(2), KEYSPACE_BATTLE);
	else
		Key_SetBinding(b, Cmd_Argv(2), KEYSPACE_GAME);
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
	qboolean deleteFile = qfalse;
	qFILE f;
	int cnt = 0;

	OBJZERO(f);
	FS_OpenFile(filename, &f, FILE_WRITE);
	if (!f.f) {
		Com_Printf("Couldn't write %s.\n", filename);
		return;
	}

	FS_Printf(&f, "// generated by ufo, do not modify\n");
	FS_Printf(&f, "// If you want to know the keyname of a specific key - set in_debug cvar to 1 and press the key\n");
	FS_Printf(&f, "unbindallmenu\n");
	FS_Printf(&f, "unbindall\n");
	FS_Printf(&f, "unbindallbattle\n");
	/* failfast, stops loop for first occurred error in fprintf */
	for (i = 0; i < K_LAST_KEY && !deleteFile; i++)
		if (menuKeyBindings[i] && menuKeyBindings[i][0]) {
			if (FS_Printf(&f, "bindmenu %s \"%s\"\n", Key_KeynumToString(i), menuKeyBindings[i]) < 0)
				deleteFile = qtrue;
			cnt++;
		}
	for (i = 0; i < K_LAST_KEY && !deleteFile; i++)
		if (keyBindings[i] && keyBindings[i][0]) {
			if (FS_Printf(&f, "bind %s \"%s\"\n", Key_KeynumToString(i), keyBindings[i]) < 0)
				deleteFile = qtrue;
			cnt++;
		}
	for (i = 0; i < K_LAST_KEY && !deleteFile; i++)
		if (battleKeyBindings[i] && battleKeyBindings[i][0]) {
			if (FS_Printf(&f, "bindbattle %s \"%s\"\n", Key_KeynumToString(i), battleKeyBindings[i]) < 0)
				deleteFile = qtrue;
			cnt++;
		}

	for (i = 0; i < UI_GetKeyBindingCount(); i++) {
		const char *path;
		uiKeyBinding_t*binding = UI_GetKeyBindingByIndex(i);

		if (binding->node == NULL)
			continue;
		if (binding->inherited)
			continue;
		if (binding->property == NULL)
			path = va("%s", UI_GetPath(binding->node));
		else
			path = va("%s@%s", UI_GetPath(binding->node), binding->property->string);

		if (FS_Printf(&f, "bindui %s \"%s\" \"%s\"\n", Key_KeynumToString(binding->key), path, binding->description ? binding->description : "") < 0)
			deleteFile = qtrue;
	}

	FS_CloseFile(&f);
	if (!deleteFile && cnt)
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
	Com_DefaultExtension(filename, sizeof(filename), ".cfg");
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
		if (keyBindings[i] && keyBindings[i][0])
			Com_Printf("- %s \"%s\"\n", Key_KeynumToString(i), keyBindings[i]);
	Com_Printf("key space: menu\n");
	for (i = K_FIRST_KEY; i < K_LAST_KEY; i++)
		if (menuKeyBindings[i] && menuKeyBindings[i][0])
			Com_Printf("- %s \"%s\"\n", Key_KeynumToString(i), menuKeyBindings[i]);
	Com_Printf("key space: battle\n");
	for (i = 0; i < K_LAST_KEY; i++)
		if (battleKeyBindings[i] && battleKeyBindings[i][0])
			Com_Printf("- %s \"%s\"\n", Key_KeynumToString(i), battleKeyBindings[i]);

}

static int Key_CompleteKeyName (const char *partial, const char **match)
{
	int matches = 0;
	const char *localMatch[MAX_COMPLETE];
	size_t len;
	const keyName_t *kn;

	len = strlen(partial);
	if (!len) {
		for (kn = keyNames; kn->name; kn++) {
			Com_Printf("%s\n", kn->name);
		}
		return 0;
	}

	/* check for partial matches */
	for (kn = keyNames; kn->name; kn++) {
		if (!strncmp(partial, kn->name, len)) {
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
		keyLines[i][0] = CONSOLE_PROMPT_CHAR;
		keyLines[i][1] = 0;
	}
	keyLinePos = 1;

	/* register our functions */
	Cmd_AddCommand("bindui", Key_Bind_f, "Bind a key to a ui node");
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
 * @brief Sets the keyDest in cls
 * @param[in] keyDest see keydest_t
 */
void Key_SetDest (int keyDest)
{
	cls.keyDest = keyDest;
	if (cls.keyDest == key_console) {
		/* make sure the menu no more capture inputs */
		UI_ReleaseInput();
	}
}

/**
 * @brief Called by the system between frames for both key up and key down events
 * @note Should NOT be called during an interrupt!
 * @sa Key_Message
 */
void Key_Event (unsigned int key, unsigned short unicode, qboolean down, unsigned time)
{
	char cmd[MAX_STRING_CHARS];

	/* unbindable key */
	if (key >= K_KEY_SIZE)
		return;

	/* any key (except F1-F12) during the sequence mode will bring up the menu */

	if (cls.keyDest == key_game) {
		if (down && UI_KeyPressed(key, unicode))
			return;
		else if (!down && UI_KeyRelease(key, unicode))
			return;
	}

	/* menu key is hardcoded, so the user can never unbind it */
	if (key == K_ESCAPE) {
		if (!down)
			return;

		switch (cls.keyDest) {
		case key_console:
			Con_ToggleConsole_f();
			break;
		default:
			Com_Error(ERR_FATAL, "Bad cls.key_dest");
		}
		return;
	}

	/* track if any key is down for BUTTON_ANY */
	keyDown[key] = down;
	if (!down) {
		int i;
		/* key up events only generate commands if the game key binding is
		 * a button command (leading + sign).  These will occur even in console mode,
		 * to keep the character from continuing an action started before a console
		 * switch.  Button commands include the kenum as a parameter, so multiple
		 * downs can be matched with ups */
		const char *kb = menuKeyBindings[key];
		/* this loop ensures, that every down event reaches it's proper kbutton_t */
		for (i = 0; i < 3; i++) {
			if (kb && kb[0] == '+') {
				/* '-' means we have released the key
				 * the key number is used to determine whether the kbutton_t is really
				 * released or whether any other bound key will still ensure that the
				 * kbutton_t is pressed
				 * the time is the msec value when the key was released */
				Com_sprintf(cmd, sizeof(cmd), "-%s %i %i\n", kb + 1, key, time);
				Cbuf_AddText(cmd);
			}
			if (i == 0)
				kb = keyBindings[key];
			else
				kb = battleKeyBindings[key];
		}
		return;
	}

	/* if not a consolekey, send to the interpreter no matter what mode is */
	if (cls.keyDest == key_game || (key >= K_MOUSE1 && key <= K_MWHEELUP)) {
		/* Some keyboards need modifiers to access key values that are
		 * present as bare keys on other keyboards. Smooth over the difference
		 * here by using the translated value if there is a binding for it. */
		const char *kb = NULL;
		if (IN_GetMouseSpace() == MS_UI && unicode >= 32 && unicode < 127)
			kb = menuKeyBindings[unicode];
		if (!kb && IN_GetMouseSpace() == MS_UI)
			kb = menuKeyBindings[key];
		if (!kb && unicode >= 32 && unicode < 127)
			kb = keyBindings[unicode];
		if (!kb)
			kb = keyBindings[key];
		if (!kb && CL_OnBattlescape())
			kb = battleKeyBindings[key];
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
			if (cls.keyDest == key_game)
				return;
		}
	}

	if (!down)
		return;	/* other systems only care about key down events */

	switch (cls.keyDest) {
	case key_game:
	case key_console:
		Key_Console(key, unicode);
		break;
	default:
		Com_Error(ERR_FATAL, "Bad cls.key_dest");
	}
}
