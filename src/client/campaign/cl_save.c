/**
 * @file cl_save.c
 * @brief Implements savegames
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion team.

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
#include "../cl_global.h"
#include "../cl_game.h"
#include "../menu/m_popup.h"
#include "cl_save.h"
#include "cp_hospital.h"
#include "cl_ufo.h"
#include "cl_alienbase.h"
#include "cp_time.h"
#include "../mxml/mxml_ufoai.h"

/**
 * @brief save file header
 */
typedef struct saveFileHeader_s {
	int version; /**< which savegame version */
	int compressed; /**< is this file compressed via zlib */
	int dummy[14]; /**< maybe we have to extend this later */
	char gameVersion[16]; /**< game version that was used to save this file */
	char name[32]; /**< savefile name */
	char gameDate[32]; /**< internal game date */
	char realDate[32]; /**< real datestring when the user saved this game */
} saveFileHeader_t;

typedef struct saveFileHeaderXML_s {
	int version; /**< which savegame version */
	int compressed; /**< is this file compressed via zlib */
	int dummy[14]; /**< maybe we have to extend this later */
	char gameVersion[16]; /**< game version that was used to save this file */
	char name[32]; /**< savefile name */
	char gameDate[32]; /**< internal game date */
	char realDate[32]; /**< real datestring when the user saved this game */
	long xml_size;
} saveFileHeaderXML_t;

typedef struct saveSubsystems_s {
	const char *name;
	qboolean (*save) (sizebuf_t *sb, void *data);	/**< return false if saving failed */
	qboolean (*load) (sizebuf_t *sb, void *data);	/**< return false if loading failed */
	int check;
} saveSubsystems_t;

typedef struct saveSubsystemsXML_s {
	const char *name;
	qboolean (*save) (mxml_node_t *parent);	/**< return false if saving failed */
	qboolean (*load) (mxml_node_t *parent);	/**< return false if loading failed */
} saveSubsystemsXML_t;

static saveSubsystems_t saveSubsystems[MAX_SAVESUBSYSTEMS];
static saveSubsystemsXML_t saveSubsystemsXML[MAX_SAVESUBSYSTEMS];
static int saveSubsystemsAmount;
static int saveSubsystemsXMLAmount;
static cvar_t* save_compressed;
int presaveArray[MAX_ARRAYINDEXES];
qboolean loading = qfalse;

/**
 * @brief Fills the presaveArray with needed values and saves it.
 * @sa SAV_PresaveArrayLoad
 */
static qboolean SAV_PresaveArraySave (sizebuf_t* sb, void* data)
{
	int i;

	presaveArray[PRE_NUMODS] = csi.numODs;		/* Number of Objects in csi.ods */
	presaveArray[PRE_NUMIDS] = csi.numIDs;		/* Number of Containers */
	presaveArray[PRE_BASESI] = BASE_SIZE;		/* #define BASE_SIZE */
	presaveArray[PRE_MAXBUI] = MAX_BUILDINGS;	/* #define MAX_BUILDINGS */
	presaveArray[PRE_ACTTEA] = MAX_ACTIVETEAM;	/* #define MAX_ACTIVETEAM */
	presaveArray[PRE_MAXEMP] = MAX_EMPLOYEES;	/* #define MAX_EMPLOYEES */
	presaveArray[PRE_MCARGO] = MAX_CARGO;		/* #define MAX_CARGO */
	presaveArray[PRE_MAXAIR] = MAX_AIRCRAFT;	/* #define MAX_AIRCRAFTS */
	presaveArray[PRE_AIRSTA] = AIR_STATS_MAX;	/* AIR_STATS_MAX in aircraftParams_t */
	presaveArray[PRE_MAXCAP] = MAX_CAP;			/* MAX_CAP in baseCapacities_t */
	presaveArray[PRE_EMPTYP] = MAX_EMPL;		/* MAX_EMPL in employeeType_t */
	presaveArray[PRE_MAXBAS] = MAX_BASES;		/* #define MAX_BASES */
	presaveArray[PRE_NATION] = ccs.numNations;	/* ccs.numNations */
	presaveArray[PRE_KILLTP] = KILLED_NUM_TYPES;	/* KILLED_NUM_TYPES in killtypes_t */
	presaveArray[PRE_SKILTP] = SKILL_NUM_TYPES;	/* SKILL_NUM_TYPES in abilityskills_t */
	presaveArray[PRE_NMTECH] = ccs.numTechnologies;	/* ccs.numTechnologies */
	presaveArray[PRE_TECHMA] = TECHMAIL_MAX;	/* TECHMAIL_MAX in techMailType_t */
	presaveArray[PRE_NUMTDS] = csi.numTeamDefs;	/* numTeamDefs */
	presaveArray[PRE_NUMALI] = ccs.numAliensTD;	/* ccs.numAliensTD */
	presaveArray[PRE_NUMUFO] = gd.numUFOs;		/* gd.numUFOs */
	presaveArray[PRE_MAXREC] = MAX_RECOVERIES;	/* #define MAX_RECOVERIES */
	presaveArray[PRE_MAXTRA] = MAX_TRANSFERS;	/* #define MAX_TRANSFERS */
	presaveArray[PRE_MAXOBJ] = MAX_OBJDEFS;		/* #define MAX_OBJDEFS */
	presaveArray[PRE_MAXMPR] = MAX_MULTIPLE_PROJECTILES;/* #define MAX_MULTIPLE_PROJECTILES */
	presaveArray[PRE_MBUITY] = MAX_BUILDING_TYPE;	/* MAX_BUILDING_TYPE in buildingType_t */
	presaveArray[PRE_MAXALB] = MAX_ALIEN_BASES;		/* #define MAX_ALIEN_BASES */
	presaveArray[PRE_MAXCAT] = INTERESTCATEGORY_MAX;	/* INTERESTCATEGORY_MAX in interestCategory_t */
	presaveArray[PRE_MAXINST] = MAX_INSTALLATIONS;

	MSG_WriteByte(sb, PRE_MAX);
	for (i = 0; i < PRE_MAX; i++) {
		MSG_WriteLong(sb, presaveArray[i]);
	}
	return qtrue;
}

