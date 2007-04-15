/**
 * @file cl_save.c
 * @brief Implements savegames
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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
#include "cl_global.h"
#include "cl_save.h"

saveSubsystems_t saveSubsystems[MAX_SAVESUBSYSTEMS];
int saveSubsystemsAmount;

/**
 * @brief Loads a savegame from file
 *
 * @param filename Savegame to load (relative to writepath/save)
 *
 * @sa SAV_GameLoad_f
 * @sa SAV_GameSave
 * @sa CL_GameInit
 * @sa CL_MessageSave
 * @sa CL_ReadSinglePlayerData
 * @sa CL_UpdatePointersInGlobalData
 */
static qboolean SAV_GameLoad (const char *filename)
{
	uLongf len = MAX_GAMESAVESIZE;
	qFILE f;
	byte *buf, *cbuf;
	int res, clen, i;
	sizebuf_t sb;
	saveFileHeader_t header;

	/* open file */
	f.f = fopen(va("%s/save/%s.sav", FS_Gamedir(), filename), "rb");
	if (!f.f) {
		Com_Printf("Couldn't open file '%s'.\n", filename);
		return 1;
	}

	/* read compressed data into cbuf buffer */
	clen = FS_FileLength(&f);
	cbuf = (byte *) malloc(sizeof(byte) * clen);
	if (fread(cbuf, 1, clen, f.f) != clen)
		Com_Printf("Warning: Could not read %i bytes from savefile\n", clen);
	fclose(f.f);

	memcpy(&header, cbuf, sizeof(saveFileHeader_t));
	Com_Printf("Loading savegame\n"
		"...version: %i\n"
		"...game version: %s\n"
		, header.version, header.gameVersion);

	buf = (byte *) malloc(sizeof(byte) * MAX_GAMESAVESIZE);
	SZ_Init(&sb, buf, MAX_GAMESAVESIZE);

	if (header.compressed) {
		/* uncompress data, skipping comment header */
		res = uncompress(buf, &len, cbuf + sizeof(saveFileHeader_t), clen - sizeof(saveFileHeader_t));
		free(cbuf);

		if (res != Z_OK) {
			free(buf);
			Com_Printf("Error decompressing data in '%s'.\n", filename);
			return 1;
		}
		sb.cursize = len;
	} else {
		memcpy(buf, cbuf + sizeof(saveFileHeader_t), clen - sizeof(saveFileHeader_t));
		sb.cursize = clen - sizeof(saveFileHeader_t);
	}

	/* check current version */
	if (header.version > SAVE_FILE_VERSION) {
		Com_Printf("File '%s' is a more recent version (%d) than is supported.\n", filename, header.version);
		free(buf);
		return 1;
	} else if (header.version < SAVE_FILE_VERSION) {
		Com_Printf("Savefileformat has changed ('%s' is version %d) - you may experience problems.\n", filename, header.version);
	}

	/* exit running game */
	if (curCampaign)
		CL_GameExit();

	CL_StartSingleplayer(qtrue);

	memset(&gd, 0, sizeof(gd));
	CL_ReadSinglePlayerData();

	for (i = 0; i < saveSubsystemsAmount; i++) {
		if (!saveSubsystems[i].load(&sb, &header)) {
			Com_Printf("Subsystem '%s' returned false - savegame could not be loaded\n", saveSubsystems[i].name);
			return qfalse;
		}
		if (MSG_ReadByte(&sb) != 0)
			Com_Printf("Subsystem '%s' could not be loaded correctly - savegame might be broken\n", saveSubsystems[i].name);
	}

	/* ensure research correctly initialised */
	RS_UpdateData();

	CL_GameInit();

	Cvar_Set("mn_main", "singleplayerInGame");
	Cvar_Set("mn_active", "map");
	Cbuf_AddText("disconnect\n");

	MN_PopMenu(qtrue);
	MN_PushMenu("map");

	Com_Printf("File '%s' loaded.\n", filename);
	free(buf);

	return qtrue;
}

/**
 * @brief
 */
