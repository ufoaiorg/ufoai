/**
 * @file
 * @brief Handles everything that is located in or accessed through an installation.
 * @note Installation functions prefix: INS_*
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
#include "../../../shared/parse.h"
#include "cp_campaign.h"
#include "cp_mapfightequip.h"
#include "cp_aircraft.h"
#include "cp_missions.h"
#include "cp_installation.h"
#include "save/save_installation.h"

/**
 * @brief Get number of installations
 */
int INS_GetCount (void)
{
	return cgi->LIST_Count(ccs.installations);
}

installationType_t INS_GetType (const char* type)
{
	if (Q_streq(type, "samsite"))
		return INSTALLATION_DEFENCE;
	else if (Q_streq(type, "ufoyard"))
		return INSTALLATION_UFOYARD;
	else if (Q_streq(type, "radartower"))
		return INSTALLATION_RADAR;
	else if (Q_streq(type, "orbit"))
		return INSTALLATION_ORBIT;

	Com_Printf("unknown type given '%s'\n", type);
	return INSTALLATION_RADAR;
}

/**
 * @brief Checks whether any installation is available
 * @param[in] status Status of installation to search for
 */
bool INS_HasAny (installationStatus_t status)
{
	INS_Foreach(installation) {
		if (status == INSTALLATION_NOT_USED || installation->installationStatus == status)
			return true;
	}

	return false;
}

/**
 * @brief Checks whether the given installation type is available
 * @param[in] type Installation type to search for
 * @param[in] status Status of installation to search for
 */
bool INS_HasType (installationType_t type, installationStatus_t status)
{
	INS_ForeachOfType(installation, type) {
		if (status == INSTALLATION_NOT_USED || installation->installationStatus == status)
			return true;
	}

	return false;
}

/**
 * @brief Get installation by it's index.
 * @param[in] idx Instalation's index
 * @return Pointer to the installation corresponding to idx or @c nullptr if not found.
 */
installation_t* INS_GetByIDX (int idx)
{
	INS_Foreach(installation) {
		if (installation->idx == idx)
			return installation;
	}

	return nullptr;
}

/**
 * @brief Returns the installation Template for a given installation ID.
 * @param[in] id ID of the installation template to find.
 * @return corresponding installation Template, @c nullptr if not found.
 */
const installationTemplate_t* INS_GetInstallationTemplateByID (const char* id)
{
	int idx;

	for (idx = 0; idx < ccs.numInstallationTemplates; idx++) {
		const installationTemplate_t* t = &ccs.installationTemplates[idx];
		if (Q_streq(t->id, id))
			return t;
	}

	return nullptr;
}

/**
 * @brief Returns the installation Template for a given installation type.
 * @param[in] type Type of the installation template to find.
 * @return corresponding installation Template, @c nullptr if not found.
 */
const installationTemplate_t* INS_GetInstallationTemplateByType (installationType_t type)
{
	int idx;

	for (idx = 0; idx < ccs.numInstallationTemplates; idx++) {
		const installationTemplate_t* t = &ccs.installationTemplates[idx];
		if (t->type == type)
			return t;
	}

	return nullptr;
}

/**
 * @brief Build a new installation
 * @param[in] installationTemplate Template pointer
 * @param[in] pos Position on Globe to build at
 * @param[in] name The name of the installation - might already be in utf-8
 */
installation_t* INS_Build (const installationTemplate_t* installationTemplate, const vec2_t pos, const char* name)
{
	installation_t installation;
	const int newInstallationAlienInterest = 1.0f;

	OBJZERO(installation);

	Vector2Copy(pos, installation.pos);
	Q_strncpyz(installation.name, name, sizeof(installation.name));
	installation.idx = ccs.campaignStats.installationsBuilt;
	installation.installationStatus = INSTALLATION_UNDER_CONSTRUCTION;
	installation.installationTemplate = installationTemplate;
	installation.buildStart = ccs.date.day;

	/* a new installation is not discovered (yet) */
	installation.alienInterest = newInstallationAlienInterest;

	/* intialise hit points */
	installation.installationDamage = installation.installationTemplate->maxDamage;

	/* Reset Radar */
	RADAR_Initialise(&(installation.radar), 0.0f, 0.0f, 0.0f, false);

	ccs.campaignStats.installationsBuilt++;
	return &LIST_Add(&ccs.installations, installation);
}