/**
 * @brief Loads presaveArray.
 * @sa SAV_PresaveArraySave
 */
static qboolean SAV_PresaveArrayLoad (sizebuf_t* sb, void* data)
{
	int i, cnt;

	cnt = MSG_ReadByte(sb);
	for (i = 0; i < cnt; i++) {
		presaveArray[i] = MSG_ReadLong(sb);
	}
	return qtrue;
}

/**
 * @brief Perform actions after loading a game for single player campaign
 * @sa SAV_GameLoad
 */
static qboolean SAV_GameActionsAfterLoad (char **error)
{
	RS_PostLoadInit();

	B_PostLoadInit();

	/* Make sure the date&time is displayed when loading. */
	CL_UpdateTime();

	/* Update number of UFO detected by radar */
	RADAR_SetRadarAfterLoading();

	return qtrue;
}


/**
 * @brief Currently it's only a test
 */
static qboolean SAV_GameLoadXML (const char *file, char **error)
{
	uLongf len;
	char filename[MAX_OSPATH];
	qFILE f;
	byte *cbuf, *buf;
	int res, i, clen;
	mxml_node_t *top_node, *node;
	saveFileHeaderXML_t header;

	Q_strncpyz(filename, file, sizeof(filename));

	/* open file */
	f.f = fopen(va("%s/save/%s.xml", FS_Gamedir(), filename), "rb");
	if (!f.f) {
		Com_Printf("Couldn't open file '%s'\n", filename);
		return qfalse;
	}

	clen = FS_FileLength(&f);
	cbuf = (byte *) Mem_PoolAlloc(sizeof(byte) * clen, cl_genericPool, CL_TAG_NONE);
	if (fread(cbuf, 1, clen, f.f) != clen)
		Com_Printf("Warning: Could not read %i bytes from savefile\n", clen);
	fclose(f.f);
	Com_Printf("Loading savegame xml (size %d)\n", clen);

	memcpy(&header, cbuf, sizeof(header));
	/* swap all int values if needed */
	header.compressed = LittleLong(header.compressed);
	header.version = LittleLong(header.version);
	Com_Printf("Loading savegame\n"
			"...version: %i\n"
			"...game version: %s\n"
			"...xml Size: %ld, compressed? %c\n"
			, header.version, header.gameVersion, header.xml_size, header.compressed?'y':'n');
	len = header.xml_size+50;
	buf = (byte *) Mem_PoolAlloc(sizeof(byte)*len, cl_genericPool, CL_TAG_NONE);

	if (header.compressed) {
		/* uncompress data, skipping comment header */
		res = uncompress(buf, &len, cbuf + sizeof(header), clen - sizeof(header));
		Mem_Free(cbuf);

		if (res != Z_OK) {
			Mem_Free(buf);
			*error = _("Error decompressing data");
			Com_Printf("Error decompressing data in '%s'.\n", filename);
			return qfalse;
		}
		top_node = mxmlLoadString(NULL, (char*)buf , mxml_ufo_type_cb);
		if (!top_node)
		{
			Mem_Free(buf);
			Com_Printf("Error: Failure in Loading the xml Data!");
			return qfalse;
		}
	} else {
		/*memcpy(buf, cbuf + sizeof(header), clen - sizeof(header));*/
		top_node = mxmlLoadString(NULL, (char*)(cbuf + sizeof(header)) , mxml_ufo_type_cb);
		Mem_Free(cbuf);
		if (!top_node)
		{
			Mem_Free(buf);
			Com_Printf("Error: Failure in Loading the xml Data!");
			return qfalse;
		}
	}


	/* doing a subsystem run ;) */


	GAME_RestartMode(GAME_CAMPAIGN);
	node = mxml_GetNode(top_node, "savegame");
	if (!node){
		Com_Printf("Error: Failure in Loading the xml Data! (savegame node not found)");
		Mem_Free(buf);
		return qfalse;
	}

	Com_Printf("Load '%s' %d subsystems\n", filename, saveSubsystemsXMLAmount);
	for (i = 0; i < saveSubsystemsXMLAmount; i++) {
		Com_Printf("...Running subsystem '%s'\n", saveSubsystemsXML[i].name);
		if (!saveSubsystemsXML[i].load(node)) {
			Com_Printf("...subsystem '%s' returned false - savegame could not be loaded\n", saveSubsystemsXML[i].name);
			loading = qfalse;
			return qfalse;
		} else
			Com_Printf("...subsystem '%s' - loaded.\n", saveSubsystemsXML[i].name);
	}
	mxmlDelete(node);

	SAV_GameActionsAfterLoad(error);

	loading = qfalse;

	assert(GAME_IsCampaign());

	CL_Drop();

	Com_Printf("File '%s' successfully loaded from %s xml savegame.\n", filename, header.compressed?"compressed":"");
	Mem_Free(buf);
	return qtrue;
}

