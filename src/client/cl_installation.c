/**
 * @file cl_installation.c
 * @brief Handles everything that is located in or accessed through an installation.
 * @note Installation functions prefix: INS_*
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
#include "cl_team.h"
#include "cl_mapfightequip.h"
#include "cl_aircraft.h"
#include "cl_hospital.h"
#include "cl_view.h"
#include "cl_map.h"
#include "cl_ufo.h"
#include "cl_popup.h"
#include "../renderer/r_draw.h"
#include "menu/m_nodes.h"
#include "menu/m_popup.h"
#include "cl_installation.h"

void R_CreateRadarOverlay(void);

vec3_t newInstallationPos;
static cvar_t *mn_installation_title;
static cvar_t *mn_installation_count;
static cvar_t *mn_installation_id;

/**
 * @brief Array bound check for the installation index.
 * @param[in] instIdx  Instalation's index
 * @return Pointer to the installation corresponding to instIdx.
 */
installation_t* INS_GetInstallationByIDX (int instIdx)
{
	assert(instIdx < MAX_INSTALLATIONS);
	assert(instIdx >= 0);
	return &gd.installations[instIdx];
}

/**
 * @brief Array bound check for the installation index.
 * @param[in] instIdx  Instalation's index
 * @return Pointer to the installation corresponding to instIdx if installation is founded, NULL else.
 */
installation_t* INS_GetFoundedInstallationByIDX (int instIdx)
{
	installation_t *installation = INS_GetInstallationByIDX(instIdx);

	if (installation->founded)
		return installation;

	return NULL;
}

/**
 * @brief Returns the installation radar range (if set) for a given installation type.
 * @param[in] installationType The type of the installation to find the radar range of.
 * @return The radar range of the installation if the installation is found and has had the range set.  Otherwise it will return -1.
 */
static installationTemplate_t* INS_GetInstallationTemplateFromInstallationId (const char *id)
{
	int idx;

	for (idx = 0; idx < gd.numInstallationTemplates; idx++) {
		if (Q_strncmp(gd.installationTemplates[idx].id, id, MAX_VAR) == 0) {
			return &gd.installationTemplates[idx];
		}
	}

	return NULL;
}


/**
 * @brief Setup new installation
 * @sa CL_NewInstallation
 */
void INS_SetUpInstallation (installation_t* installation, installationTemplate_t *installationTemplate)
{
	const int newInstallationAlienInterest = 1.0f;
	int idxBattery;

	assert(installation);

	installation->idx = gd.numInstallations - 1;
	installation->founded = qtrue;
	installation->installationStatus = INSTALLATION_WORKING;
	installation->installationTemplate = installationTemplate;

	/* Reset current capacities. */
	installation->aircraftCapacitiy.cur = 0;

	/* this cvar is used for disabling the installation build button on geoscape if MAX_INSTALLATIONS was reached */
	Cvar_Set("mn_installation_count", va("%i", gd.numInstallations));

	/* this cvar is needed by INS_SetBuildingByClick below*/
	Cvar_SetValue("mn_installation_id", installation->idx);

	installation->numAircraftInInstallation = 0;

	/* a new installation is not discovered (yet) */
	installation->alienInterest = newInstallationAlienInterest;

	/* intialise hit points */
	installation->installationDamage = MAX_INSTALLATION_DAMAGE;

	Q_strncpyz (&installation->storage.name[16], "base_AA51_launcher", sizeof(installation->storage.name[16]));
	installation->storage.num[16] = 3;

	Com_Printf("id = %s range = %f batteries = %i ufo's = %i", installation->installationTemplate->id, installation->installationTemplate->radarRange, installation->installationTemplate->numMaxBatteries, installation->installationTemplate->numMaxUfoStored);

	/* Reset Radar range */
	RADAR_Initialise(&(installation->radar), 0.0f, 1.0f, qtrue);
	RADAR_UpdateInstallationRadarCoverage_f(installation, installation->installationTemplate->radarRange);

	for (idxBattery = 0; idxBattery < installation->installationTemplate->numMaxBatteries; idxBattery++) {
		AII_InitialiseSlot(&installation->batteries[idxBattery].slot, NULL, NULL, installation, AC_ITEM_BASE_MISSILE);
		installation->batteries[idxBattery].target = NULL;

	}
}

/**
 * @brief Renames an installation.
 */
