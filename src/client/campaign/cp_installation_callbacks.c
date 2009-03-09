/**
 * @file cp_installation_callbacks.c
 * @brief Menu related console command callbacks
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
#include "../cl_game.h"
#include "../cl_menu.h"
#include "cl_campaign.h"
#include "cp_installation_callbacks.h"
#include "cl_installation.h"
#include "cl_map.h"
#include "../menu/m_popup.h"
#include "../renderer/r_draw.h"

vec3_t newInstallationPos;
static cvar_t *mn_installation_count;
static cvar_t *mn_installation_id;

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
		Com_DPrintf(DEBUG_CLIENT, "INS_SelectInstallation: new installationID is %i\n", installationID);
		if (installationID < B_GetInstallationLimit()) {
			const installationTemplate_t *instmp = INS_GetInstallationTemplateFromInstallationID(Cvar_VariableString("mn_installation_type"));
			int i = 1;
			int j;

			installationCurrent = INS_GetInstallationByIDX(installationID);
			installationCurrent->idx = installationID;

			do {
				j = 0;
				Com_sprintf(installationCurrent->name, sizeof(installationCurrent->name), "%s #%i", instmp ? _(instmp->name) : _("Installation"), i);
				while (j <= ccs.numInstallations && strcmp(installationCurrent->name, ccs.installations[j++].name));
			} while (i++ <= ccs.numInstallations && j <= ccs.numInstallations);


			Com_DPrintf(DEBUG_CLIENT, "INS_SelectInstallation: baseID is valid for base: %s\n", installationCurrent->name);
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

		Com_DPrintf(DEBUG_CLIENT, "INS_SelectInstallation: select installation with id %i\n", installation->idx);
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

	/* we should always have at least one base */
	if (!ccs.numBases)
		return;

	installationTemplate = INS_GetInstallationTemplateFromInstallationID(Cmd_Argv(1));

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
		if (INS_NewInstallation(installationCurrent, installationTemplate, newInstallationPos)) {
			Com_DPrintf(DEBUG_CLIENT, "INS_BuildInstallation_f: numInstallations: %i\n", ccs.numInstallations);

			/* set up the installation */
			INS_SetUpInstallation(installationCurrent, installationTemplate);

			campaignStats.installationsBuild++;
			ccs.mapAction = MA_NONE;
			CL_UpdateCredits(ccs.credits - installationTemplate->cost);
			Q_strncpyz(installationCurrent->name, Cvar_VariableString("mn_installation_title"), sizeof(installationCurrent->name));
			nation = MAP_GetNation(installationCurrent->pos);
			if (nation)
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("A new installation has been built: %s (nation: %s)"), installationCurrent->name, _(nation->name));
			else
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("A new installation has been built: %s"), installationCurrent->name);
			MSO_CheckAddNewMessage(NT_INSTALLATION_BUILDSTART, _("Installation building"), cp_messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);

			Cbuf_AddText(va("mn_select_installation %i;", installationCurrent->idx));
			return;
		}
	} else {
		if (r_geoscape_overlay->integer & OVERLAY_RADAR)
			MAP_SetOverlay("radar");
		if (ccs.mapAction == MA_NEWINSTALLATION)
			ccs.mapAction = MA_NONE;

		Q_strncpyz(popupText, _("Not enough credits to set up a new installation."), sizeof(popupText));
		MN_Popup(_("Notice"), popupText);
	}
}

/**
 * @brief Called when an installation is opened or a new installation is created on geoscape.
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

	if (installationID >= 0 && installationID < ccs.numInstallations)
		installation = INS_GetFoundedInstallationByIDX(installationID);
	else
		/* create a new base */
		installation = NULL;
	INS_SelectInstallation(installation);
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
 * @brief Sets the title of the installation.
 */
static void INS_SetInstallationTitle_f (void)
{
	Com_DPrintf(DEBUG_CLIENT, "INS_SetInstallationTitle_f: #installations: %i\n", ccs.numInstallations);
	if (ccs.numInstallations < B_GetInstallationLimit())
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
		if (!installation) {
			Com_DPrintf(DEBUG_CLIENT, "Installation not founded (idx %i)\n", atoi(Cmd_Argv(1)));
			return;
		}
		Cvar_SetValue("mn_installation_id", installation->idx);
	}

	/* Ask 'Are you sure?' by default */
	if (Cmd_Argc() < 3) {
		char command[MAX_VAR];

		Com_sprintf(command, sizeof(command), "mn_destroyinstallation %d 1; mn_pop;", installation->idx);
		MN_PopupButton(_("Destroy Installation"), _("Do you really want to destroy this installation?"),
			command, _("Destroy"), _("Destroy installation"),
			"mn_pop;", _("Cancel"), _("Forget it"),
			NULL, NULL, NULL);
		return;
	}
	INS_DestroyInstallation(installation);
	Cvar_Set("mn_installation_count", va("%i", ccs.numInstallations));
}


/**
 * @brief upadtes the installation limit cvar for menus
 */
static void INS_UpdateInsatallationLimit_f (void)
{
	Cvar_SetValue("mn_installation_max", B_GetInstallationLimit());
}

void INS_InitCallbacks (void)
{
	Cmd_AddCommand("mn_select_installation", INS_SelectInstallation_f, "Parameter is the installation index. -1 will build a new one.");
	Cmd_AddCommand("mn_build_installation", INS_BuildInstallation_f, NULL);
	Cmd_AddCommand("mn_set_installation_title", INS_SetInstallationTitle_f, NULL);
	Cmd_AddCommand("mn_rename_installation", INS_RenameInstallation_f, "Rename the current installation");
	Cmd_AddCommand("mn_installation_changename", INS_ChangeInstallationName_f, "Called after editing the cvar installation name");
	Cmd_AddCommand("mn_destroyinstallation", INS_DestroyInstallation_f, "Destroys an installation");
	Cmd_AddCommand("mn_update_max_installations", INS_UpdateInsatallationLimit_f, "Updates the installation count limit");

	INS_UpdateInsatallationLimit_f();
	mn_installation_count = Cvar_Get("mn_installation_count", "0", 0, "Current amount of build installations");
	mn_installation_id = Cvar_Get("mn_installation_id", "-1", 0, "Internal id of the current selected installation");
	Cvar_Set("mn_installation_title", "");
}

/** @todo unify the names into mn_base_* */
void INS_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("mn_select_installation");
	Cmd_RemoveCommand("mn_build_installation");
	Cmd_RemoveCommand("mn_set_installation_title");
	Cmd_RemoveCommand("mn_rename_installation");
	Cmd_RemoveCommand("mn_installation_changename");
	Cmd_RemoveCommand("mn_destroyinstallation");
	Cmd_RemoveCommand("mn_update_max_installations");
	Cvar_Delete("mn_installation_title");
}