/**
 * @brief Loads a savegame from file
 * @param[in] file Savegame to load (relative to writepath/save)
 * @sa SAV_GameLoad_f
 * @sa SAV_GameSave
 * @sa CP_CampaignInit
 * @sa CL_ReadSinglePlayerData
 */
static qboolean SAV_GameLoad (const char *file, char **error)
{
	uLongf len = MAX_GAMESAVESIZE;
	char filename[MAX_OSPATH];
	qFILE f;
	byte *buf, *cbuf;
	int res, clen, i, diff;
	sizebuf_t sb;
	saveFileHeader_t header;
	int check;

	Q_strncpyz(filename, file, sizeof(filename));
	if (SAV_GameLoadXML(file, error))
		return qtrue;


	/* open file */
	f.f = fopen(va("%s/save/%s.sav", FS_Gamedir(), filename), "rb");
	if (!f.f) {
		*error = _("Couldn't open file");
		Com_Printf("Couldn't open file '%s'\n", filename);
		return qfalse;
	}

	/* read compressed data into cbuf buffer */
	clen = FS_FileLength(&f);
	cbuf = (byte *) Mem_PoolAlloc(sizeof(byte) * clen, cl_genericPool, CL_TAG_NONE);
	if (fread(cbuf, 1, clen, f.f) != clen)
		Com_Printf("Warning: Could not read %i bytes from savefile\n", clen);
	fclose(f.f);

	memcpy(&header, cbuf, sizeof(header));
	/* swap all int values if needed */
	header.compressed = LittleLong(header.compressed);
	header.version = LittleLong(header.version);
	Com_Printf("Loading savegame\n"
		"...version: %i\n"
		"...game version: %s\n"
		, header.version, header.gameVersion);

	buf = (byte *) Mem_PoolAlloc(sizeof(byte) * MAX_GAMESAVESIZE, cl_genericPool, CL_TAG_NONE);
	SZ_Init(&sb, buf, MAX_GAMESAVESIZE);

	if (header.compressed) {
		/* uncompress data, skipping comment header */
		res = uncompress(buf, &len, cbuf + sizeof(header), clen - sizeof(header));
		Mem_Free(cbuf);

		if (res != Z_OK) {
			Mem_Free(buf);
			*error = _("Error decompressing data");
			Com_Printf("Error decompressing data in '%s'.\n", filename);
			return qfalse;
		}
		sb.cursize = len;
	} else {
		memcpy(buf, cbuf + sizeof(header), clen - sizeof(header));
		sb.cursize = clen - sizeof(header);
		Mem_Free(cbuf);
	}

	/* check current version */
	if (header.version > SAVE_FILE_VERSION) {
		*error = _("The file is a more recent version than is supported");
		Com_Printf("File '%s' is a more recent version (%d) than is supported.\n",
			filename, header.version);
		Mem_Free(buf);
		return qfalse;
	} else if (header.version < SAVE_FILE_VERSION) {
		Com_Printf("Savefileformat has changed ('%s' is version %d) - you may experience problems.\n",
			filename, header.version);
	}

	loading = qtrue;

	GAME_RestartMode(GAME_CAMPAIGN);

	Com_Printf("Load '%s'\n", filename);
	for (i = 0; i < saveSubsystemsAmount; i++) {
		diff = sb.readcount;
		if (!saveSubsystems[i].load(&sb, &header)) {
			*error = _("Error in loading a subsystem.\n\nSee game console for more information.");
			Com_Printf("...subsystem '%s' returned false - savegame could not be loaded\n", saveSubsystems[i].name);
			loading = qfalse;
			return qfalse;
		} else
			Com_Printf("...subsystem '%s' - loaded %i bytes\n", saveSubsystems[i].name, sb.readcount - diff);
		check = MSG_ReadByte(&sb);
		if (check != saveSubsystems[i].check) {
			*error = _("Error in loading a subsystem. Sentinel doesn't match.\n\n"
				"This might be due to an old savegame or a bug in the game.\n\n"
				"See game console for more information.");
			Com_Printf("...subsystem '%s' could not be loaded correctly - savegame might be broken (is %x, should be %x)\n",
				saveSubsystems[i].name, check, saveSubsystems[i].check);
			loading = qfalse;
			return qfalse;
		}
	}

	SAV_GameActionsAfterLoad(error);

	loading = qfalse;

	CL_Drop();

	Com_Printf("File '%s' loaded.\n", filename);
	Mem_Free(buf);

	return qtrue;
}

/**
 * @brief This is a savegame function which stores the game in xml-Format.
 * @sa SAV_GameSave
 *
 */