static void INS_RenameInstallation_f (void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <name>\n", Cmd_Argv(0));
		return;
	}

	if (installationCurrent)
		Q_strncpyz(installationCurrent->name, Cmd_Argv(1), sizeof(installationCurrent->name));
}

/**
 * @brief Get the lower IDX of unfounded installation.
 * @return instIdx of first Installation Unfounded, or MAX_INSTALLATIONS is maximum installation number is reached.
 */
static int INS_GetFirstUnfoundedInstallation (void)
{
	int instIdx;

	for (instIdx = 0; instIdx < MAX_INSTALLATIONS; instIdx++) {
		const installation_t const *installation = INS_GetFoundedInstallationByIDX(instIdx);
		if (!installation)
			return instIdx;
	}

	return MAX_INSTALLATIONS;
}
/**
 * @param[in] installation If this is @c NULL we want to installation a new base
 */
void INS_SelectInstallation (installation_t *installation)
{
	/* set up a new installation */
	if (!installation) {
		int installationID;

		/* if player hit the "create base" button while creating base mode is enabled
		 * that means that player wants to quit this mode */
		if (gd.mapAction == MA_NEWINSTALLATION) {
			MAP_ResetAction();
			if (!radarOverlayWasSet)
				MAP_DeactivateOverlay("radar");
			return;
		}

		gd.mapAction = MA_NEWINSTALLATION;
		installationID = INS_GetFirstUnfoundedInstallation();
		Com_DPrintf(DEBUG_CLIENT, "INS_SelectInstallation_f: new installationID is %i\n", installationID);
		if (installationID < MAX_INSTALLATIONS) {
			installationCurrent = INS_GetInstallationByIDX(installationID);
			installationCurrent->idx = installationID;
			Com_DPrintf(DEBUG_CLIENT, "B_SelectBase_f: baseID is valid for base: %s\n", installationCurrent->name);
			/* Store configuration of radar overlay to be able to set it back */
			radarOverlayWasSet = (r_geoscape_overlay->integer & OVERLAY_RADAR);
			/* show radar overlay (if not already displayed) */
			if (!radarOverlayWasSet)
				MAP_SetOverlay("radar");
		} else {
			Com_Printf("MaxInstallations reached\n");
			/* select the first installation in list */
			installationCurrent = INS_GetInstallationByIDX(0);
			gd.mapAction = MA_NONE;
		}
	} else {
		Com_DPrintf(DEBUG_CLIENT, "INS_SelectInstallation_f: select installation with id %i\n", installation->idx);
		installationCurrent = installation;
		baseCurrent = NULL;
		gd.mapAction = MA_NONE;
		if (installation->installationTemplate->numMaxBatteries > 0)
			MN_PushMenu("basedefence");
		else if (installation->numAircraftInInstallation > 0)
			MN_PushMenu("ufoyard");
	}

}

/**
 * @brief Called when a base is opened or a new base is created on geoscape.
 * For a new base the baseID is -1.
 */
static void INS_SelectInstallation_f (void)
{
	int installationID;
	installation_t *installation;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <installationID>\n", Cmd_Argv(0));
		return;
	}
	installationID = atoi(Cmd_Argv(1));

	if (installationID >= 0 && installationID < gd.numInstallations)
		installation = INS_GetFoundedInstallationByIDX(installationID);
	else
		/* create a new base */
		installation = NULL;
	INS_SelectInstallation(installation);
}


/**
 * @brief Constructs a new installation.
 * @sa CL_NewInstallation
 */
