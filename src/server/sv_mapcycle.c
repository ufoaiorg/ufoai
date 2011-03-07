#include "server.h"
#include "../shared/parse.h"

/**
 * @brief map cycle list element
*/
typedef struct mapcycle_s {
	char *map;			/**< map name */
	char *type;			/**< gametype to play on this map */
	qboolean day;		/**< load the day version */
	struct mapcycle_s* next;	/**< pointer to the next map in cycle */
} mapcycle_t;

static mapcycle_t *mapcycleList;	/**< map cycle linked list */
static int mapcycleCount;		/**< number of maps in the cycle */

/**
 * @brief Start the next map in the cycle
 */
void SV_NextMapcycle (void)
{
	const char *map = NULL, *gameType = NULL;
	qboolean day = qtrue;
	char *base;
	char assembly[MAX_QPATH];
	char expanded[MAX_QPATH];
	char cmd[MAX_QPATH];
	mapcycle_t *mapcycle;

	mapcycle = mapcycleList;
	if (sv->name[0]) {
		int i;
		Com_Printf("current map: %s\n", sv->name);
		for (i = 0; i < mapcycleCount; i++) {
			/* random maps may have a theme - but that's not stored in sv->name
			 * but in sv->assembly */
			if (mapcycle->map[0] == '+') {
				Q_strncpyz(expanded, mapcycle->map, sizeof(expanded));
				base = strstr(expanded, " ");
				if (base) {
					base[0] = '\0'; /* split the strings */
					Q_strncpyz(assembly, base + 1, sizeof(assembly));
					/* get current position */
					if (Q_streq(sv->name, expanded) && Q_streq(sv->assembly, assembly)) {
						/* next map in cycle */
						if (mapcycle->next) {
							map = mapcycle->next->map;
							day = mapcycle->next->day;
							gameType = mapcycle->next->type;
							Com_DPrintf(DEBUG_SERVER, "SV_NextMapcycle: next one: '%s' (gametype: %s)\n", map, gameType);
						/* switch back to first list on cycle - if there is one */
						} else {
							map = mapcycleList->map;
							day = mapcycleList->day;
							gameType = mapcycleList->type;
							Com_DPrintf(DEBUG_SERVER, "SV_NextMapcycle: first one: '%s' (gametype: %s)\n", map, gameType);
						}
						break;
					}
				} else {
					Com_Printf("ignore mapcycle entry for random map (%s) with"
						" no assembly given\n", mapcycle->map);
				}
			} else {
				/* get current position */
				if (Q_streq(sv->name, mapcycle->map)) {
					/* next map in cycle */
					if (mapcycle->next) {
						map = mapcycle->next->map;
						day = mapcycle->next->day;
						gameType = mapcycle->next->type;
						Com_DPrintf(DEBUG_SERVER, "SV_NextMapcycle: next one: '%s' (gametype: %s)\n", map, gameType);
					/* switch back to first list on cycle - if there is one */
					} else {
						map = mapcycleList->map;
						day = mapcycleList->day;
						gameType = mapcycleList->type;
						Com_DPrintf(DEBUG_SERVER, "SV_NextMapcycle: first one: '%s' (gametype: %s)\n", map, gameType);
					}
					Com_sprintf(expanded, sizeof(expanded), "maps/%s.bsp", map);

					/* check for bsp file */
					if (map[0] != '+' && FS_CheckFile("%s", expanded) < 0) {
						Com_Printf("SV_NextMapcycle: Can't find '%s' - mapcycle error\n"
							"Use the 'maplist' command to get a list of valid maps\n", expanded);
						map = NULL;
						gameType = NULL;
					} else
						break;
				}
			}
			mapcycle = mapcycle->next;
		}
	}

	if (!map) {
		if (mapcycleCount > 0) {
			map = mapcycleList->map;
			day = mapcycleList->day;
			gameType = mapcycleList->type;
			if (map[0] != '+') {
				Com_sprintf(expanded, sizeof(expanded), "maps/%s.bsp", map);

				/* check for bsp file */
				if (FS_CheckFile("%s", expanded) < 0) {
					Com_Printf("SV_NextMapcycle: Can't find '%s' - mapcycle error\n"
						"Use the 'maplist' command to get a list of valid maps\n", expanded);
					return;
				}
			}
		} else if (sv->name[0]) {
			Com_Printf("No mapcycle - restart the current map (%s)\n", sv->name);
			map = sv->name;
			gameType = NULL;
		} else {
			Com_Printf("No mapcycle and no running map\n");
			return;
		}
		/* still not set? */
		if (!map)
			return;
	}

	/* check whether we want to change the gametype, too */
	if (gameType && gameType[0] != '\0') {
		Cvar_Set("sv_gametype", gameType);
		Com_SetGameType();
		sv_gametype->modified = qfalse;
	}

	if (day)
		Com_sprintf(cmd, sizeof(cmd), "map day %s", map);
	else
		Com_sprintf(cmd, sizeof(cmd), "map night %s", map);
	Cbuf_AddText(cmd);
}

/**
 * @brief Empty the mapcycle list
 * @sa SV_MapcycleAdd
 */