extern qboolean SAV_GameSave (const char *filename, const char *comment)
{
	sizebuf_t sb;
	int i;
	int res;
	char savegame[MAX_OSPATH];
	byte *buf, *fbuf;
	uLongf bufLen;
	int day, month;
	saveFileHeader_t header;

	if (!curCampaign) {
		Com_Printf("No campaign active.\n");
		return qfalse;
	}

	if (!gd.numBases) {
		Com_Printf("Nothing to save yet.\n");
		return qfalse;
	}

	/* step 1 - get the filename */
	Com_sprintf(savegame, sizeof(savegame), "%s/save/%s.sav", FS_Gamedir(), filename);

	/* step 2 - allocate the buffers */
	buf = (byte *) malloc(sizeof(byte) * MAX_GAMESAVESIZE);
	if (!buf) {
		Com_Printf("Error: Could not allocate enough memory to save this game\n");
		return qfalse;
	}
	/* create data */
	SZ_Init(&sb, buf, MAX_GAMESAVESIZE);

	Com_Printf("Save '%s'\n", filename);
	/* step 3 - serialzer */
	for (i = 0; i < saveSubsystemsAmount; i++) {
		Com_Printf("...subsystem '%s'\n", saveSubsystems[i].name);
		if (!saveSubsystems[i].save(&sb, NULL))
			Com_Printf("...subsystem '%s' failed to save the data\n", saveSubsystems[i].name);
		MSG_WriteByte(&sb, 0);
	}

	/* compress data using zlib before writing */
	bufLen = (uLongf) (24 + 1.02 * sb.cursize);
	fbuf = (byte *) malloc(sizeof(byte) * bufLen + sizeof(saveFileHeader_t));
	/* write an uncompressed header */
	memset(fbuf, 0, sizeof(saveFileHeader_t));

	/* step 4 - write the header */
	memset(&header, 0, sizeof(saveFileHeader_t));
	header.compressed = 1; /* TODO: switchable if zlib is not available */
	header.version = SAVE_FILE_VERSION;
	Q_strncpyz(header.name, comment, sizeof(header.name));
	Q_strncpyz(header.gameVersion, UFO_VERSION, sizeof(header.gameVersion));
	CL_DateConvert(&ccs.date, &day, &month);
	Com_sprintf(header.gameDate, sizeof(header.gameDate), "%02i %s %i",
		day, CL_DateGetMonthName(month), ccs.date.day / 365);
	/* TODO fill real date string */
	memcpy(fbuf, &header, sizeof(saveFileHeader_t));

	/* step 5 - compress */
	if (header.compressed) {
		res = compress(fbuf + sizeof(saveFileHeader_t), &bufLen, buf, sb.cursize);
		free(buf);

		if (res != Z_OK) {
			free(fbuf);
			Com_Printf("Memory error compressing save-game data (%s) (Error: %i)!\n", comment, res);
			return qfalse;
		}
	} else {
		memcpy(fbuf + sizeof(saveFileHeader_t), buf, sb.cursize);
		free(buf);
	}

	/* last step - write data */
	res = FS_WriteFile(fbuf, bufLen + sizeof(saveFileHeader_t), savegame);
	free(fbuf);

	if (res == bufLen + sizeof(saveFileHeader_t)) {
		/* set mn_lastsave to let the continue function know which game to
		 * automatically continue TODO: redo this in the menu */
		Cvar_Set("mn_lastsave", filename);
		Com_Printf("Campaign '%s' saved.\n", comment);
		return qtrue;
	} else {
		Com_Printf("Failed to save campaign '%s' !!!\n", comment);
		return qfalse;
	}
}

/**
 * @brief
 * @sa SAV_GameSave
 */
static void SAV_GameSave_f (void)
{
	char comment[MAX_COMMENTLENGTH] = "";
	const char *arg = NULL;
	qboolean result;

	/* get argument */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: game_save <filename> <comment|*cvar>\n");
		return;
	}

	if (!curCampaign) {
		Com_Printf("No running game - no saving...\n");
		return;
	}

	/* get comment */
	if (Cmd_Argc() > 2) {
		arg = Cmd_Argv(2);
		assert(arg);
		Q_strncpyz(comment, arg, sizeof(comment));
	}

	/* save the game */
	result = SAV_GameSave(Cmd_Argv(1), comment);

	Cbuf_ExecuteText(EXEC_NOW, "mn_pop");

	/* if save failed refresh the SaveGame menu and popup error message */
	if (!result) {
		MN_PushMenu("save");
		MN_Popup(_("Note"), _("Error saving game. Check free disk space!"));
	}
}

/**
 * @brief Console commands to read the comments from savegames
 * @note The comment is the part of the savegame header that you type in at saving
 * for reidentifying the savegame
 * @sa SAV_GameLoad_f
 * @sa SAV_GameLoad
 */
