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

/**
 * @brief Select an installation when clicking on it on geoscape, or build a new installation.
 * @param[in] installation If this is @c NULL we want to build a new installation
 * @note This is (and should be) the only place where installationCurrent is set
 * to a value that is not @c NULL
 */
void INS_SelectInstallation (installation_t *installation)
{
	/* set up a new installation */
	if (!installation) {
		/* if player hit the "create base" button while creating base mode is enabled
		 * that means that player wants to quit this mode */
		if (ccs.mapAction == MA_NEWINSTALLATION || ccs.numInstallations >= B_GetInstallationLimit()) {
			MAP_ResetAction();
			if (!radarOverlayWasSet)
				MAP_DeactivateOverlay("radar");
			return;
		} else {
			ccs.mapAction = MA_NEWINSTALLATION;

			/* show radar overlay (if not already displayed) */
			if (!(r_geoscape_overlay->integer & OVERLAY_RADAR))
				MAP_SetOverlay("radar");
		}
	} else {
		const int timetobuild = max(0, installation->installationTemplate->buildTime - (ccs.date.day - installation->buildStart));

		Com_DPrintf(DEBUG_CLIENT, "INS_SelectInstallation: select installation with id %i\n", installation->idx);
		ccs.mapAction = MA_NONE;
		if (installation->installationStatus == INSTALLATION_WORKING) {
			Cvar_Set("mn_installation_timetobuild", "-");
		} else {
			Cvar_Set("mn_installation_timetobuild", va(ngettext("%d day", "%d days", timetobuild), timetobuild));
		}
		MN_PushMenu("popup_installationstatus", NULL);

		INS_SetCurrentSelectedInstallation(installation);
	}
}

/**
 * @brief Constructs a new installation.
 * @sa CL_NewInstallation
 */
static void INS_BuildInstallation_f (void)
{
	const nation_t *nation;
	int i = 1;
	int j;
	installationTemplate_t *installationTemplate;
	installation_t *installation;

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

	installation = INS_GetFirstUnfoundedInstallation();
	if (!installation)
		return;

	do {
		j = 0;
		Com_sprintf(installation->name, sizeof(installation->name), "%s #%i", installationTemplate ? _(installationTemplate->name) : _("Installation"), i);
		while (j <= ccs.numInstallations && strcmp(installation->name, ccs.installations[j++].name));
	} while (i++ <= ccs.numInstallations && j <= ccs.numInstallations);

	assert(!installation->founded);
	assert(installationTemplate->cost >= 0);

	if (ccs.credits - installationTemplate->cost > 0) {
		/** @todo If there is no nation assigned to the current selected position,
		 * tell this the gamer and give him an option to rechoose the location.
		 * If we don't do this, any action that is done for this installation has no
		 * influence to any nation happiness/funding/supporting */
		if (INS_NewInstallation(installation, installationTemplate, newInstallationPos)) {
			Com_DPrintf(DEBUG_CLIENT, "INS_BuildInstallation_f: numInstallations: %i\n", ccs.numInstallations);

			/* set up the installation */
			INS_SetUpInstallation(installation, installationTemplate);

			ccs.campaignStats.installationsBuild++;
			ccs.mapAction = MA_NONE;
			CL_UpdateCredits(ccs.credits - installationTemplate->cost);
			Q_strncpyz(installation->name, Cvar_GetString("mn_installation_title"), sizeof(installation->name));
			nation = MAP_GetNation(installation->pos);
			if (nation)
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("A new installation has been built: %s (nation: %s)"), installation->name, _(nation->name));
			else
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("A new installation has been built: %s"), installation->name);
			MSO_CheckAddNewMessage(NT_INSTALLATION_BUILDSTART, _("Installation building"), cp_messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);

			Cbuf_AddText(va("mn_select_installation %i;", installation->idx));
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
 * @brief Sets the title of the installation to a cvar to prepare the rename menu.
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
	installation_t *installation = INS_GetCurrentSelectedInstallation();

	/* maybe called without installation initialized or active */
	if (!installation)
		return;

	Q_strncpyz(installation->name, Cvar_GetString("mn_installation_title"), sizeof(installation->name));
}

/**
 * @brief console function for destroying an installation
 * @sa INS_DestroyInstallation
 */
static void INS_DestroyInstallation_f (void)
{
	installation_t *installation;

	if (Cmd_Argc() < 2 || atoi(Cmd_Argv(1)) < 0) {
		installation = INS_GetCurrentSelectedInstallation();
	} else {
		installation = INS_GetFoundedInstallationByIDX(atoi(Cmd_Argv(1)));
		if (!installation) {
			Com_DPrintf(DEBUG_CLIENT, "Installation not founded (idx %i)\n", atoi(Cmd_Argv(1)));
			return;
		}
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
	Cmd_AddCommand("mn_installation_changename", INS_ChangeInstallationName_f, "Called after editing the cvar installation name");
	Cmd_AddCommand("mn_destroyinstallation", INS_DestroyInstallation_f, "Destroys an installation");
	Cmd_AddCommand("mn_update_max_installations", INS_UpdateInsatallationLimit_f, "Updates the installation count limit");

	INS_UpdateInsatallationLimit_f();
	Cvar_Set("mn_installation_count", "0");
	Cvar_Set("mn_installation_title", "");
}

/** @todo unify the names into mn_base_* */
void INS_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("mn_select_installation");
	Cmd_RemoveCommand("mn_build_installation");
	Cmd_RemoveCommand("mn_set_installation_title");
	Cmd_RemoveCommand("mn_installation_changename");
	Cmd_RemoveCommand("mn_destroyinstallation");
	Cmd_RemoveCommand("mn_update_max_installations");
	Cvar_Delete("mn_installation_count");
	Cvar_Delete("mn_installation_title");
}