/**
 * @brief Destroys an installation
 * @param[in,out] installation Pointer to the installation to be destroyed
 */
void INS_DestroyInstallation (installation_t* installation)
{
	if (!installation)
		return;
	/* Disable radar */
	RADAR_UpdateInstallationRadarCoverage(installation, 0, 0);
	/* Destroy stored UFOs */
	if (installation->ufoCapacity.max > 0) {
		installation->ufoCapacity.max = 0;
		US_RemoveUFOsExceedingCapacity(installation);
	}
	CP_MissionNotifyInstallationDestroyed(installation);

	Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Installation %s was destroyed."), installation->name);
	MSO_CheckAddNewMessage(NT_INSTALLATION_DESTROY, _("Installation destroyed"), cp_messageBuffer, MSG_CONSTRUCTION);

	cgi->LIST_Remove(&ccs.installations, installation);
	cgi->Cvar_Set("mn_installation_count", "%i", INS_GetCount());
}

/**
 * @brief Returns the current selected installation
 */
installation_t* INS_GetCurrentSelectedInstallation (void)
{
	INS_Foreach(installation) {
		if (installation->selected)
			return installation;
	}

	return nullptr;
}

/**
 * @brief Sets the currently selected installation
 * @param installation Pointer to the installation to select
 * @sa INS_SelectInstallation
 */
void INS_SetCurrentSelectedInstallation (const installation_t* installation)
{
	INS_Foreach(ins)
		ins->selected = (ins == installation);

	if (installation) {
		B_SetCurrentSelectedBase(nullptr);
		cgi->Cvar_Set("mn_installation_title", "%s", installation->name);
		cgi->Cvar_Set("mn_installation_type", "%s", installation->installationTemplate->id);
	} else {
		cgi->Cvar_Set("mn_installation_title", "");
		cgi->Cvar_Set("mn_installation_type", "");
	}
}

/**
 * @brief Finishes an installation
 * @param[in,out] installation Pointer to the installation to be finished
 * @sa INS_UpdateInstallationData
 * @sa INS_ConstructionFinished_f
 */
static void INS_FinishInstallation (installation_t* installation)
{
	if (!installation)
		cgi->Com_Error(ERR_DROP, "INS_FinishInstallation: No Installation.\n");
	if (!installation->installationTemplate)
		cgi->Com_Error(ERR_DROP, "INS_FinishInstallation: No Installation template.\n");
	if (installation->installationStatus != INSTALLATION_UNDER_CONSTRUCTION) {
		Com_DPrintf(DEBUG_CLIENT, "INS_FinishInstallation: Installation is not being built.\n");
		return;
	}

	installation->installationStatus = INSTALLATION_WORKING;
	/* if Radar Tower */
	RADAR_UpdateInstallationRadarCoverage(installation, installation->installationTemplate->radarRange, installation->installationTemplate->trackingRange);
	/* if SAM Site */
	installation->numBatteries = installation->installationTemplate->maxBatteries;
	BDEF_InitialiseInstallationSlots(installation);
	/* if UFO Yard */
	installation->ufoCapacity.max = installation->installationTemplate->maxUFOsStored;
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
	INS_Foreach(installation) {
		Com_Printf("Installation idx %i\n", installation->idx);
		Com_Printf("Installation name %s\n", installation->name);
		Com_Printf("Installation pos %.02f:%.02f\n", installation->pos[0], installation->pos[1]);
		Com_Printf("Installation Alien interest %f\n", installation->alienInterest);

		Com_Printf("\nInstallation sensorWidth %i\n", installation->radar.range);
		Com_Printf("\nInstallation trackingWidth %i\n", installation->radar.trackingRange);
		Com_Printf("Installation numSensoredAircraft %i\n\n", installation->radar.numUFOs);

		Com_Printf("\nInstallation numBatteries %i\n", installation->numBatteries);
		/** @todo list batteries */

		Com_Printf("\nInstallation stored UFOs %i/%i\n", installation->ufoCapacity.cur, installation->ufoCapacity.max);
		/** @todo list stored Ufos*/

		Com_Printf("\n\n");
	}
}

