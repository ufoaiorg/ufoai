/**
 * @file
 * @brief Menu related console command callbacks
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "../../cl_shared.h"
#include "../../ui/ui_dataids.h"
#include "cp_campaign.h"
#include "cp_installation_callbacks.h"
#include "cp_installation.h"
#include "cp_geoscape.h"
#include "cp_popup.h"
#include "cp_ufo.h"
#include "cp_uforecovery_callbacks.h"

/**
 * @brief Sets the title of the installation to a cvar to prepare the rename menu.
 * @note it also assigns description text
 */
static void INS_SetInstallationTitle (installationType_t type)
{
	const installationTemplate_t* insTemp = INS_GetInstallationTemplateByType(type);
	cgi->Cvar_Set("mn_installation_title", "%s #%i", (insTemp) ? _(insTemp->name) : _("Installation"), ccs.campaignStats.installationsBuilt + 1);
	if (!insTemp || !Q_strvalid(insTemp->description))
		cgi->UI_ResetData(TEXT_BUILDING_INFO);
	else
		cgi->UI_RegisterText(TEXT_BUILDING_INFO, _(insTemp->description));
}

/**
 * @brief Select an installation when clicking on it on geoscape
 * @param[in] installation The installation to select
 * @note This is (and should be) the only place where installationCurrent is set
 * to a value that is not @c nullptr
 */
void INS_SelectInstallation (installation_t* installation)
{
	const int timetobuild = std::max(0, installation->installationTemplate->buildTime - (ccs.date.day - installation->buildStart));

	Com_DPrintf(DEBUG_CLIENT, "INS_SelectInstallation: select installation with id %i\n", installation->idx);
	ccs.mapAction = MA_NONE;
	if (installation->installationStatus == INSTALLATION_WORKING) {
		cgi->Cvar_Set("mn_installation_timetobuild", "-");
	} else {
		cgi->Cvar_Set("mn_installation_timetobuild", ngettext("%d day", "%d days", timetobuild), timetobuild);
	}
	INS_SetCurrentSelectedInstallation(installation);

	switch (installation->installationTemplate->type) {
	case INSTALLATION_UFOYARD:
		cgi->UI_PushWindow("popup_ufoyards");
		break;
	case INSTALLATION_DEFENCE:
		cgi->UI_PushWindow("basedefence");
		break;
	default:
		cgi->UI_PushWindow("popup_installationstatus");
		break;
	}
}

/**
 * @brief Constructs a new installation.
 */
static void INS_BuildInstallation_f (void)
{
	const installationTemplate_t* installationTemplate;

	if (cgi->Cmd_Argc() < 1) {
		Com_Printf("Usage: %s <installationType>\n", cgi->Cmd_Argv(0));
		return;
	}

	/* We shouldn't build more installations than the actual limit */
	if (B_GetInstallationLimit() <= INS_GetCount())
		return;

	installationTemplate = INS_GetInstallationTemplateByID(cgi->Cmd_Argv(1));
	if (!installationTemplate) {
		Com_Printf("The installation type %s passed for %s is not valid.\n", cgi->Cmd_Argv(1), cgi->Cmd_Argv(0));
		return;
	}

	assert(installationTemplate->cost >= 0);

	if (ccs.credits - installationTemplate->cost > 0) {
		/* set up the installation */
		installation_t* installation = INS_Build(installationTemplate, ccs.newBasePos, cgi->Cvar_GetString("mn_installation_title"));

		CP_UpdateCredits(ccs.credits - installationTemplate->cost);
		/* this cvar is used for disabling the installation build button on geoscape if MAX_INSTALLATIONS was reached */
		cgi->Cvar_SetValue("mn_installation_count", INS_GetCount());

		const nation_t* nation = GEO_GetNation(installation->pos);
		if (nation)
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("A new installation has been built: %s (nation: %s)"), installation->name, _(nation->name));
		else
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("A new installation has been built: %s"), installation->name);
		MSO_CheckAddNewMessage(NT_INSTALLATION_BUILDSTART, _("Installation building"), cp_messageBuffer, MSG_CONSTRUCTION);
	} else {
		if (installationTemplate->type == INSTALLATION_RADAR) {
			if (GEO_IsRadarOverlayActivated())
					GEO_SetOverlay("radar", 1);
		}
		if (ccs.mapAction == MA_NEWINSTALLATION)
			ccs.mapAction = MA_NONE;

		CP_Popup(_("Notice"), _("Not enough credits to set up a new installation."));
	}
	ccs.mapAction = MA_NONE;
}