static void INS_BuildInstallation_f (void)
{
	const nation_t *nation;
	installationTemplate_t *installationTemplate;

	if (Cmd_Argc() < 1) {
		Com_Printf("Usage: %s <installationType>\n", Cmd_Argv(0));
		return;
	}

	installationTemplate = INS_GetInstallationTemplateFromInstallationId(Cmd_Argv(1));

	if (!installationTemplate) {
		Com_Printf("The installation type %s passed for %s is not valid.\n", Cmd_Argv(1), Cmd_Argv(0));
		return;
	}

	if (!installationCurrent)
		return;

	assert(!installationCurrent->founded);
	assert(ccs.singleplayer);
	assert(curCampaign);
	assert(installationTemplate->cost >= 0);

	if (ccs.credits - installationTemplate->cost > 0) {
		if (CL_NewInstallation(installationCurrent, installationTemplate, newInstallationPos)) {
			Com_DPrintf(DEBUG_CLIENT, "INS_BuildInstallation_f: numInstallations: %i\n", gd.numInstallations);

			/* set up the installation */
			INS_SetUpInstallation(installationCurrent, installationTemplate);

			campaignStats.installationsBuild++;
			gd.mapAction = MA_NONE;
			CL_UpdateCredits(ccs.credits - installationTemplate->cost);
			Q_strncpyz(installationCurrent->name, mn_installation_title->string, sizeof(installationCurrent->name));
			nation = MAP_GetNation(installationCurrent->pos);
			if (nation)
				Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("A new installation has been built: %s (nation: %s)"), mn_installation_title->string, _(nation->name));
			else
				Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("A new installation has been built: %s"), mn_installation_title->string);
			MN_AddNewMessage(_("Installation built"), mn.messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);

			Cbuf_AddText(va("mn_select_installation %i;", installationCurrent->idx));
			return;
		}
	} else {
		if (r_geoscape_overlay->integer & OVERLAY_RADAR)
			MAP_SetOverlay("radar");
		if (gd.mapAction == MA_NEWINSTALLATION)
			gd.mapAction = MA_NONE;

		Com_sprintf(popupText, sizeof(popupText), _("Not enough credits to set up a new installation."));
		MN_Popup(_("Notice"), popupText);
	}
}

/**
 * @brief Cleans all installations but restore the installation names
 * @sa CL_GameNew
 */

void INS_NewInstallations (void)
{
	/* reset installations */
	int i;
	char title[MAX_VAR];
	installation_t *installation;

	for (i = 0; i < MAX_INSTALLATIONS; i++) {
		installation = INS_GetInstallationByIDX(i);
		Q_strncpyz(title, installation->name, sizeof(title));
/*		INS_ClearInstallation(installation); */
		Q_strncpyz(installation->name, title, sizeof(title));
	}
}


#ifdef DEBUG

/**
 * @brief Just lists all installations with their data
 * @note called with debug_listinstallation
 * @note Just for debugging purposes - not needed in game
 * @todo To be extended for load/save purposes
 */
static void INS_InstallationList_f (void)
{
	int i, j;
	installation_t *installation;

	for (i = 0, installation = gd.installations; i < MAX_INSTALLATIONS; i++, installation++) {
		if (!installation->founded) {
			Com_Printf("Installation idx %i not founded\n\n", i);
			continue;
		}

		Com_Printf("Installation idx %i\n", installation->idx);
		Com_Printf("Installation name %s\n", installation->name);
		Com_Printf("Installation founded %i\n", installation->founded);
		Com_Printf("Installation numAircraftInInstallation %i\n", installation->numAircraftInInstallation);
		Com_Printf("Installation numMissileBattery %i\n", installation->numBatteries);
		Com_Printf("Installation sensorWidth %i\n", installation->radar.range);
		Com_Printf("Installation numSensoredAircraft %i\n", installation->radar.numUFOs);
		Com_Printf("Installation Alien interest %f\n", installation->alienInterest);
		Com_Printf("\nInstallation aircraft %i\n", installation->numAircraftInInstallation);
		for (j = 0; j < installation->numAircraftInInstallation; j++) {
			Com_Printf("Installation aircraft-team %i\n", installation->aircraft[j].teamSize);
		}
		Com_Printf("Installation pos %.02f:%.02f\n", installation->pos[0], installation->pos[1]);
		Com_Printf("\n\n");
	}
}
#endif

/**
 * @brief Sets the title of the installation.
 */
static void INS_SetInstallationTitle_f (void)
{
	Com_DPrintf(DEBUG_CLIENT, "INS_SetInstallationTitle_f: #installations: %i\n", gd.numInstallations);
	if (gd.numInstallations < MAX_INSTALLATIONS)
		Cvar_Set("mn_installation_title", gd.installations[gd.numInstallations].name);
	else {
		MN_AddNewMessage(_("Notice"), _("You've reached the installation limit."), qfalse, MSG_STANDARD, NULL);
		MN_PopMenu(qfalse);		/* remove the new installation popup */
	}
}

/**
 * @brief Creates console command to change the name of a installation.
 * Copies the value of the cvar mn_installation_title over as the name of the
 * current selected installation
 */
static void INS_ChangeInstallationName_f (void)
{
	/* maybe called without installation initialized or active */
	if (!installationCurrent)
		return;

	Q_strncpyz(installationCurrent->name, Cvar_VariableString("mn_installation_title"), sizeof(installationCurrent->name));
}


/**
 * @brief This function checks whether a user build the max allowed amount of installations
 * if yes, the MN_PopMenu will pop the installation build menu from the stack
 */
