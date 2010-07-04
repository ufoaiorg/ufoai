/**
 * @file cgame.h
 * @brief Client game mode interface
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#ifndef CL_CGAME_H
#define CL_CGAME_H

typedef struct {
	const char *name;
	const char *menu;
	int gametype;
	void (EXPORT *Init) (void);
	void (EXPORT *Shutdown) (void);
	/** soldier spawn functions may differ between the different gametypes */
	qboolean (EXPORT *Spawn) (void);
	/** each gametype can handle the current team in a different way */
	int (EXPORT *GetTeam) (void);
	/** some gametypes only support special maps */
	const mapDef_t* (EXPORT *MapInfo) (int step);
	/** some gametypes require extra data in the results parsing (like e.g. campaign mode) */
	void (EXPORT *Results) (struct dbuffer *msg, int, int*, int*, int[][MAX_TEAMS], int[][MAX_TEAMS]);
	/** check whether the given item is usable in the current game mode */
	qboolean (EXPORT *IsItemUseable) (const objDef_t *od);
	/** shows item info if not resolvable via objDef_t */
	void (EXPORT *DisplayItemInfo) (menuNode_t *node, const char *string);
	/** returns the equipment definition the game mode is using */
	equipDef_t* (EXPORT *GetEquipmentDefinition) (void);
	/** update character display values for game type dependent stuff */
	void (EXPORT *UpdateCharacterValues) (const character_t *chr);
	/** checks whether the given team is known in the particular gamemode */
	qboolean (EXPORT *IsTeamKnown) (const teamDef_t *teamDef);
	/** called on errors */
	void (EXPORT *Drop) (void);
	/** called after the team spawn messages where send, can e.g. be used to set initial actor states */
	void (EXPORT *InitializeBattlescape) (const chrList_t *team);
	/** callback that is executed every frame */
	void (EXPORT *RunFrame) (void);
	/** if you want to display a different model for the given object in your game mode, implement this function */
	const char* (EXPORT *GetModelForItem) (const objDef_t*od);
	void (EXPORT *EndRoundAnnounce) (int playerNum, int team);
	void (EXPORT *StartBattlescape) (qboolean isTeamPlay);
} cgame_export_t;

/** @todo define the import interface */
typedef struct {
	csi_t *csi;

	void (IMPORT *MN_ExecuteConfunc) (const char *fmt, ...) __attribute__((format(printf, 1, 2)));

	/* filesystem functions */
	const char *(IMPORT *FS_Gamedir) (void);
	int (IMPORT *FS_LoadFile) (const char *path, byte **buffer);
	void (IMPORT *FS_FreeFile) (void *buffer);

	/* console variable interaction */
	cvar_t *(IMPORT *Cvar_Get) (const char *varName, const char *value, int flags, const char* desc);
	cvar_t *(IMPORT *Cvar_Set) (const char *varName, const char *value);
	const char *(IMPORT *Cvar_String) (const char *varName);

	/* ClientCommand and ServerCommand parameter access */
	int (IMPORT *Cmd_Argc) (void);
	const char *(IMPORT *Cmd_Argv) (int n);
	const char *(IMPORT *Cmd_Args) (void);		/**< concatenation of all argv >= 1 */
} cgame_import_t;

cgame_export_t *GetCGameAPI(cgame_import_t *import);

#endif