static qboolean SAV_GameSaveXML (const char *filename, const char *comment, char **error)
{
	mxml_node_t *top_node, *node;
	char savegame[MAX_OSPATH];
	int res;
	int requiredbuflen;
	byte *buf, *fbuf;
	uLongf bufLen;
	saveFileHeaderXML_t header;
	char dummy[2];
	int i;
	dateLong_t date;
	char message[30];

	if (!GAME_CP_IsRunning()) {
		*error = _("No campaign active.");
		Com_Printf("Error: No campaign active.\n");
		return qfalse;
	}

	if (!ccs.numBases) {
		*error = _("Nothing to save yet.");
		Com_Printf("Error: Nothing to save yet.\n");
		return qfalse;
	}

	Com_sprintf(savegame, sizeof(savegame), "%s/save/%s.xml", FS_Gamedir(), filename);


	top_node = mxmlNewXML("1.0");
	node = mxml_AddNode(top_node, "savegame");
	/* writing  Header */
	mxml_AddInt(node, "Saveversion", SAVE_FILE_VERSION);
	mxml_AddString(node, "Comment", comment);
	mxml_AddString(node, "Version", UFO_VERSION);
	CL_DateConvertLong(&ccs.date, &date);
	Com_sprintf(message, sizeof(message), _("%i %s %02i"),
		    date.year, Date_GetMonthName(date.month - 1), date.day);
	mxml_AddString(node, "GameDate", message);
	/* working through all subsystems. perhaps we should redesign it, order is not important anymore */
	Com_Printf("Calling subsystems\n");
	for (i = 0; i < saveSubsystemsXMLAmount; i++) {
		if (!saveSubsystemsXML[i].save(node))
			Com_Printf("...subsystem '%s' failed to save the data\n", saveSubsystemsXML[i].name);
		else
			Com_Printf("...subsystem '%s' - saved\n", saveSubsystemsXML[i].name);
	}

	/*calculate the needed buffer size */

	memset(&header, 0, sizeof(header));
	header.compressed = LittleLong(save_compressed->integer);
	header.version = LittleLong(SAVE_FILE_VERSION);
	Q_strncpyz(header.name, comment, sizeof(header.name));
	Q_strncpyz(header.gameVersion, UFO_VERSION, sizeof(header.gameVersion));
	CL_DateConvertLong(&ccs.date, &date);
	Com_sprintf(header.gameDate, sizeof(header.gameDate), _("%i %s %02i"),
		    date.year, Date_GetMonthName(date.month - 1), date.day);
	/** @todo fill real date string */

	requiredbuflen = mxmlSaveString(top_node, dummy, 2, MXML_NO_CALLBACK);

	/** @todo little/big endian */
	header.xml_size = requiredbuflen;
	buf = (byte *) Mem_PoolAlloc(sizeof(byte) * requiredbuflen+1, cl_genericPool, CL_TAG_NONE);
	if (!buf) {
		*error = _("Could not allocate enough memory to save this game");
		Com_Printf("Error: Could not allocate enough memory to save this game\n");
		return qfalse;
	}
	res = mxmlSaveString(top_node, (char*)buf, requiredbuflen+1, MXML_NO_CALLBACK);
	Com_Printf("XML Written to buffer (%d Bytes)\n", res);

	bufLen = (uLongf) (24 + 1.02 * requiredbuflen);
	fbuf = (byte *) Mem_PoolAlloc(sizeof(byte) * bufLen + sizeof(header), cl_genericPool, CL_TAG_NONE);
	memcpy(fbuf, &header, sizeof(header));

	if (header.compressed) {
		res = compress(fbuf + sizeof(header), &bufLen, buf, requiredbuflen+1);
		Mem_Free(buf);

		if (res != Z_OK) {
			Mem_Free(fbuf);
			*error = _("Memory error compressing save-game data - set save_compressed cvar to 0");
			Com_Printf("Memory error compressing save-game data (%s) (Error: %i)!\n", comment, res);
			return qfalse;
		}
	} else {
		memcpy(fbuf + sizeof(header), buf, requiredbuflen+1);
		Mem_Free(buf);
	}


	/* last step - write data */
	res = FS_WriteFile(fbuf, bufLen + sizeof(header), savegame);
	Mem_Free(fbuf);

	return qtrue;

}


/**
 * @brief This is the save function that calls all save-subsystems and stores
 * the data in a portable way
 * @sa SAV_GameLoad
 * @sa SAV_GameSave_f
 * @param[in] filename The filename to save your gamestate into (only the base name)
 * @param[in] comment The comment under which the savegame will be available in the
 * save/load menu
 * @param[out] error Pointer to error string in case of errors
 * @return qfalse on failure, qtrue on success
 */