/**
 * @brief Finishes the construction of an/all installation(s)
 */
static void INS_ConstructionFinished_f (void)
{
	int idx = -1;

	if (cgi->Cmd_Argc() == 2) {
		idx = atoi(cgi->Cmd_Argv(1));
		if (idx < 0) {
			Com_Printf("Usage: %s [installationIDX]\nWithout parameter it builds up all.\n", cgi->Cmd_Argv(0));
			return;
		}
	}

	INS_Foreach(ins) {
		if (idx >= 0 && ins->idx != idx)
			continue;

		INS_FinishInstallation(ins);
	}
}
#endif

/**
 * @brief returns the first installation with (free) ufostoring capacity
 * @param[in] free On true it gives the first UFO Yard with free space
 * @return installation_t Pointer to the UFO Yard
 */
installation_t* INS_GetFirstUFOYard (bool free)
{
	INS_ForeachOfType(installation, INSTALLATION_UFOYARD) {
		if (free && installation->ufoCapacity.cur >= installation->ufoCapacity.max)
			continue;

		return installation;
	}

	return nullptr;
}

static const cmdList_t debugInstallationCmds[] = {
#ifdef DEBUG
	{"debug_listinstallation", INS_InstallationList_f, "Print installation information to the game console"},
	{"debug_finishinstallation", INS_ConstructionFinished_f, "Finish construction of a specified or every installation"},
#endif
	{nullptr, nullptr, nullptr}
};
/**
 * @brief Resets console commands.
 */
void INS_InitStartup (void)
{
	cgi->Cmd_TableAddList(debugInstallationCmds);
}

/**
 * @brief Closing operations for installations subsystem
 */
void INS_Shutdown (void)
{
	cgi->LIST_Delete(&ccs.installations);
	cgi->Cmd_TableRemoveList(debugInstallationCmds);
}

/**
 * @brief Check if some installation are build.
 * @note Daily called.
 */
void INS_UpdateInstallationData (void)
{
	INS_Foreach(installation) {
		if (installation->installationStatus == INSTALLATION_UNDER_CONSTRUCTION
		 && installation->buildStart
		 && installation->buildStart + installation->installationTemplate->buildTime <= ccs.date.day) {
			INS_FinishInstallation(installation);

			Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer), _("Construction of installation %s finished."), installation->name);
			MSO_CheckAddNewMessage(NT_INSTALLATION_BUILDFINISH, _("Installation finished"), cp_messageBuffer, MSG_CONSTRUCTION);
		}
	}
}

static const value_t installation_vals[] = {
	{"name", V_TRANSLATION_STRING, offsetof(installationTemplate_t, name), 0},
	{"description", V_TRANSLATION_STRING, offsetof(installationTemplate_t, description), 0},
	{"radar_range", V_INT, offsetof(installationTemplate_t, radarRange), MEMBER_SIZEOF(installationTemplate_t, radarRange)},
	{"radar_tracking_range", V_INT, offsetof(installationTemplate_t, trackingRange), MEMBER_SIZEOF(installationTemplate_t, trackingRange)},
	{"max_batteries", V_INT, offsetof(installationTemplate_t, maxBatteries), MEMBER_SIZEOF(installationTemplate_t, maxBatteries)},
	{"max_ufo_stored", V_INT, offsetof(installationTemplate_t, maxUFOsStored), MEMBER_SIZEOF(installationTemplate_t, maxUFOsStored)},
	{"max_damage", V_INT, offsetof(installationTemplate_t, maxDamage), MEMBER_SIZEOF(installationTemplate_t, maxDamage)},
	{"cost", V_INT, offsetof(installationTemplate_t, cost), MEMBER_SIZEOF(installationTemplate_t, cost)},
	{"buildtime", V_INT, offsetof(installationTemplate_t, buildTime), MEMBER_SIZEOF(installationTemplate_t, buildTime)},
	{"once", V_BOOL, offsetof(installationTemplate_t, once), MEMBER_SIZEOF(installationTemplate_t, once)},
	{"model", V_HUNK_STRING, offsetof(installationTemplate_t, model), 0},
	{"image", V_HUNK_STRING, offsetof(installationTemplate_t, image), 0},

	{nullptr, V_NULL, 0, 0}
};

