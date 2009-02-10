/**
 * @file cl_installation.c
 * @brief Handles everything that is located in or accessed through an installation.
 * @note Installation functions prefix: INS_*
 * @todo Allow transfer of items to installations
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
#include "../cl_team.h"
#include "cl_mapfightequip.h"
#include "cp_aircraft.h"
#include "cp_missions.h"
#include "../cl_view.h"
#include "cl_map.h"
#include "cl_ufo.h"
#include "cl_popup.h"
#include "../renderer/r_draw.h"
#include "../menu/m_popup.h"
#include "cl_installation.h"
#include "cp_geoscape_actions.h"
#include "../mxml/mxml_ufoai.h"
#include "../../shared/parse.h"

installation_t *installationCurrent;

vec3_t newInstallationPos;
static cvar_t *mn_installation_title;
static cvar_t *mn_installation_count;
static cvar_t *mn_installation_id;

installationType_t INS_GetType (const installation_t *installation)
{
	if (installation->installationTemplate->maxBatteries > 0)
		return INSTALLATION_DEFENCE;
	else if (installation->installationTemplate->maxUFOsStored > 0)
		return INSTALLATION_UFOYARD;

	/* default is radar */
	return INSTALLATION_RADAR;
}

/**
 * @brief Array bound check for the installation index.
 * @param[in] instIdx  Instalation's index
 * @return Pointer to the installation corresponding to instIdx.
 */