static void INS_CheckMaxInstallations_f (void)
{
	if (gd.numInstallations >= MAX_INSTALLATIONS) {
		MN_PopMenu(qfalse);
	}
}

/**
 * @brief Resets console commands.
 * @sa MN_ResetMenus
 */
void INS_InitStartup (void)
{
	int idx;

	Com_DPrintf(DEBUG_CLIENT, "Reset installation\n");

	Cvar_SetValue("mn_installation_max", 10);

	for (idx = 0; idx < gd.numInstallationTemplates; idx++) {
		gd.installationTemplates[idx].id = NULL;
		gd.installationTemplates[idx].name = NULL;
		gd.installationTemplates[idx].cost = 0;
		gd.installationTemplates[idx].radarRange = 0.0f;
		gd.installationTemplates[idx].numMaxUfoStored = 0;
		gd.installationTemplates[idx].numMaxBatteries = 0;
	}


	/* add commands and cvars */
	Cmd_AddCommand("mn_select_installation", INS_SelectInstallation_f, NULL);
	Cmd_AddCommand("mn_build_installation", INS_BuildInstallation_f, NULL);
	Cmd_AddCommand("mn_set_installation_title", INS_SetInstallationTitle_f, NULL);
	Cmd_AddCommand("mn_check_max_installations", INS_CheckMaxInstallations_f, NULL);
	Cmd_AddCommand("mn_rename_installation", INS_RenameInstallation_f, "Rename the current installation");
	Cmd_AddCommand("mn_installation_changename", INS_ChangeInstallationName_f, "Called after editing the cvar installation name");
#ifdef DEBUG
	Cmd_AddCommand("debug_listinstallation", INS_InstallationList_f, "Print installation information to the game console");
#endif

	mn_installation_count = Cvar_Get("mn_installation_count", "0", 0, "Current amount of build installations");
	mn_installation_id = Cvar_Get("mn_installation_id", "-1", 0, "Internal id of the current selected installation");
}

/**
 * @brief Counts the number of installations.
 * @return The number of founded installations.
 */
int INS_GetFoundedInstallationCount (void)
{
	int i, cnt = 0;

	for (i = 0; i < MAX_INSTALLATIONS; i++) {
		if (!gd.installations[i].founded)
			continue;
		cnt++;
	}

	return cnt;
}

/**
 * @brief Reads information about installations.
 * @sa CL_ParseScriptFirst
 * @note write into cl_localPool - free on every game restart and reparse
 */
void INS_ParseInstallationNames (const char *name, const char **text)
{
	const char *errhead = "INS_ParseInstallationNames: unexpected end of file (names ";
	const char *token;
	installation_t *installation;

	gd.numInstallationNames = 0;

	/* get token */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("INS_ParseInstallationNames: installation \"%s\" without body ignored\n", name);
		return;
	}
	do {
		/* add installation */
		if (gd.numInstallationNames > MAX_INSTALLATIONS) {
			Com_Printf("INS_ParseInstallationNames: too many installations\n");
			return;
		}

		/* get the name */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		installation = INS_GetInstallationByIDX(gd.numInstallationNames);
		memset(installation, 0, sizeof(*installation));
		installation->idx = gd.numInstallationNames;

		/* get the title */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;
		if (*token == '_')
			token++;
		Q_strncpyz(installation->name, _(token), sizeof(installation->name));
		Com_DPrintf(DEBUG_CLIENT, "Found installation %s\n", installation->name);
		gd.numInstallationNames++; /** @todo Use this value instead of MAX_INSTALLATIONS in the for loops */
	} while (*text);

	mn_installation_title = Cvar_Get("mn_installation_title", "", 0, NULL);
}

/**
 * @brief Copies an entry from the installation description file into the list of installation templates.
 * @note Parses one "installation" entry in the installation.ufo file and writes
 * it into the next free entry in installationTemplates.
 * @param[in] name Unique test-id of a installationTemplate_t.
 * @param[in] text @todo document this ... It appears to be the whole following text that is part of the "building" item definition in .ufo.
 */
