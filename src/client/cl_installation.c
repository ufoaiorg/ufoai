/**
 * @file cl_installation.c
 * @brief Handles everything that is located in or accessed through an installation.
 * @note Installation functions prefix: I_
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

installation_t *installationCurrent;

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
 * @brief Setup new installation
 * @sa CL_NewInstallation
 */
void INS_SetUpInstallation (installation_t* installation)
{
	const int newInstallationAlienInterest = 1.0f;

	assert(installation);
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

	/* Reset Radar range */
	RADAR_Initialise(&(installation->radar), 0.0f, 1.0f, qtrue);
	RADAR_UpdateInstallationRadarCoverage_f(installation);
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
 * @brief Called when a installation is opened or a new installation is created on geoscape.
 *
 * For a new installation the installationID is -1.
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

	/* set up a new installation */
	/* called from *.ufo with -1 */
	if (installationID < 0) {
		gd.mapAction = MA_NEWINSTALLATION;
		installationID = INS_GetFirstUnfoundedInstallation();
		Com_DPrintf(DEBUG_CLIENT, "INS_SelectInstallation_f: new installationID is %i\n", installationID);
		if (installationID < MAX_INSTALLATIONS) {
			installationCurrent = INS_GetInstallationByIDX(installationID);
			installationCurrent->idx = installationID;
/*			Cvar_Set("mn_installation_newcost", va(_("%i c"), curCampaign->installationcost)); @todo setup installation cost for the three types */
			Com_DPrintf(DEBUG_CLIENT, "INS_SelectInstallation_f: installationID is valid for installation: %s\n", installationCurrent->name);
		} else {
			Com_Printf("MaxInstallations reached\n");
			/* select the first installation in list */
			installationCurrent = INS_GetInstallationByIDX(0);
			gd.mapAction = MA_NONE;
		}
	} else if (installationID < MAX_INSTALLATIONS) {
		Com_DPrintf(DEBUG_CLIENT, "INS_SelectInstallation_f: select installation with id %i\n", installationID);
		installation = INS_GetInstallationByIDX(installationID);
		if (installation->founded) {
			installationCurrent = installation;
			gd.mapAction = MA_NONE;
		}
	} else
		return;

}

/**
 * @brief Constructs a new installation.
 * @sa CL_NewInstallation
 */
static void INS_BuildInstallation_f (void)
{
	const nation_t *nation;
	const int installationcost = 100000;
	
	if (!installationCurrent)
		return;

	assert(!installationCurrent->founded);
	assert(ccs.singleplayer);
	assert(curCampaign);

	/* @todo: need to work out where the cost of the installation is to be stored.  Should it be part of the 
	 * campaign or in the installation.ufo config.  (curCampaign->installationcost) */
	if (ccs.credits - installationcost > 0) {
		if (CL_NewInstallation(installationCurrent, newInstallationPos)) {
			Com_DPrintf(DEBUG_CLIENT, "INS_BuildInstallation_f: numInstallations: %i\n", gd.numInstallations);
			installationCurrent->idx = gd.numInstallations - 1;
			installationCurrent->founded = qtrue;
			installationCurrent->installationStatus = INSTALLATION_WORKING;
			Q_strncpyz (installationCurrent->name, "MyInstall", sizeof("MyInstall"));
			
			campaignStats.installationsBuild++;
			gd.mapAction = MA_NONE;
			CL_UpdateCredits(ccs.credits - installationcost);
/*			Q_strncpyz(installationCurrent->name, mn_installation_title->string, sizeof(installationCurrent->name)); */
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
	Com_DPrintf(DEBUG_CLIENT, "Reset installation\n");

	Cvar_SetValue("mn_installation_max", 10);

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
		memset(installation, 0, sizeof(installation_t));
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
 * @brief Save callback for savegames
 * @sa INS_Load
 * @sa SAV_GameSave
 */
qboolean INS_Save (sizebuf_t* sb, void* data)
{

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

	return qtrue;
}