/**
 * @brief Copies an entry from the installation description file into the list of installation templates.
 * @note Parses one "installation" entry in the installation.ufo file and writes
 * it into the next free entry in installationTemplates.
 * @param[in] name Unique test-id of a installationTemplate_t.
 * @param[in] text the rest of the script file that is tokenized here
 */
void INS_ParseInstallations (const char* name, const char** text)
{
	/* get id list body */
	const char* token = Com_Parse(text);
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

	for (int i = 0; i < ccs.numInstallationTemplates; i++) {
		if (Q_streq(ccs.installationTemplates[i].name, name)) {
			Com_Printf("INS_ParseInstallations: Second installation with same name found (%s) - second ignored\n", name);
			return;
		}
	}

	/* new entry */
	installationTemplate_t* installation = &ccs.installationTemplates[ccs.numInstallationTemplates];
	OBJZERO(*installation);
	installation->id = cgi->PoolStrDup(name, cp_campaignPool, 0);
	installation->type = INSTALLATION_RADAR;

	Com_DPrintf(DEBUG_CLIENT, "...found installation %s\n", installation->id);

	ccs.numInstallationTemplates++;
	const char* errhead = "INS_ParseInstallations: unexpected end of file (names ";
	do {
		/* get the name type */
		token = cgi->Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* check for some standard values */
		if (!cgi->Com_ParseBlockToken(name, text, installation, installation_vals, cp_campaignPool, token)) {
			/* other values */
			if (Q_streq(token, "type")) {
				token = cgi->Com_EParse(text, errhead, name);
				if (!*text)
					return;

				installation->type = INS_GetType(token);
			}
		}
	} while (*text);
}

void INS_LinkTechnologies (void)
{
	for (int i = 0; i < ccs.numInstallationTemplates; i++) {
		installationTemplate_t* ins = &ccs.installationTemplates[i];
		technology_t* techLink = RS_GetTechByProvided(ins->id);
		if (techLink)
			ins->tech = techLink;
	}
}

/**
 * @brief Save callback for savegames in xml
 * @param[out] p XML Node structure, where we write the information to
 * @sa INS_LoadXML
 * @sa SAV_GameSaveXML
 */
bool INS_SaveXML (xmlNode_t* p)
{
	xmlNode_t* n = cgi->XML_AddNode(p, SAVE_INSTALLATION_INSTALLATIONS);
	cgi->Com_RegisterConstList(saveInstallationConstants);
	INS_Foreach(inst) {
		xmlNode_t* s, *ss;

		s = cgi->XML_AddNode(n, SAVE_INSTALLATION_INSTALLATION);
		cgi->XML_AddString(s, SAVE_INSTALLATION_TEMPLATEID, inst->installationTemplate->id);
		cgi->XML_AddInt(s, SAVE_INSTALLATION_IDX, inst->idx);
		cgi->XML_AddString(s, SAVE_INSTALLATION_NAME, inst->name);
		cgi->XML_AddPos3(s, SAVE_INSTALLATION_POS, inst->pos);
		cgi->XML_AddString(s, SAVE_INSTALLATION_STATUS, cgi->Com_GetConstVariable(SAVE_INSTALLATIONSTATUS_NAMESPACE, inst->installationStatus));
		cgi->XML_AddInt(s, SAVE_INSTALLATION_DAMAGE, inst->installationDamage);
		cgi->XML_AddFloat(s, SAVE_INSTALLATION_ALIENINTEREST, inst->alienInterest);
		cgi->XML_AddInt(s, SAVE_INSTALLATION_BUILDSTART, inst->buildStart);

		ss = cgi->XML_AddNode(s, SAVE_INSTALLATION_BATTERIES);
		cgi->XML_AddIntValue(ss, SAVE_INSTALLATION_NUM, inst->numBatteries);
		B_SaveBaseSlotsXML(inst->batteries, inst->numBatteries, ss);
	}
	cgi->Com_UnregisterConstList(saveInstallationConstants);
	return true;
}