static qboolean SAV_GameSave (const char *filename, const char *comment, char **error)
{
	sizebuf_t sb;
	int i, diff;
	int res;
	char savegame[MAX_OSPATH];
	byte *buf, *fbuf;
	uLongf bufLen;
	dateLong_t date;
	saveFileHeader_t header;

	if (!GAME_CP_IsRunning()) {
		*error = _("No campaign active.");
		Com_Printf("Error: No campaign active.\n");
		return qfalse;
	}

	if (!ccs.numBases) {
		*error = _("Nothing to save yet.");
		Com_Printf("Error: Nothing to save yet.\n");
		return qfalse;
	}

	/* step 1 - get the filename */
	Com_sprintf(savegame, sizeof(savegame), "%s/save/%s.sav", FS_Gamedir(), filename);

	/* step 2 - allocate the buffers */
	buf = (byte *) Mem_PoolAlloc(sizeof(byte) * MAX_GAMESAVESIZE, cl_genericPool, CL_TAG_NONE);
	if (!buf) {
		*error = _("Could not allocate enough memory to save this game");
		Com_Printf("Error: Could not allocate enough memory to save this game\n");
		return qfalse;
	}
	/* create data */
	SZ_Init(&sb, buf, MAX_GAMESAVESIZE);

	Com_Printf("Save '%s'\n", filename);
	/* step 3 - serialzer */
	for (i = 0; i < saveSubsystemsAmount; i++) {
		diff = sb.cursize;
		if (!saveSubsystems[i].save(&sb, NULL))
			Com_Printf("...subsystem '%s' failed to save the data\n", saveSubsystems[i].name);
		else
			Com_Printf("...subsystem '%s' - saved %i bytes\n", saveSubsystems[i].name, sb.cursize - diff);
		MSG_WriteByte(&sb, saveSubsystems[i].check);
	}

	/* compress data using zlib before writing */
	bufLen = (uLongf) (24 + 1.02 * sb.cursize);
	fbuf = (byte *) Mem_PoolAlloc(sizeof(byte) * bufLen + sizeof(header), cl_genericPool, CL_TAG_NONE);

	/* step 4 - write the uncompressed header */
	memset(&header, 0, sizeof(header));
	header.compressed = LittleLong(save_compressed->integer);
	header.version = LittleLong(SAVE_FILE_VERSION);
	Q_strncpyz(header.name, comment, sizeof(header.name));
	Q_strncpyz(header.gameVersion, UFO_VERSION, sizeof(header.gameVersion));
	CL_DateConvertLong(&ccs.date, &date);
	Com_sprintf(header.gameDate, sizeof(header.gameDate), _("%i %s %02i"),
		date.year, Date_GetMonthName(date.month - 1), date.day);
	/** @todo fill real date string */
	memcpy(fbuf, &header, sizeof(header));

	/* step 5 - compress */
	if (header.compressed) {
		res = compress(fbuf + sizeof(header), &bufLen, buf, sb.cursize);
		Mem_Free(buf);

		if (res != Z_OK) {
			Mem_Free(fbuf);
			*error = _("Memory error compressing save-game data - set save_compressed cvar to 0");
			Com_Printf("Memory error compressing save-game data (%s) (Error: %i)!\n", comment, res);
			return qfalse;
		}
	} else {
		memcpy(fbuf + sizeof(header), buf, sb.cursize);
		Mem_Free(buf);
	}

	/* last step - write data */
	res = FS_WriteFile(fbuf, bufLen + sizeof(header), savegame);
	Mem_Free(fbuf);

	if (res == bufLen + sizeof(header)) {
		/* set cl_lastsave to let the continue function know which game to
		 * automatically continue */
		/** @todo redo this in the menu */
		Cvar_Set("cl_lastsave", filename);
		Com_Printf("Campaign '%s' saved.\n", comment);
		return qtrue;
	} else {
		Com_Printf("Failed to save campaign '%s' !!!\n", comment);
		*error = _("Size mismatch - failed to save the campaign");
		return qfalse;
	}
}

/**
 * @brief Console command binding for save function
 * @sa SAV_GameSave
 */
static void SAV_GameSave_f (void)
{
	char comment[MAX_COMMENTLENGTH] = "";
	char *error = NULL;
	const char *arg = NULL;
	qboolean result;

	/* get argument */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <filename> <comment|*cvar>\n", Cmd_Argv(0));
		return;
	}

	if (!GAME_CP_IsRunning()) {
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
	result = SAV_GameSave(Cmd_Argv(1), comment, &error);

	/* if save failed refresh the SaveGame menu and popup error message */
	if (!result) {
		if (error)
			Com_sprintf(popupText, sizeof(popupText), "%s\n%s", _("Error saving game."), error);
		else
			Com_sprintf(popupText, sizeof(popupText), "%s\n", _("Error saving game."));
		MN_Popup(_("Note"), popupText);
	}
	result = SAV_GameSaveXML(Cmd_Argv(1), comment, &error);
	if (!result) {
		if (error)
			Com_sprintf(popupText, sizeof(popupText), "%s\n%s", _("Error saving game."), error);
		else
			Com_sprintf(popupText, sizeof(popupText), "%s\n", _("Error saving game."));
		MN_Popup(_("Note"), popupText);
	}
}

/**
 * @brief Console commands to read the comments from savegames
 * @note The comment is the part of the savegame header that you type in at saving
 * for reidentifying the savegame
 * @sa SAV_GameLoad_f
 * @sa SAV_GameLoad
 * @sa SAV_GameSaveNameCleanup_f
 */