void SV_MapcycleClear (void)
{
	int i;
	mapcycle_t *mapcycle;

	mapcycle = mapcycleList;
	for (i = 0; i < mapcycleCount; i++) {
		mapcycle_t *oldMapcycle = mapcycle;
		mapcycle = mapcycle->next;
		Mem_Free(oldMapcycle->type);
		Mem_Free(oldMapcycle->map);
		Mem_Free(oldMapcycle);
	}

	/* reset the mapcycle data */
	mapcycleList = NULL;
	mapcycleCount = 0;
}

/**
 * @brief Append a new mapname to the list of maps for the cycle
 * @todo check for maps and valid gametypes here
 * @sa SV_MapcycleClear
 */
static void SV_MapcycleAdd (const char* mapName, qboolean day, const char* gameType)
{
	mapcycle_t *mapcycle;

	if (!mapcycleList) {
		mapcycleList = (mapcycle_t *)Mem_PoolAlloc(sizeof(*mapcycle), sv_genericPool, 0);
		mapcycle = mapcycleList; /* first one */
	} else {
		/* go to the last entry */
		mapcycle = mapcycleList;
		while (mapcycle->next)
			mapcycle = mapcycle->next;
		mapcycle->next = (mapcycle_t *)Mem_PoolAlloc(sizeof(*mapcycle), sv_genericPool, 0);
		mapcycle = mapcycle->next;
	}
	mapcycle->map = Mem_PoolStrDup(mapName, sv_genericPool, 0);
	mapcycle->day = day;
	mapcycle->type = Mem_PoolStrDup(gameType, sv_genericPool, 0);
	Com_DPrintf(DEBUG_SERVER, "mapcycle add: '%s' type '%s'\n", mapcycle->map, mapcycle->type);
	mapcycle->next = NULL;
	mapcycleCount++;
}

/**
 * @brief Parses the server mapcycle
 * @sa SV_MapcycleAdd
 * @sa SV_MapcycleClear
 */
static void SV_ParseMapcycle (void)
{
	int length = 0;
	byte *buffer = NULL;
	const char *token;
	const char *buf;
	char map[MAX_VAR], gameType[MAX_VAR];

	mapcycleCount = 0;
	mapcycleList = NULL;

	length = FS_LoadFile("mapcycle.txt", &buffer);
	if (length == -1 || !buffer)
		return;

	if (length != -1) {
		buf = (const char*)buffer;
		do {
			qboolean day = qfalse;
			/* parse map name */
			token = Com_Parse(&buf);
			if (!buf)
				break;
			Q_strncpyz(map, token, sizeof(map));
			/* parse day or night */
			token = Com_Parse(&buf);
			if (!buf)
				break;
			if (Q_streq(token, "day"))
				day = qtrue;
			else if (!Q_streq(token, "night")) {
				Com_Printf("Skip mapcycle parsing, expected day or night.");
				break;
			}
			/* parse gametype */
			token = Com_Parse(&buf);
			if (!buf)
				break;
			Q_strncpyz(gameType, token, sizeof(gameType));
			SV_MapcycleAdd(map, day, gameType);
		} while (buf);

		Com_Printf("added %i maps to the mapcycle\n", mapcycleCount);
	}
	FS_FreeFile(buffer);
}

static void SV_MapcycleList_f (void)
{
	int i;
	const mapcycle_t* mapcycle;

	mapcycle = mapcycleList;
	Com_Printf("current mapcycle has %i entries\n", mapcycleCount);
	for (i = 0; i < mapcycleCount; i++) {
		Com_Printf(" %s (%s)\n", mapcycle->map, mapcycle->type);
		mapcycle = mapcycle->next;
	}
}

static void SV_MapcycleAdd_f (void)
{
	if (Cmd_Argc() == 4) {
		const char *map = Cmd_Argv(1);
		const char *day = Cmd_Argv(2);
		const char *gametype = Cmd_Argv(3);
		if (!SV_CheckMap(map, NULL)) {
			Com_Printf("map '%s' isn't a valid map\n", map);
			return;
		}
		Com_Printf("adding map '%s' with gametype '%s' to mapcycle (to add this permanently edit your mapcycle.txt)\n", map, gametype);
		if (Q_streq(day, "day"))
			SV_MapcycleAdd(map, qtrue, gametype);
		else
			SV_MapcycleAdd(map, qfalse, gametype);
	} else {
		Com_Printf("Usage: %s <mapname> <day|night> <gametype>\n", Cmd_Argv(0));
		Com_Printf(" ...to get a list of valid maps type 'maplist'\n"
			" ...to get a list of valid gametypes 'gametypelist'\n");
	}
}

static void SV_MapcycleNext_f (void)
{
	if (mapcycleCount > 0)
		SV_NextMapcycle();
	else
		Com_Printf("no mapcycle.txt\n");
}

void SV_MapcycleInit (void)
{
	SV_ParseMapcycle();

	Cmd_AddCommand("mapcyclelist", SV_MapcycleList_f, "Print the current mapcycle");
	Cmd_AddCommand("mapcyclenext", SV_MapcycleNext_f, "Start the next map from the cycle");
	Cmd_AddCommand("mapcycleclear", SV_MapcycleClear, "Delete the current mapcycle");
	Cmd_AddCommand("mapcycleadd", SV_MapcycleAdd_f, "Add new maps to the mapcycle");
}