/**
 * @brief Called when an installation is opened or a new installation is created on geoscape.
 * For a new installation the installationID is -1.
 */
static void INS_SelectInstallation_f (void)
{
	int installationID;
	installation_t* installation;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <installationID>\n", cgi->Cmd_Argv(0));
		return;
	}
	installationID = atoi(cgi->Cmd_Argv(1));

	installation = INS_GetByIDX(installationID);
	if (installation != nullptr)
		INS_SelectInstallation(installation);
}

/**
 * @brief Creates console command to change the name of a installation.
 * Copies the value of the cvar mn_installation_title over as the name of the
 * current selected installation
 */
static void INS_ChangeInstallationName_f (void)
{
	installation_t* installation = INS_GetCurrentSelectedInstallation();

	/* maybe called without installation initialized or active */
	if (!installation)
		return;

	Q_strncpyz(installation->name, cgi->Cvar_GetString("mn_installation_title"), sizeof(installation->name));
}

/**
 * @brief console function for destroying an installation
 * @sa INS_DestroyInstallation
 */
static void INS_DestroyInstallation_f (void)
{
	installation_t* installation;

	if (cgi->Cmd_Argc() < 2 || atoi(cgi->Cmd_Argv(1)) < 0) {
		installation = INS_GetCurrentSelectedInstallation();
	} else {
		installation = INS_GetByIDX(atoi(cgi->Cmd_Argv(1)));
		if (!installation) {
			Com_DPrintf(DEBUG_CLIENT, "Installation not founded (idx %i)\n", atoi(cgi->Cmd_Argv(1)));
			return;
		}
	}

	/* Ask 'Are you sure?' by default */
	if (cgi->Cmd_Argc() < 3 || !atoi(cgi->Cmd_Argv(2))) {
		char command[MAX_VAR];

		Com_sprintf(command, sizeof(command), "mn_installation_destroy %d 1; ui_pop;", installation->idx);
		cgi->UI_PopupButton(_("Destroy Installation"), _("Do you really want to destroy this installation?"),
			command, _("Destroy"), _("Destroy installation"),
			"ui_pop;", _("Cancel"), _("Forget it"),
			nullptr, nullptr, nullptr);
		return;
	}
	INS_DestroyInstallation(installation);
}

/**
 * @brief updates the installation limit cvar for menus
 */
static void INS_UpdateInstallationLimit_f (void)
{
	cgi->Cvar_SetValue("mn_installation_max", B_GetInstallationLimit());
}

/**
 * @brief Fills the UI with ufo yard data
 */
static void INS_FillUFOYardData_f (void)
{
	installation_t* ins;

	cgi->UI_ExecuteConfunc("ufolist_clear");
	if (cgi->Cmd_Argc() < 2 || atoi(cgi->Cmd_Argv(1)) < 0) {
		ins = INS_GetCurrentSelectedInstallation();
		if (!ins || ins->installationTemplate->type != INSTALLATION_UFOYARD)
			ins = INS_GetFirstUFOYard(false);
	} else {
		ins = INS_GetByIDX(atoi(cgi->Cmd_Argv(1)));
		if (!ins)
			Com_DPrintf(DEBUG_CLIENT, "Installation not founded (idx %i)\n", atoi(cgi->Cmd_Argv(1)));
	}

	if (ins) {
		const nation_t* nat = GEO_GetNation(ins->pos);
		const int timeToBuild = std::max(0, ins->installationTemplate->buildTime - (ccs.date.day - ins->buildStart));
		const char* buildTime = (timeToBuild > 0 && ins->installationStatus == INSTALLATION_UNDER_CONSTRUCTION) ? va(ngettext("%d day", "%d days", timeToBuild), timeToBuild) : "-";
		const int freeCap = std::max(0, ins->ufoCapacity.max - ins->ufoCapacity.cur);
		const char* nationName = nat ? _(nat->name) : "";

		cgi->UI_ExecuteConfunc("ufolist_addufoyard %d \"%s\" \"%s\" %d %d \"%s\"", ins->idx, ins->name, nationName, ins->ufoCapacity.max, freeCap, buildTime);

		US_Foreach(ufo) {
			if (ufo->installation != ins)
				continue;

			const char* ufoName = UFO_GetName(ufo->ufoTemplate);
			const char* condition = va(_("Condition: %3.0f%%"), ufo->condition * 100);
			const char* status = US_StoredUFOStatus(ufo);
			cgi->UI_ExecuteConfunc("ufolist_addufo %d \"%s\" \"%s\" \"%s\" \"%s\"", ufo->idx, ufoName, condition, ufo->ufoTemplate->model, status);
		}
	}
}