static void SAV_GameReadGameComments_f (void)
{
	int i;

	if (Cmd_Argc() == 2) {
		/* checks whether we plan to save without a running game */
		if (!GAME_CP_IsRunning() && !Q_strncmp(Cmd_Argv(1), "save", 4)) {
			MN_PopMenu(qfalse);
			return;
		}
	}

	for (i = 0; i < 8; i++) {
		char comment[MAX_VAR];
		saveFileHeader_t header;
		saveFileHeaderXML_t headerXML;
		qboolean error=qfalse;
		/* open file */
		FILE *f;
		f = fopen(va("%s/save/slot%i.xml", FS_Gamedir(), i), "rb");
		if (!f) {
			error=qtrue;
		}else{
			if (fread(&headerXML, 1, sizeof(headerXML), f) != sizeof(headerXML))
				Com_Printf("Warning: SaveXMLfile header may be corrupted\n");
			Com_sprintf(comment, sizeof(comment), "%s - %s", headerXML.name, headerXML.gameDate);
			Cvar_Set(va("mn_slot%i", i), comment);
			fclose(f);
		}

		/*If we have an error, we try to read the information from the _old_ savegame style */
		if (error){
			f = fopen(va("%s/save/slot%i.sav", FS_Gamedir(), i), "rb");
			if (!f) {
				Cvar_Set(va("mn_slot%i", i), "");
				continue;
			}

			/* read the comment */
			if (fread(&header, 1, sizeof(header), f) != sizeof(header))
				Com_Printf("Warning: Savefile header may be corrupted\n");
			Com_sprintf(comment, sizeof(comment), "%s - %s", header.name, header.gameDate);
			Cvar_Set(va("mn_slot%i", i), comment);
			fclose(f);
		}
	}
}

/**
 * @brief Console command to load a savegame
 * @sa SAV_GameLoad
 */
static void SAV_GameLoad_f (void)
{
	char *error = NULL;
	const cvar_t *gamedesc;

	/* get argument */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <filename>\n", Cmd_Argv(0));
		return;
	}

	/* try to get game description from "mn_<filename>" as indicator whether file will exist */
	gamedesc = Cvar_FindVar(va("mn_%s", Cmd_Argv(1)));
	if (!gamedesc || gamedesc->string[0] == '\0') {
		Com_DPrintf(DEBUG_CLIENT, "don't load file '%s', there is no description for it\n", Cmd_Argv(1));
		return;
	}

	Com_DPrintf(DEBUG_CLIENT, "load file '%s'\n", Cmd_Argv(1));

	/* load and go to map */
	if (!SAV_GameLoad(Cmd_Argv(1), &error)) {
		Cbuf_Execute(); /* wipe outstanding campaign commands */
		Com_sprintf(popupText, sizeof(popupText), "%s\n%s", _("Error loading game."), error ? error : "");
		MN_Popup(_("Error"), popupText);
	}
}

/**
 * @brief Loads the last saved game
 * @note At saving the archive cvar cl_lastsave was set to the latest savegame
 * @sa SAV_GameLoad
 */
static void SAV_GameContinue_f (void)
{
	char *error = NULL;

	if (cls.state == ca_active) {
		MN_PopMenu(qfalse);
		return;
	}

	if (!GAME_CP_IsRunning()) {
		/* try to load the last saved campaign */
		if (!SAV_GameLoad(cl_lastsave->string, &error)) {
			Cbuf_Execute(); /* wipe outstanding campaign commands */
			Com_sprintf(popupText, sizeof(popupText), "%s\n%s", _("Error loading game."), error ? error : "");
			MN_Popup(_("Error"), popupText);
		}
	} else {
		/* just continue the current running game */
		MN_PopMenu(qfalse);
	}
}

/**
 * @brief Adds a subsystem to the saveSubsystems array
 * @note The order matters - don't change it or all previous savegames will be broken
 * @sa SAV_Init
 */
static qboolean SAV_AddSubsystem (saveSubsystems_t *subsystem)
{
	if (saveSubsystemsAmount >= MAX_SAVESUBSYSTEMS)
		return qfalse;

	saveSubsystems[saveSubsystemsAmount].name = subsystem->name;
	saveSubsystems[saveSubsystemsAmount].load = subsystem->load;
	saveSubsystems[saveSubsystemsAmount].save = subsystem->save;
	saveSubsystems[saveSubsystemsAmount].check = subsystem->check;
	saveSubsystemsAmount++;

	Com_Printf("added %s subsystem (check %x)\n", subsystem->name, subsystem->check);
	return qtrue;
}

/**
 * @brief Adds a subsystem to the saveSubsystems array
 * @note The order is _not_ important
 * @sa SAV_Init
 */
static qboolean SAV_AddSubsystemXML (saveSubsystemsXML_t *subsystem)
{
	if (saveSubsystemsXMLAmount >= MAX_SAVESUBSYSTEMS)
		return qfalse;

	saveSubsystemsXML[saveSubsystemsXMLAmount].name = subsystem->name;
	saveSubsystemsXML[saveSubsystemsXMLAmount].load = subsystem->load;
	saveSubsystemsXML[saveSubsystemsXMLAmount].save = subsystem->save;
	saveSubsystemsXMLAmount++;

	Com_Printf("added %s subsystem\n", subsystem->name);
	return qtrue;
}

/**
 * @brief Set the mn_slotX cvar to the comment (remove the date string) for clean
 * editing of the save comment
 * @sa SAV_GameReadGameComments_f
 */
static void SAV_GameSaveNameCleanup_f (void)
{
	int slotID;
	char cvar[16];
	FILE *f;
	saveFileHeader_t header;

	/* get argument */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <[0-7]>\n", Cmd_Argv(0));
		return;
	}

	slotID = atoi(Cmd_Argv(1));
	if (slotID < 0 || slotID > 7)
		return;

	Com_sprintf(cvar, sizeof(cvar), "mn_slot%i", slotID);

	f = fopen(va("%s/save/slot%i.sav", FS_Gamedir(), slotID), "rb");
	if (!f)
		return;

	/* read the comment */
	if (fread(&header, 1, sizeof(header), f) != sizeof(header))
		Com_Printf("Warning: Savefile header may be corrupted\n");
	Cvar_Set(cvar, header.name);
	fclose(f);
}