/**
 * @brief Load callback for savegames
 * @param[in] p XML Node structure, where we get the information from
 * @sa INS_SaveXML
 * @sa SAV_GameLoadXML
 * @sa INS_LoadItemSlots
 */
bool INS_LoadXML (xmlNode_t* p)
{
	xmlNode_t* n = cgi->XML_GetNode(p, SAVE_INSTALLATION_INSTALLATIONS);
	xmlNode_t* s;
	bool success = true;

	if (!n)
		return false;

	cgi->Com_RegisterConstList(saveInstallationConstants);
	for (s = cgi->XML_GetNode(n, SAVE_INSTALLATION_INSTALLATION); s; s = cgi->XML_GetNextNode(s, n, SAVE_INSTALLATION_INSTALLATION)) {
		xmlNode_t* ss;
		installation_t inst;
		const char* instID = cgi->XML_GetString(s, SAVE_INSTALLATION_TEMPLATEID);
		const char* instStat = cgi->XML_GetString(s, SAVE_INSTALLATION_STATUS);

		OBJZERO(inst);
		inst.idx = cgi->XML_GetInt(s, SAVE_INSTALLATION_IDX, -1);
		if (inst.idx < 0) {
			Com_Printf("Invalid installation index %i\n", inst.idx);
			success = false;
			break;
		}
		const installationType_t type = INS_GetType(instID);
		inst.installationTemplate = INS_GetInstallationTemplateByType(type);
		if (!inst.installationTemplate) {
			Com_Printf("Could not find installation template '%s'\n", instID);
			success = false;
			break;
		}

		if (!cgi->Com_GetConstIntFromNamespace(SAVE_INSTALLATIONSTATUS_NAMESPACE, instStat, (int*) &inst.installationStatus)) {
			Com_Printf("Invalid installation status '%s'\n", instStat);
			success = false;
			break;
		}

		Q_strncpyz(inst.name, cgi->XML_GetString(s, SAVE_INSTALLATION_NAME), sizeof(inst.name));
		cgi->XML_GetPos3(s, SAVE_INSTALLATION_POS, inst.pos);

		inst.installationDamage = cgi->XML_GetInt(s, SAVE_INSTALLATION_DAMAGE, 0);
		inst.alienInterest = cgi->XML_GetFloat(s, SAVE_INSTALLATION_ALIENINTEREST, 0.0);
		inst.buildStart = cgi->XML_GetInt(s, SAVE_INSTALLATION_BUILDSTART, 0);

		/* Radar */
		RADAR_InitialiseUFOs(&inst.radar);
		RADAR_Initialise(&(inst.radar), 0.0f, 0.0f, 1.0f, true);
		if (inst.installationStatus == INSTALLATION_WORKING) {
			RADAR_UpdateInstallationRadarCoverage(&inst, inst.installationTemplate->radarRange, inst.installationTemplate->trackingRange);
			/* UFO Yard */
			inst.ufoCapacity.max = inst.installationTemplate->maxUFOsStored;
		} else {
			inst.ufoCapacity.max = 0;
		}
		inst.ufoCapacity.cur = 0;

		/* read battery slots */
		ss = cgi->XML_GetNode(s, SAVE_INSTALLATION_BATTERIES);
		if (!ss) {
			Com_Printf("INS_LoadXML: Batteries not defined!\n");
			success = false;
			break;
		}
		inst.numBatteries = cgi->XML_GetInt(ss, SAVE_INSTALLATION_NUM, 0);
		if (inst.numBatteries > inst.installationTemplate->maxBatteries) {
			Com_Printf("Installation has more batteries than possible, using upper bound\n");
			inst.numBatteries = inst.installationTemplate->maxBatteries;
		}

		installation_t& instp = LIST_Add(&ccs.installations, inst);
		BDEF_InitialiseInstallationSlots(&instp);
		B_LoadBaseSlotsXML(instp.batteries, instp.numBatteries, ss);
	}
	cgi->Com_UnregisterConstList(saveInstallationConstants);
	cgi->Cvar_Set("mn_installation_count", "%i", INS_GetCount());

	return success;
}