installation_t* INS_GetInstallationByIDX (int instIdx)
{
	assert(instIdx < MAX_INSTALLATIONS);
	assert(instIdx >= 0);
	return &ccs.installations[instIdx];
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
 * @brief Returns the installation Template for a given installation ID.
 * @param[in] id ID of the installation template to find.
 * @return corresponding installation Template, @c NULL if not found.
 */
static installationTemplate_t* INS_GetInstallationTemplateFromInstallationId (const char *id)
{
	int idx;

	for (idx = 0; idx < ccs.numInstallationTemplates; idx++) {
		if (Q_strncmp(ccs.installationTemplates[idx].id, id, MAX_VAR) == 0) {
			return &ccs.installationTemplates[idx];
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
	int i;

	assert(installation);

	installation->idx = ccs.numInstallations - 1;
	installation->founded = qtrue;
	installation->installationStatus = INSTALLATION_UNDER_CONSTRUCTION;
	installation->installationTemplate = installationTemplate;
	installation->buildStart = ccs.date.day;

	/* Reset current capacities. */
	installation->aircraftCapacity.cur = 0;

	/* this cvar is used for disabling the installation build button on geoscape if MAX_INSTALLATIONS was reached */
	Cvar_Set("mn_installation_count", va("%i", ccs.numInstallations));

	/* this cvar is needed by INS_SetBuildingByClick below*/
	Cvar_SetValue("mn_installation_id", installation->idx);

	installation->numUFOsInInstallation = 0;

	/* a new installation is not discovered (yet) */
	installation->alienInterest = newInstallationAlienInterest;

	/* intialise hit points */
	installation->installationDamage = installation->installationTemplate->maxDamage;

	/* initialise Batteries */
	installation->numBatteries = installation->installationTemplate->maxBatteries;

	/* Add defenceweapons to storage */
	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *item = &csi.ods[i];
		/* this is a craftitem but also dummy */
		if (INV_IsBaseDefenceItem(item)) {
			installation->storage.num[item->idx] = installation->installationTemplate->maxBatteries;
		}
	}
	BDEF_InitialiseInstallationSlots(installation);

	Com_DPrintf(DEBUG_CLIENT, "INS_SetUpInstallation: id = %s, range = %i, batteries = %i, ufos = %i\n",
		installation->installationTemplate->id, installation->installationTemplate->radarRange,
		installation->installationTemplate->maxBatteries, installation->installationTemplate->maxUFOsStored);

	/* Reset Radar range */
	RADAR_Initialise(&(installation->radar), 0.0f, 0.0f, 1.0f, qtrue);
	RADAR_UpdateInstallationRadarCoverage(installation, installation->installationTemplate->radarRange, installation->installationTemplate->trackingRange);
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
 * @brief Select an installation when clicking on it on geoscape, or build a new installation.
 * @param[in] installation If this is @c NULL we want to installation a new base
 * @note This is (and should be) the only place where installationCurrent is set
 * to a value that is not @c NULL
 */
void INS_SelectInstallation (installation_t *installation)
{
	/* set up a new installation */
	if (!installation) {
		int installationID;

		/* if player hit the "create base" button while creating base mode is enabled
		 * that means that player wants to quit this mode */
		if (ccs.mapAction == MA_NEWINSTALLATION) {
			MAP_ResetAction();
			if (!radarOverlayWasSet)
				MAP_DeactivateOverlay("radar");
			return;
		}

		ccs.mapAction = MA_NEWINSTALLATION;
		installationID = INS_GetFirstUnfoundedInstallation();
		Com_DPrintf(DEBUG_CLIENT, "INS_SelectInstallation_f: new installationID is %i\n", installationID);
		if (installationID < MAX_INSTALLATIONS) {
			installationCurrent = INS_GetInstallationByIDX(installationID);
			installationCurrent->idx = installationID;
			Com_DPrintf(DEBUG_CLIENT, "B_SelectBase_f: baseID is valid for base: %s\n", installationCurrent->name);
			/* show radar overlay (if not already displayed) */
			if (!(r_geoscape_overlay->integer & OVERLAY_RADAR))
				MAP_SetOverlay("radar");
		} else {
			Com_Printf("MaxInstallations reached\n");
			/* select the first installation in list */
			installationCurrent = INS_GetInstallationByIDX(0);
			ccs.mapAction = MA_NONE;
		}
	} else {
		const int timetobuild = max(0, installation->installationTemplate->buildTime - (ccs.date.day - installation->buildStart));

		Com_DPrintf(DEBUG_CLIENT, "INS_SelectInstallation_f: select installation with id %i\n", installation->idx);
		installationCurrent = installation;
		baseCurrent = NULL;
		ccs.mapAction = MA_NONE;
		Cvar_SetValue("mn_installation_id", installation->idx);
		Cvar_Set("mn_installation_title", installation->name);
		Cvar_Set("mn_installation_type", installation->installationTemplate->id);
		if (installation->installationStatus == INSTALLATION_WORKING) {
			Cvar_Set("mn_installation_timetobuild", "-");
		} else {
			Cvar_Set("mn_installation_timetobuild", va(ngettext("%d day", "%d days", timetobuild), timetobuild));
		}
		MN_PushMenu("popup_installationstatus", NULL);
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

	if (installationID >= 0 && installationID < ccs.numInstallations)
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

	if (!curCampaign)
		return;

	if (Cmd_Argc() < 1) {
		Com_Printf("Usage: %s <installationType>\n", Cmd_Argv(0));
		return;
	}

	/* we should always have at least one base */
	if (!ccs.numBases)
		return;

	installationTemplate = INS_GetInstallationTemplateFromInstallationId(Cmd_Argv(1));

	if (!installationTemplate) {
		Com_Printf("The installation type %s passed for %s is not valid.\n", Cmd_Argv(1), Cmd_Argv(0));
		return;
	}

	if (!installationCurrent)
		return;

	assert(!installationCurrent->founded);
	assert(installationTemplate->cost >= 0);

	if (ccs.credits - installationTemplate->cost > 0) {
		/** @todo If there is no nation assigned to the current selected position,
		 * tell this the gamer and give him an option to rechoose the location.
		 * If we don't do this, any action that is done for this installation has no
		 * influence to any nation happiness/funding/supporting */
		if (CL_NewInstallation(installationCurrent, installationTemplate, newInstallationPos)) {
			Com_DPrintf(DEBUG_CLIENT, "INS_BuildInstallation_f: numInstallations: %i\n", ccs.numInstallations);

			/* set up the installation */
			INS_SetUpInstallation(installationCurrent, installationTemplate);

			campaignStats.installationsBuild++;
			ccs.mapAction = MA_NONE;
			CL_UpdateCredits(ccs.credits - installationTemplate->cost);
			Q_strncpyz(installationCurrent->name, mn_installation_title->string, sizeof(installationCurrent->name));
			nation = MAP_GetNation(installationCurrent->pos);
			if (nation)
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("A new installation has been built: %s (nation: %s)"), mn_installation_title->string, _(nation->name));
			else
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("A new installation has been built: %s"), mn_installation_title->string);
			MSO_CheckAddNewMessage(NT_INSTALLATION_BUILDSTART, _("Installation building"), cp_messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);

			Cbuf_AddText(va("mn_select_installation %i;", installationCurrent->idx));
			return;
		}
	} else {
		if (r_geoscape_overlay->integer & OVERLAY_RADAR)
			MAP_SetOverlay("radar");
		if (ccs.mapAction == MA_NEWINSTALLATION)
			ccs.mapAction = MA_NONE;

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
	int i;
	installation_t *installation;

	for (i = 0, installation = ccs.installations; i < MAX_INSTALLATIONS; i++, installation++) {
		if (!installation->founded) {
			Com_Printf("Installation idx %i not founded\n\n", i);
			continue;
		}

		Com_Printf("Installation idx %i\n", installation->idx);
		Com_Printf("Installation name %s\n", installation->name);
		Com_Printf("Installation founded %i\n", installation->founded);
		Com_Printf("Installation numUFOsInInstallation %i\n", installation->numUFOsInInstallation);
		Com_Printf("Installation sensorWidth %i\n", installation->radar.range);
		Com_Printf("Installation numSensoredAircraft %i\n", installation->radar.numUFOs);
		Com_Printf("Installation Alien interest %f\n", installation->alienInterest);
		Com_Printf("\nInstallation aircraft %i\n", installation->numUFOsInInstallation);
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
	Com_DPrintf(DEBUG_CLIENT, "INS_SetInstallationTitle_f: #installations: %i\n", ccs.numInstallations);
	if (ccs.numInstallations < MAX_INSTALLATIONS)
		Cvar_Set("mn_installation_title", ccs.installations[ccs.numInstallations].name);
	else {
		MS_AddNewMessage(_("Notice"), _("You've reached the installation limit."), qfalse, MSG_STANDARD, NULL);
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
	if (ccs.numInstallations >= MAX_INSTALLATIONS) {
		MN_PopMenu(qfalse);
	}
}

/**
 * @brief Destroys an installation
 * @param[in] pointer to the installation to be destroyed
 */
void INS_DestroyInstallation (installation_t *installation)
{
	if (!installation)
		return;
	if (!installation->founded)
		return;

	RADAR_UpdateInstallationRadarCoverage(installation, 0, 0);
	CP_MissionNotifyInstallationDestroyed(installation);
	ccs.numInstallations--;
	installationCurrent = NULL;
	installation->founded = qfalse;

	Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Installation %s was destroyed."), _(installation->name));
	MSO_CheckAddNewMessage(NT_INSTALLATION_DESTROY, _("Installation destroyed"), cp_messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);
}

/**
 * @brief console function for destroying an installation
 * @sa INS_DestroyInstallation
 */
static void INS_DestroyInstallation_f (void)
{
	installation_t *installation;

	if (Cmd_Argc() < 2 || atoi(Cmd_Argv(1)) < 0) {
		installation = installationCurrent;
	} else {
		installation = INS_GetFoundedInstallationByIDX(atoi(Cmd_Argv(1)));
		Cvar_SetValue("mn_installation_id", installation->idx);
	}
	/** Ask 'Are you sure?' by default */
	if (Cmd_Argc() < 3) {
		char command[MAX_VAR];

		Com_sprintf(command, MAX_VAR, "mn_destroyinstallation %d 1; mn_pop;", installation->idx);
		MN_PopupButton(_("Destroy Installation"), _("Do you really want to destroy this installation?"),
			command, _("Destroy"), _("Destroy installation"),
			"mn_pop;", _("Cancel"), _("Forget it"),
			NULL, NULL, NULL);
		return;
	}
	INS_DestroyInstallation(installation);
}

/**
 * @brief Resets console commands.
 * @sa MN_ResetMenus
 */
void INS_InitStartup (void)
{
	int idx;

	Com_DPrintf(DEBUG_CLIENT, "Reset installation\n");

	Cvar_SetValue("mn_installation_max", MAX_INSTALLATIONS);

	for (idx = 0; idx < ccs.numInstallationTemplates; idx++) {
		ccs.installationTemplates[idx].id = NULL;
		ccs.installationTemplates[idx].name = NULL;
		ccs.installationTemplates[idx].cost = 0;
		ccs.installationTemplates[idx].radarRange = 0.0f;
		ccs.installationTemplates[idx].trackingRange = 0.0f;
		ccs.installationTemplates[idx].maxUFOsStored = 0;
		ccs.installationTemplates[idx].maxBatteries = 0;
	}

	/* add commands and cvars */
	Cmd_AddCommand("mn_select_installation", INS_SelectInstallation_f, NULL);
	Cmd_AddCommand("mn_build_installation", INS_BuildInstallation_f, NULL);
	Cmd_AddCommand("mn_set_installation_title", INS_SetInstallationTitle_f, NULL);
	Cmd_AddCommand("mn_check_max_installations", INS_CheckMaxInstallations_f, NULL);
	Cmd_AddCommand("mn_rename_installation", INS_RenameInstallation_f, "Rename the current installation");
	Cmd_AddCommand("mn_installation_changename", INS_ChangeInstallationName_f, "Called after editing the cvar installation name");
	Cmd_AddCommand("mn_destroyinstallation", INS_DestroyInstallation_f, "Destroys an installation");
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
		if (!ccs.installations[i].founded)
			continue;
		cnt++;
	}

	return cnt;
}

/**
 * @brief Check if some installation are build.
 * @note Daily called.
 */
void INS_UpdateInstallationData (void)
{
	int instIdx;

	for (instIdx = 0; instIdx < MAX_INSTALLATIONS; instIdx++) {
		installation_t *installation = INS_GetFoundedInstallationByIDX(instIdx);
		if (!installation)
			continue;

		if ((installation->installationStatus == INSTALLATION_UNDER_CONSTRUCTION)
		 && installation->buildStart
		 && installation->buildStart + installation->installationTemplate->buildTime <= ccs.date.day) {
			installation->installationStatus = INSTALLATION_WORKING;
			RADAR_UpdateInstallationRadarCoverage(installation, installation->installationTemplate->radarRange, installation->installationTemplate->trackingRange);

			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Construction of installation %s finished."), _(installation->name));
				MSO_CheckAddNewMessage(NT_INSTALLATION_BUILDFINISH, _("Installation finished"), cp_messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);
		}
	}
}

/**
 * @brief Reads information about installations.
 * @sa CL_ParseScriptFirst
 * @note write into cl_localPool - free on every game restart and reparse
 * @todo Remove this and give an automatically generated name
 */
void INS_ParseInstallationNames (const char *name, const char **text)
{
	const char *errhead = "INS_ParseInstallationNames: unexpected end of file (names ";
	const char *token;
	installation_t *installation;

	ccs.numInstallationNames = 0;

	/* get token */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("INS_ParseInstallationNames: installation \"%s\" without body ignored\n", name);
		return;
	}
	do {
		/* add installation */
		if (ccs.numInstallationNames > MAX_INSTALLATIONS) {
			Com_Printf("INS_ParseInstallationNames: too many installations\n");
			return;
		}

		/* get the name */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		installation = INS_GetInstallationByIDX(ccs.numInstallationNames);
		memset(installation, 0, sizeof(*installation));
		installation->idx = ccs.numInstallationNames;

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
		ccs.numInstallationNames++; /** @todo Use this value instead of MAX_INSTALLATIONS in the for loops */
	} while (*text);

	mn_installation_title = Cvar_Get("mn_installation_title", "", 0, NULL);
}

static const value_t installation_vals[] = {
	{"name", V_TRANSLATION_STRING, offsetof(installationTemplate_t, name), 0},
	{"radar_range", V_INT, offsetof(installationTemplate_t, radarRange), MEMBER_SIZEOF(installationTemplate_t, radarRange)},
	{"radar_tracking_range", V_INT, offsetof(installationTemplate_t, trackingRange), MEMBER_SIZEOF(installationTemplate_t, trackingRange)},
	{"max_batteries", V_INT, offsetof(installationTemplate_t, maxBatteries), MEMBER_SIZEOF(installationTemplate_t, maxBatteries)},
	{"max_ufo_stored", V_INT, offsetof(installationTemplate_t, maxUFOsStored), MEMBER_SIZEOF(installationTemplate_t, maxUFOsStored)},
	{"max_damage", V_INT, offsetof(installationTemplate_t, maxDamage), MEMBER_SIZEOF(installationTemplate_t, maxDamage)},
	{"model", V_CLIENT_HUNK_STRING, offsetof(installationTemplate_t, model), 0},

	{NULL, 0, 0, 0}
};

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
	const value_t *vp;
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

	if (ccs.numInstallationTemplates >= MAX_INSTALLATION_TEMPLATES) {
		Com_Printf("INS_ParseInstallations: too many installation templates\n");
		ccs.numInstallationTemplates = MAX_INSTALLATION_TEMPLATES;	/* just in case it's bigger. */
		return;
	}

	for (i = 0; i < ccs.numInstallationTemplates; i++) {
		if (!Q_strcmp(ccs.installationTemplates[i].name, name)) {
			Com_Printf("INS_ParseInstallations: Second installation with same name found (%s) - second ignored\n", name);
			return;
		}
	}

	/* new entry */
	installation = &ccs.installationTemplates[ccs.numInstallationTemplates];
	memset(installation, 0, sizeof(*installation));
	installation->id = Mem_PoolStrDup(name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);

	Com_DPrintf(DEBUG_CLIENT, "...found installation %s\n", installation->id);

	ccs.numInstallationTemplates++;
	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* check for some standard values */
		for (vp = installation_vals; vp->string; vp++)
			if (!Q_strcmp(token, vp->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				switch (vp->type) {
				case V_TRANSLATION_STRING:
					token++;
				case V_CLIENT_HUNK_STRING:
					Mem_PoolStrDupTo(token, (char**) ((char*)installation + (int)vp->ofs), cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
					break;
				default:
					if (Com_EParseValue(installation, token, vp->type, vp->ofs, vp->size) == -1)
						Com_Printf("INS_ParseInstallations: Wrong size for value %s\n", vp->string);
					break;
				}
				break;
			}

		/* other values */
		if (!vp->string) {
			if (!Q_strncmp(token, "cost", MAX_VAR)) {
				char cvarname[MAX_VAR] = "mn_installation_";

				Q_strcat(cvarname, installation->id, MAX_VAR);
				Q_strcat(cvarname, "_cost", MAX_VAR);

				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;
				installation->cost = atoi(token);

				Cvar_Set(cvarname, va(_("%d c"), atoi(token)));
			} else if (!Q_strncmp(token, "buildtime", MAX_VAR)) {
				char cvarname[MAX_VAR] = "mn_installation_";

				Q_strcat(cvarname, installation->id, MAX_VAR);
				Q_strcat(cvarname, "_buildtime", MAX_VAR);

				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;
				installation->buildTime = atoi(token);

				Cvar_Set(cvarname, va(ngettext("%d day\n", "%d days\n", atoi(token)), atoi(token)));
			}
		}
	} while (*text);
}

/**
 * @brief Save callback for savegames in xml
 * @sa INS_LoadXML
 * @sa SAV_GameSaveXML
 */
qboolean INS_SaveXML (mxml_node_t *p)
{
	int i;
	mxml_node_t *n;
	n = mxml_AddNode(p, "installations");
	for (i = 0; i < MAX_INSTALLATIONS; i++) {
		const installation_t *inst = INS_GetInstallationByIDX(i);
		mxml_node_t *s, *ss;
		if (!inst->founded)
			continue;
		s = mxml_AddNode(n, "installation");
		mxml_AddInt(s, "idx", inst->idx);
		mxml_AddBool(s, "founded", inst->founded);
		mxml_AddString(s, "templateid", inst->installationTemplate->id);
		mxml_AddString(s, "name", inst->name);
		mxml_AddPos3(s, "pos", inst->pos);
		mxml_AddInt(s, "status", inst->installationStatus);
		mxml_AddInt(s, "damage", inst->installationDamage);
		mxml_AddFloat(s, "alieninterest", inst->alienInterest);
		mxml_AddInt(s, "buildstart", inst->buildStart);

		ss = mxml_AddNode(s, "batteries");
		mxml_AddInt(ss, "num", inst->numBatteries);
		B_SaveBaseSlotsXML(inst->batteries, inst->numBatteries, ss);

		/* store equipments */
		/* reducing redundant code */
		B_SaveStorageXML(s, inst->storage);

		/** @todo aircraft (don't save capacities, they should
		 * be recalculated after loading) */
	}
	return qtrue;
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
		int j;
		const installation_t *inst = INS_GetInstallationByIDX(i);
		MSG_WriteByte(sb, inst->founded);
		if (!inst->founded)
			continue;
		MSG_WriteString(sb, inst->installationTemplate->id);
		MSG_WriteString(sb, inst->name);
		MSG_WritePos(sb, inst->pos);
		MSG_WriteByte(sb, inst->installationStatus);
		MSG_WriteShort(sb, inst->installationDamage);
		MSG_WriteFloat(sb, inst->alienInterest);
		MSG_WriteLong(sb, inst->buildStart);

		B_SaveBaseSlots(inst->batteries, inst->numBatteries, sb);

		/* store equipments */
		for (j = 0; j < presaveArray[PRE_NUMODS]; j++) {
			MSG_WriteString(sb, csi.ods[j].id);
			MSG_WriteLong(sb, inst->storage.num[j]);
		}

		/** @todo aircraft (don't save capacities, they should
		 * be recalculated after loading) */
	}
	return qtrue;
}

/**
 * @brief Load callback for savegames
 * @sa INS_SaveXML
 * @sa SAV_GameLoadXML
 * @sa INS_LoadItemSlots
 */
qboolean INS_LoadXML (mxml_node_t *p)
{
	mxml_node_t *s;
	mxml_node_t *n = mxml_GetNode(p, "installations");
	if (!n)
		return qfalse;

	for (s = mxml_GetNode(n, "installation"); s ; s = mxml_GetNextNode(s,n, "installation")) {
		mxml_node_t *ss;
		const int idx = mxml_GetInt(s, "idx", 0);
		installation_t *inst = INS_GetInstallationByIDX(idx);
		inst->founded = mxml_GetBool(s, "founded", inst->founded);
		/* should never happen, we only save founded installations */
		if (!inst->founded)
			continue;
		inst->installationTemplate = INS_GetInstallationTemplateFromInstallationId(mxml_GetString(s, "templateid"));
		if (!inst->installationTemplate) {
			Com_Printf("Could not find installation template\n");
			return qfalse;
		}
		ccs.numInstallations++;
		Q_strncpyz(inst->name, mxml_GetString(s, "name"), sizeof(inst->name));

		mxml_GetPos3(s, "pos", inst->pos);

		inst->installationStatus = mxml_GetInt(s, "status", 0);
		inst->installationDamage = mxml_GetInt(s, "damage", 0);
		inst->alienInterest = mxml_GetFloat(s, "alieninterest", 0.0);

		RADAR_InitialiseUFOs(&inst->radar);
		RADAR_Initialise(&(inst->radar), 0.0f, 0.0f, 1.0f, qtrue);
		RADAR_UpdateInstallationRadarCoverage(inst, inst->installationTemplate->radarRange, inst->installationTemplate->trackingRange);

		inst->buildStart = mxml_GetInt(s, "buildstart", 0);

		/* read battery slots */
		BDEF_InitialiseInstallationSlots(inst);

		ss = mxml_GetNode(s, "batteries");
		if (!ss){
			Com_Printf("INS_LoadXML: Batteries not defined!\n");
			return qfalse;
		}
		inst->numBatteries = mxml_GetInt(ss, "num", 0);
		if (inst->numBatteries > inst->installationTemplate->maxBatteries){
			Com_Printf("Installation has more batteries than possible, using upper bound\n");
			inst->numBatteries = inst->installationTemplate->maxBatteries;
		}
		B_LoadBaseSlotsXML(inst->batteries, inst->numBatteries, ss);

		B_LoadStorageXML(s, &inst->storage);
		/** @todo aircraft */
		/** @todo don't forget to recalc the capacities like we do for bases */
	}
	Cvar_SetValue("mn_installation_count", ccs.numInstallations);
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
		int j;
		installation_t *inst = INS_GetInstallationByIDX(i);
		inst->founded = MSG_ReadByte(sb);
		if (!inst->founded)
			continue;
		inst->installationTemplate = INS_GetInstallationTemplateFromInstallationId(MSG_ReadString(sb));
		if (!inst->installationTemplate) {
			Com_Printf("Could not find installation template\n");
			return qfalse;
		}
		ccs.numInstallations++;
		Q_strncpyz(inst->name, MSG_ReadStringRaw(sb), sizeof(inst->name));
		MSG_ReadPos(sb, inst->pos);
		inst->installationStatus = MSG_ReadByte(sb);
		inst->installationDamage = MSG_ReadShort(sb);
		inst->alienInterest = MSG_ReadFloat(sb);
		RADAR_InitialiseUFOs(&inst->radar);
		RADAR_Initialise(&(inst->radar), 0.0f, 0.0f, 1.0f, qtrue);
		RADAR_UpdateInstallationRadarCoverage(inst, inst->installationTemplate->radarRange, inst->installationTemplate->trackingRange);
		inst->buildStart = MSG_ReadLong(sb);

		/* read battery slots */
		BDEF_InitialiseInstallationSlots(inst);

		inst->numBatteries = MSG_ReadByte(sb);
		if (inst->numBatteries > inst->installationTemplate->maxBatteries){
			Com_Printf("Installation has more batteries than possible\n");
			return qfalse;
		}
		B_LoadBaseSlots(inst->batteries, inst->numBatteries, sb);

		/* load equipments */
		for (j = 0; j < presaveArray[PRE_NUMODS]; j++) {
			const char *s = MSG_ReadString(sb);
			objDef_t *od = INVSH_GetItemByID(s);
			if (!od) {
				Com_Printf("INS_Load: Could not find item '%s'\n", s);
				MSG_ReadLong(sb);
			} else {
				inst->storage.num[od->idx] = MSG_ReadLong(sb);
			}
		}

		/** @todo aircraft */
		/** @todo don't forget to recalc the capacities like we do for bases */
	}
	Cvar_SetValue("mn_installation_count", ccs.numInstallations);
	return qtrue;
}