/**
 * @brief Quick save the current campaign
 * @sa CP_StartMissionMap
 */
qboolean SAV_QuickSave (void)
{
	char *error = NULL;
	qboolean result;

	assert(curCampaign);

	if (CL_OnBattlescape())
		return qfalse;

	result = SAV_GameSave("slotquick", "QuickSave", &error);
	if (!result)
		Com_Printf("Error saving the game: %s\n", error ? error : "");

	return qtrue;
}

/**
 * @brief Checks whether there is a quicksave file at all - otherwise close
 * the quickload menu
 */
static void SAV_GameQuickLoadInit_f (void)
{
	FILE *f;

	/* only allow quickload while there is already a running campaign */
	if (!GAME_CP_IsRunning()) {
		MN_PopMenu(qfalse);
		return;
	}

	f = fopen(va("%s/save/slotquick.sav", FS_Gamedir()), "rb");
	if (!f)
		MN_PopMenu(qfalse);
	else
		fclose(f);
}

/**
 * @brief Saves to the quick save slot
 * @sa SAV_GameQuickLoad_f
 */
static void SAV_GameQuickSave_f (void)
{
	if (!GAME_CP_IsRunning())
		return;

	if (!SAV_QuickSave())
		Com_Printf("Could not save the campaign\n");
	else
		MS_AddNewMessage(_("Quicksave"), _("Campaign was successfully saved."), qfalse, MSG_INFO, NULL);
}

/**
 * @brief Loads the quick save slot
 * @sa SAV_GameQuickSave_f
 */
static void SAV_GameQuickLoad_f (void)
{
	char *error = NULL;

	if (!GAME_CP_IsRunning())
		return;

	if (CL_OnBattlescape()) {
		Com_Printf("Could not load the campaign while you are on the battlefield\n");
		return;
	}

	if (!SAV_GameLoad("slotquick", &error)) {
		Cbuf_Execute(); /* wipe outstanding campaign commands */
		Com_sprintf(popupText, sizeof(popupText), "%s\n%s", _("Error loading game."), error ? error : "");
		MN_Popup(_("Error"), popupText);
	} else {
		MN_Popup(_("Campaign loaded"), _("Quicksave campaign was successfully loaded."));
	}
}

/**
 * @brief Register all save-subsystems and init some cvars and commands
 * @sa SAV_GameSave
 * @sa SAV_GameLoad
 */