/**
 * @brief Fills create installation / installation type selection popup
 */
static void INS_FillTypes_f (void)
{
	cgi->UI_ExecuteConfunc("installationtype_clear");
	if (INS_GetCount() < B_GetInstallationLimit()) {
		for (int i = 0; i < ccs.numInstallationTemplates; i++) {
			const installationTemplate_t* tpl = &ccs.installationTemplates[i];
			if (tpl->once && INS_HasType(tpl->type, INSTALLATION_NOT_USED))
				continue;
			if (tpl->tech == nullptr || RS_IsResearched_ptr(tpl->tech)) {
				cgi->UI_ExecuteConfunc("installationtype_add \"%s\" \"%s\" \"%s\" \"%d c\"", tpl->id, _(tpl->name),
					(tpl->buildTime > 0) ? va("%d %s", tpl->buildTime, ngettext("day", "days", tpl->buildTime)) : "-", tpl->cost);
			}
		}
	}

	/** @todo Move this out from installations code */
	if (B_GetCount() < MAX_BASES)
		cgi->UI_ExecuteConfunc("installationtype_add base \"%s\" - \"%d c\"", _("Base"), ccs.curCampaign->basecost);
}

/**
 * @brief Selects installation type to build
 */
static void INS_SelectType_f (void)
{
	if (cgi->Cmd_Argc() < 2)
		return;

	const char* id = cgi->Cmd_Argv(1);

	if (ccs.mapAction == MA_NEWINSTALLATION) {
		GEO_ResetAction();
		return;
	}

	const installationTemplate_t* tpl = INS_GetInstallationTemplateByID(id);
	if (!tpl) {
		Com_Printf("Invalid installation template\n");
		return;
	}

	if (INS_GetCount() >= B_GetInstallationLimit()) {
		Com_Printf("Maximum number of installations reached\n");
		return;
	}

	if (tpl->tech != nullptr && !RS_IsResearched_ptr(tpl->tech)) {
		Com_Printf("This type of installation is not yet researched\n");
		return;
	}

	if (tpl->once && INS_HasType(tpl->type, INSTALLATION_NOT_USED)) {
		Com_Printf("Cannot build more of this installation\n");
		return;
	}

	ccs.mapAction = MA_NEWINSTALLATION;

	/* show radar overlay (if not already displayed) */
	if (tpl->type == INSTALLATION_RADAR && !GEO_IsRadarOverlayActivated())
		GEO_SetOverlay("radar", 1);

	INS_SetInstallationTitle(tpl->type);
	cgi->Cvar_Set("mn_installation_type", "%s", tpl->id);
}

static const cmdList_t installationCallbacks[] = {
	{"mn_installation_select", INS_SelectInstallation_f, "Parameter is the installation index. -1 will build a new one."},
	{"mn_installation_build", INS_BuildInstallation_f, nullptr},
	{"mn_installation_changename", INS_ChangeInstallationName_f, "Called after editing the cvar installation name"},
	{"mn_installation_destroy", INS_DestroyInstallation_f, "Destroys an installation"},
	{"mn_installation_update_max_count", INS_UpdateInstallationLimit_f, "Updates the installation count limit"},
	{"ui_fill_installationtypes", INS_FillTypes_f, "Fills create installation / installation type selection popup"},
	{"ui_build_installationtype", INS_SelectType_f, "Selects installation type to build"},
	{"ui_fillufoyards", INS_FillUFOYardData_f, "Fills UFOYard UI with data"},
	{nullptr, nullptr, nullptr}
};
void INS_InitCallbacks (void)
{
	cgi->Cmd_TableAddList(installationCallbacks);

	cgi->Cvar_SetValue("mn_installation_count", INS_GetCount());
	cgi->Cvar_Set("mn_installation_title", "");
	cgi->Cvar_Set("mn_installation_type", "");
	cgi->Cvar_Set("mn_installation_max", "");
}

void INS_ShutdownCallbacks (void)
{
	cgi->Cmd_TableRemoveList(installationCallbacks);

	cgi->Cvar_Delete("mn_installation_count");
	cgi->Cvar_Delete("mn_installation_title");
	cgi->Cvar_Delete("mn_installation_max");
	cgi->Cvar_Delete("mn_installation_type");
}