void INS_ParseInstallations (const char *name, const char **text)
{
	installationTemplate_t *installation;
	const char *errhead = "INS_ParseInstallations: unexpected end of file (names ";
	const char *token;
	int i;

	/* get id list body */
	token = COM_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("INS_ParseInstallations: installation \"%s\" without body ignored\n", name);
		return;
	}

	if (!name) {
		Com_Printf("INS_ParseInstallations: installation name not specified.\n");
		return;

	}

	if (gd.numInstallationTemplates >= MAX_INSTALLATION_TEMPLATES) {
		Com_Printf("INS_ParseInstallations: too many installation templates\n");
		gd.numInstallationTemplates = MAX_INSTALLATION_TEMPLATES;	/* just in case it's bigger. */
		return;
	}

	for (i = 0; i < gd.numInstallationTemplates; i++) {
		if (!Q_strcmp(gd.installationTemplates[i].name, name)) {
			Com_Printf("INS_ParseInstallations: Second installation with same name found (%s) - second ignored\n", name);
			return;
		}
	}

	/* new entry */
	installation = &gd.installationTemplates[gd.numInstallationTemplates];
	memset(installation, 0, sizeof(*installation));
	installation->id = Mem_PoolStrDup(name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);

	Com_DPrintf(DEBUG_CLIENT, "...found installation %s\n", installation->id);

	gd.numInstallationTemplates++;
	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* get values */
		if (!Q_strncmp(token, "name", MAX_VAR)) {
			token = COM_EParse(text, errhead, name);
			if (!*text)
				return;

			installation->name = Mem_PoolStrDup(token, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
		} else if (!Q_strncmp(token, "cost", MAX_VAR)) {
			token = COM_EParse(text, errhead, name);
			if (!*text)
				return;
			installation->cost = atoi(token);
		} else if (!Q_strncmp(token, "radar_range", MAX_VAR)) {
			token = COM_EParse(text, errhead, name);
			if (!*text)
				return;
			installation->radarRange = atof(token);
		} else if (!Q_strncmp(token, "max_batteries", MAX_VAR)) {
			token = COM_EParse(text, errhead, name);
			if (!*text)
				return;
			installation->numMaxBatteries = atoi(token);
		} else if (!Q_strncmp(token, "max_ufo_stored", MAX_VAR)) {
			token = COM_EParse(text, errhead, name);
			if (!*text)
				return;
			installation->numMaxUfoStored = atoi(token);
		}

	} while (*text);
}


/**
 * @brief Save callback for savegames
 * @sa INS_Load
 * @sa SAV_GameSave
 */
qboolean INS_Save (sizebuf_t* sb, void* data)
{
	int i;
	for (i = 0; i < presaveArray[PRE_MAXINST]; i++) {
#if 0
		const installation_t *inst = INS_GetInstallationByIDX(i);
		MSG_WriteByte(sb, inst->founded);
		if (!inst->founded)
			continue;
		MSG_WriteString(sb, inst->name);
		MSG_WritePos(sb, inst->pos);
		MSG_WriteByte(sb, inst->installationStatus);
		MSG_WriteShort(sb, inst->installationDamage);
		MSG_WriteFloat(sb, inst->alienInterest);
		MSG_WriteShort(sb, inst->radar.range);

		MSG_WriteByte(sb, inst->numBatteries);
		B_SaveBaseSlots(inst->batteries, inst->numBatteries, sb);
		/** @todo storage, aircraft (don't save capacities, they should
		 * be recalculated after loading) */
#endif
	}
	return qtrue;
}



/**
 * @brief Load callback for savegames
 * @sa INS_Save
 * @sa SAV_GameLoad
 * @sa INS_LoadItemSlots
 */
qboolean INS_Load (sizebuf_t* sb, void* data)
{
	int i;
	for (i = 0; i < presaveArray[PRE_MAXINST]; i++) {
#if 0
		installation_t *inst = INS_GetInstallationByIDX(i);
		inst->founded = MSG_ReadByte(sb);
		if (!inst->founded)
			continue;
		Q_strncpyz(inst->name, MSG_ReadStringRaw(sb), sizeof(inst->name));
		MSG_ReadPos(sb, inst->pos);
		inst->installationStatus = MSG_ReadByte(sb);
		inst->installationDamage = MSG_ReadShort(sb);
		inst->alienInterest = MSG_ReadFloat(sb);
		RADAR_Initialise(&inst->radar, MSG_ReadShort(sb), 1.0f, qtrue);

		/* read battery slots */
		inst->numBatteries = MSG_ReadByte(sb);
		B_LoadBaseSlots(inst->batteries, inst->numBatteries, sb);

		/** @todo storage, aircraft */
		/** @todo don't forget to recalc the capacities like we do for bases */
#endif
	}
	return qtrue;
}