void SAV_Init (void)
{
	static saveSubsystems_t pre_subsystem = {"size", SAV_PresaveArraySave, SAV_PresaveArrayLoad, 0xFF};
	static saveSubsystems_t b_subsystem = {"base", B_Save, B_Load, 0x0};
	static saveSubsystems_t cp_subsystem = {"campaign", CP_Save, CP_Load, 0x1};
	static saveSubsystems_t hos_subsystem = {"hospital", HOS_Save, HOS_Load, 0x2};
	static saveSubsystems_t bs_subsystem = {"market", BS_Save, BS_Load, 0x3};
	static saveSubsystems_t rs_subsystem = {"research", RS_Save, RS_Load, 0x4};
	static saveSubsystems_t e_subsystem = {"employee", E_Save, E_Load, 0x5};
	static saveSubsystems_t ac_subsystem = {"aliencont", AC_Save, AC_Load, 0x6};
	static saveSubsystems_t pr_subsystem = {"production", PR_Save, PR_Load, 0x7};
	static saveSubsystems_t air_subsystem = {"aircraft", AIR_Save, AIR_Load, 0x8};
	static saveSubsystems_t ms_subsystem = {"messagesystem", MS_Save, MS_Load, 0x9};
	static saveSubsystems_t stats_subsystem = {"stats", STATS_Save, STATS_Load, 0xA};
	static saveSubsystems_t na_subsystem = {"nations", NA_Save, NA_Load, 0xB};
	static saveSubsystems_t trans_subsystem = {"transfer", TR_Save, TR_Load, 0xC};
	static saveSubsystems_t ab_subsystem = {"alien base", AB_Save, AB_Load, 0xD};
	static saveSubsystems_t xvi_subsystem = {"xvirate", XVI_Save, XVI_Load, 0xE};
	static saveSubsystems_t ins_subsystem = {"installation", INS_Save, INS_Load, 0x0};
	static saveSubsystems_t mso_subsystem = {"messageoptions", MSO_Save, MSO_Load, 0x1};

	/*static saveSubsystemsXML_t pre_subsystemXML = {"size", SAV_PresaveArraySaveXML, SAV_PresaveArrayLoadXML, 0xFF};**/
	static saveSubsystemsXML_t b_subsystemXML = {"base", B_SaveXML, B_LoadXML};
	static saveSubsystemsXML_t cp_subsystemXML = {"campaign", CP_SaveXML, CP_LoadXML};
	static saveSubsystemsXML_t hos_subsystemXML = {"hospital", HOS_SaveXML, HOS_LoadXML};
	static saveSubsystemsXML_t bs_subsystemXML = {"market", BS_SaveXML, BS_LoadXML};
	static saveSubsystemsXML_t rs_subsystemXML = {"research", RS_SaveXML, RS_LoadXML};
	static saveSubsystemsXML_t e_subsystemXML = {"employee", E_SaveXML, E_LoadXML};
	static saveSubsystemsXML_t ac_subsystemXML = {"aliencont", AC_SaveXML, AC_LoadXML};
	static saveSubsystemsXML_t pr_subsystemXML = {"production", PR_SaveXML, PR_LoadXML};
	static saveSubsystemsXML_t air_subsystemXML = {"aircraft", AIR_SaveXML, AIR_LoadXML};
	static saveSubsystemsXML_t ms_subsystemXML = {"messagesystem", MS_SaveXML, MS_LoadXML};
	static saveSubsystemsXML_t stats_subsystemXML = {"stats", STATS_SaveXML, STATS_LoadXML};
	static saveSubsystemsXML_t na_subsystemXML = {"nations", NAT_SaveXML, NAT_LoadXML};
	static saveSubsystemsXML_t trans_subsystemXML = {"transfer", TR_SaveXML, TR_LoadXML};
	static saveSubsystemsXML_t ab_subsystemXML = {"alien base", AB_SaveXML, AB_LoadXML};
	static saveSubsystemsXML_t xvi_subsystemXML = {"xvirate", XVI_SaveXML, XVI_LoadXML};
	static saveSubsystemsXML_t ins_subsystemXML = {"installation", INS_SaveXML, INS_LoadXML};
	static saveSubsystemsXML_t mso_subsystemXML = {"messageoptions", MSO_SaveXML, MSO_LoadXML};

	saveSubsystemsAmount = 0;
	memset(&saveSubsystems, 0, sizeof(saveSubsystems));
	saveSubsystemsXMLAmount = 0;
	memset(&saveSubsystemsXML, 0, sizeof(saveSubsystemsXML));

	Com_Printf("\n--- save subsystem initialization --\n");

	/* don't mess with the order */
	SAV_AddSubsystem(&pre_subsystem);
	SAV_AddSubsystem(&b_subsystem);
	SAV_AddSubsystem(&cp_subsystem);
	SAV_AddSubsystem(&hos_subsystem);
	SAV_AddSubsystem(&bs_subsystem);
	SAV_AddSubsystem(&rs_subsystem);
	SAV_AddSubsystem(&e_subsystem);
	SAV_AddSubsystem(&ac_subsystem);
	SAV_AddSubsystem(&pr_subsystem);
	SAV_AddSubsystem(&air_subsystem);
	SAV_AddSubsystem(&ms_subsystem);
	SAV_AddSubsystem(&stats_subsystem);
	SAV_AddSubsystem(&na_subsystem);
	SAV_AddSubsystem(&trans_subsystem);
	SAV_AddSubsystem(&ab_subsystem);
	SAV_AddSubsystem(&xvi_subsystem);
	SAV_AddSubsystem(&ins_subsystem);
	SAV_AddSubsystem(&mso_subsystem);

	SAV_AddSubsystemXML(&b_subsystemXML);
	SAV_AddSubsystemXML(&cp_subsystemXML);
	SAV_AddSubsystemXML(&hos_subsystemXML);
	SAV_AddSubsystemXML(&bs_subsystemXML);
	SAV_AddSubsystemXML(&rs_subsystemXML);
	SAV_AddSubsystemXML(&e_subsystemXML);
	SAV_AddSubsystemXML(&ac_subsystemXML);
	SAV_AddSubsystemXML(&pr_subsystemXML);
	SAV_AddSubsystemXML(&air_subsystemXML);
	SAV_AddSubsystemXML(&ms_subsystemXML);
	SAV_AddSubsystemXML(&stats_subsystemXML);
	SAV_AddSubsystemXML(&na_subsystemXML);
	SAV_AddSubsystemXML(&trans_subsystemXML);
	SAV_AddSubsystemXML(&ab_subsystemXML);
	SAV_AddSubsystemXML(&xvi_subsystemXML);
	SAV_AddSubsystemXML(&ins_subsystemXML);
	SAV_AddSubsystemXML(&mso_subsystemXML);

	Cmd_AddCommand("game_quickloadinit", SAV_GameQuickLoadInit_f, "Check whether there is a quicksave at all");
	Cmd_AddCommand("game_quicksave", SAV_GameQuickSave_f, _("Saves to the quick save slot"));
	Cmd_AddCommand("game_quickload", SAV_GameQuickLoad_f, "Loads the quick save slot");
	Cmd_AddCommand("game_save", SAV_GameSave_f, "Saves to a given filename");
	Cmd_AddCommand("game_load", SAV_GameLoad_f, "Loads a given filename");
	Cmd_AddCommand("game_comments", SAV_GameReadGameComments_f, "Loads the savegame names");
	Cmd_AddCommand("game_continue", SAV_GameContinue_f, "Continue with the last saved game");
	Cmd_AddCommand("game_savenamecleanup", SAV_GameSaveNameCleanup_f, "Remove the date string from mn_slotX cvars");
	save_compressed = Cvar_Get("save_compressed", "1", CVAR_ARCHIVE, "Save the savefiles compressed if set to 1");
}