static void SAV_GameSaveNames_f (void)
{
	char comment[MAX_VAR];
	FILE *f;
	int i;
	saveFileHeader_t header;

	if (Cmd_Argc() == 2) {
		/* checks whether we plan to save without a running game */
		if (!Q_strncmp(Cmd_Argv(1), "save", 4) && !curCampaign) {
			Cbuf_ExecuteText(EXEC_NOW, "mn_pop");
			return;
		}
	}

	for (i = 0; i < 8; i++) {
		/* open file */
		f = fopen(va("%s/save/slot%i.sav", FS_Gamedir(), i), "rb");
		if (!f) {
			Cvar_Set(va("mn_slot%i", i), "");
			continue;
		}

		/* read the comment */
		if (fread(&header, 1, sizeof(saveFileHeader_t), f) != sizeof(saveFileHeader_t))
			Com_Printf("Warning: Savefile header may be corrupted\n");
		Com_sprintf(comment, sizeof(comment), "%s - %s", header.name, header.gameDate);
		Cvar_Set(va("mn_slot%i", i), comment);
		fclose(f);
	}
}

/**
 * @brief Console command to load a savegame
 * @sa SAV_GameLoad
 */
static void SAV_GameLoad_f (void)
{
	/* get argument */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: game_load <filename>\n");
		return;
	}

	Com_DPrintf("load file '%s'\n", Cmd_Argv(1));

	/* load and go to map */
	SAV_GameLoad(Cmd_Argv(1));
}

/**
 * @brief Loads the last saved game
 * @note At saving the archive cvar mn_lastsave was set to the latest savegame
 * @sa SAV_GameLoad
 */
static void SAV_GameContinue_f (void)
{
	if (cls.state == ca_active) {
		MN_PopMenu(qfalse);
		return;
	}

	if (!curCampaign) {
		/* try to load the current campaign */
		SAV_GameLoad(mn_lastsave->string);
		if (!curCampaign)
			return;
	} else {
		MN_PopMenu(qfalse);
	}
}

/**
 * @brief
 */
extern qboolean SAV_AddSubsystem (saveSubsystems_t *subsystem)
{
	if (saveSubsystemsAmount >= MAX_SAVESUBSYSTEMS)
		return qfalse;

	saveSubsystems[saveSubsystemsAmount].name = subsystem->name;
	saveSubsystems[saveSubsystemsAmount].load = subsystem->load;
	saveSubsystems[saveSubsystemsAmount].save = subsystem->save;
	saveSubsystemsAmount++;

	Com_Printf("...added %s subsystem\n", subsystem->name);
	return qtrue;
}

/**
 * @brief
 */
extern void SAV_Init (void)
{
	static saveSubsystems_t b_subsystem = {"base", B_Save, B_Load};
	static saveSubsystems_t cp_subsystem = {"campaign", CP_Save, CP_Load};
	static saveSubsystems_t air_subsystem = {"aircraft", AIR_Save, AIR_Load};
	static saveSubsystems_t hos_subsystem = {"hospital", HOS_Save, HOS_Load};
	static saveSubsystems_t bs_subsystem = {"market", BS_Save, BS_Load};
	static saveSubsystems_t rs_subsystem = {"research", RS_Save, RS_Load};
	static saveSubsystems_t e_subsystem = {"employee", E_Save, E_Load};
	static saveSubsystems_t ac_subsystem = {"aliencont", AC_Save, AC_Load};
	static saveSubsystems_t pr_subsystem = {"production", PR_Save, PR_Load};

	saveSubsystemsAmount = 0;
	memset(&saveSubsystems, 0, sizeof(saveSubsystems));

	Com_Printf("Init saving subsystems\n");
	/* don't mess with the order */
	SAV_AddSubsystem(&b_subsystem);
	SAV_AddSubsystem(&cp_subsystem);
	SAV_AddSubsystem(&air_subsystem);
	SAV_AddSubsystem(&hos_subsystem);
	SAV_AddSubsystem(&bs_subsystem);
	SAV_AddSubsystem(&rs_subsystem);
	SAV_AddSubsystem(&e_subsystem);
	SAV_AddSubsystem(&ac_subsystem);
	SAV_AddSubsystem(&pr_subsystem);

	Cmd_AddCommand("game_save", SAV_GameSave_f, "Saves to a given filename");
	Cmd_AddCommand("game_load", SAV_GameLoad_f, "Loads a given filename");
	Cmd_AddCommand("game_comments", SAV_GameSaveNames_f, "Loads the savegame names");
	Cmd_AddCommand("game_continue", SAV_GameContinue_f, "Continue with the last saved game");
}
