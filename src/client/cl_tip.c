/**
 * @file cl_tip.c
 * @brief Tip of the day code
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "cl_tip.h"
#include "client.h"
#include "ui/ui_main.h"
#include "../shared/parse.h"

typedef struct tipOfTheDay_s {
	const char *tipString;	/**< untranslated tips string from script files */
	struct tipOfTheDay_s* next;
} tipOfTheDay_t;

static tipOfTheDay_t *tipList;	/**< linked list of all parsed tips */
static int tipCount; /**< how many tips do we have */

static cvar_t* cl_showTipOfTheDay;	/**< tip of the day can be deactivated */

/**
 * @brief Popup with tip of the day messages
 * @note Only call this from the menu definition (click action or init node)
 * because this function calls also UI_PopWindow if no tips are parsed.
 */
static void CL_GetTipOfTheDay_f (void)
{
	static int lastOne = 0;
	int rnd;
	tipOfTheDay_t* tip;

	/** @todo not his role, remove it when its possible */
	if (!tipCount) {
		UI_CloseWindow("popup_tipoftheday");
		Com_Printf("No tips parsed\n");
		return;
	}

	if (Cmd_Argc() == 2) {
		rnd = rand() % tipCount;
		lastOne = rnd;
	} else {
		lastOne = (lastOne + 1) % tipCount;
		rnd = lastOne;
	}

	tip = tipList;
	while (rnd) {
		tip = tip->next;
		rnd--;
	}

	UI_RegisterText(TEXT_TIPOFTHEDAY, tip->tipString);
}

/**
 * @brief Parse all tip definitions from the script files
 */
void CL_ParseTipsOfTheDay (const char *name, const char **text)
{
	const char *errhead = "CL_ParseTipsOfTheDay: unexpected end of file (tips ";
	const char	*token;
	tipOfTheDay_t *tip;

	/* get it's body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseTipsOfTheDay: tips without body ignored\n");
		return;
	}

	do {
		/* get the name type */
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;
		if (*token != '_') {
			Com_Printf("Ignore tip: '%s' - not marked translatable\n", token);
			continue;
		}
		tip = (tipOfTheDay_t *)Mem_PoolAlloc(sizeof(*tip), cl_genericPool, 0);
		tip->tipString = Mem_PoolStrDup(token, cl_genericPool, 0);
		tip->next = tipList;
		tipList = tip;
		tipCount++;
	} while (*text);
}

/**
 * @brief Init function for cvars and console command bindings
 */
void TOTD_InitStartup (void)
{
	cl_showTipOfTheDay = Cvar_Get("cl_showTipOfTheDay", "1", CVAR_ARCHIVE, "Show the tip of the day for singleplayer campaigns");

	/* commands */
	Cmd_AddCommand("tipoftheday", CL_GetTipOfTheDay_f, "Get the next tip of the day from the script files - called from tip of the day menu");
}
