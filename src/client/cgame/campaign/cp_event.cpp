/**
 * @file
 * @brief Geoscape event implementation
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
#include "../../../common/binaryexpressionparser.h"
#include "cp_campaign.h"
#include "cp_time.h"
#include "cp_xvi.h"
#include "cp_event_callbacks.h"
#include "save/save_triggerevents.h"

static linkedList_t* eventMails = nullptr;

/**
 * @brief Searches all event mails for a given id
 * @note Might also return nullptr - always check the return value
 * @note If you want to create mails that base on a script definition but have different
 * body messages, set createCopy to true
 * @param[in] id The id from the script files
 */
eventMail_t* CL_GetEventMail (const char* id)
{
	int i;

	for (i = 0; i < ccs.numEventMails; i++) {
		eventMail_t* mail = &ccs.eventMails[i];
		if (Q_streq(mail->id, id))
			return mail;
	}

	LIST_Foreach(eventMails, eventMail_t, listMail) {
		if (Q_streq(listMail->id, id))
			return listMail;
	}

	return nullptr;
}

/**
 * @brief Make sure, that the linked list is freed with every new game
 * @sa CP_ResetCampaignData
 */
void CP_FreeDynamicEventMail (void)
{
	/* the pointers are not freed, this is done with the
	 * pool clear in CP_ResetCampaignData */
	cgi->LIST_Delete(&eventMails);
}

/** @brief Valid event mail parameters */
static const value_t eventMail_vals[] = {
	{"subject", V_TRANSLATION_STRING, offsetof(eventMail_t, subject), 0},
	{"from", V_TRANSLATION_STRING, offsetof(eventMail_t, from), 0},
	{"to", V_TRANSLATION_STRING, offsetof(eventMail_t, to), 0},
	{"cc", V_TRANSLATION_STRING, offsetof(eventMail_t, cc), 0},
	{"date", V_TRANSLATION_STRING, offsetof(eventMail_t, date), 0},
	{"body", V_TRANSLATION_STRING, offsetof(eventMail_t, body), 0},
	{"icon", V_HUNK_STRING, offsetof(eventMail_t, icon), 0},
	{"model", V_HUNK_STRING, offsetof(eventMail_t, model), 0},
	{"skipmessage", V_BOOL, offsetof(eventMail_t, skipMessage), MEMBER_SIZEOF(eventMail_t, skipMessage)},

	{nullptr, V_NULL, 0, 0}
};

/**
 * @sa CL_ParseScriptFirst
 * @note write into cp_campaignPool - free on every game restart and reparse
 */
void CL_ParseEventMails (const char* name, const char** text)
{
	eventMail_t* eventMail;

	if (ccs.numEventMails >= MAX_EVENTMAILS) {
		Com_Printf("CL_ParseEventMails: mail def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the eventMail */
	eventMail = &ccs.eventMails[ccs.numEventMails++];
	OBJZERO(*eventMail);

	Com_DPrintf(DEBUG_CLIENT, "...found eventMail %s\n", name);

	eventMail->id = cgi->PoolStrDup(name, cp_campaignPool, 0);

	cgi->Com_ParseBlock(name, text, eventMail, eventMail_vals, cp_campaignPool);
}

void CP_CheckCampaignEvents (campaign_t* campaign)
{
	const campaignEvents_t* events = campaign->events;
	int i;

	/* no events for the current campaign */
	if (!events)
		return;

	/* no events in that definition */
	if (!events->numCampaignEvents)
		return;

	for (i = 0; i < events->numCampaignEvents; i++) {
		const campaignEvent_t* event = &events->campaignEvents[i];
		if (event->interest <= ccs.overallInterest) {
			RS_MarkStoryLineEventResearched(event->tech);
		}
	}
}

/**
 * Will return the campaign related events
 * @note Also performs some sanity check
 * @param name The events id
 */
const campaignEvents_t* CP_GetEventsByID (const char* name)
{
	for (int i = 0; i < ccs.numCampaignEventDefinitions; i++) {
		const campaignEvents_t* events = &ccs.campaignEvents[i];
		if (Q_streq(events->id, name)) {
			for (int j = 0; j < events->numCampaignEvents; j++) {
				const campaignEvent_t* event = &events->campaignEvents[j];
				if (!RS_GetTechByID(event->tech))
					Sys_Error("Illegal tech '%s' given in events '%s'", event->tech, events->id);
			}
			return events;
		}
	}

	return nullptr;
}

static int CP_CheckTriggerEvent (const char* expression, const void* userdata)
{
	if (Q_strnull(expression))
		return -1;

	const char* type;

	/* check that a particular installation type is built already */
	type = Q_strstart(expression, "installation");
	if (type != nullptr) {
		if (strlen(type) <= 1)
			return -1;
		char value[MAX_VAR];
		Q_strncpyz(value, type + 1, sizeof(value));
		value[strlen(value) - 1] = '\0';
		const installationType_t insType = INS_GetType(value);
		if (INS_HasType(insType, INSTALLATION_NOT_USED))
			return 1;
		return 0;
	}

	/* check whether a particular ufo was detected */
	type = Q_strstart(expression, "ufo");
	if (type != nullptr) {
		if (strlen(type) <= 1)
			return -1;
		char value[MAX_VAR];
		Q_strncpyz(value, type + 1, sizeof(value));
		value[strlen(value) - 1] = '\0';
		const char* detectedUFO = static_cast<const char*>(userdata);
		if (Q_strnull(detectedUFO))
			return -1;
		return Q_streq(detectedUFO, value);
	}

	/* check that the given xvi level is reached in any nation */
	type = Q_strstart(expression, "xvi");
	if (type != nullptr) {
		int xvi;
		if (sscanf(type, "[%i]", &xvi) != 1)
			return -1;
		int i;
		/* check for XVI infection rate */
		for (i = 0; i < ccs.numNations; i++) {
			const nation_t* nation = NAT_GetNationByIDX(i);
			const nationInfo_t* stats = NAT_GetCurrentMonthInfo(nation);
			if (stats->xviInfection >= xvi)
				return 1;
		}
		return 0;
	}

	/* check that the given tech is already researched */
	type = Q_strstart(expression, "researched");
	if (type != nullptr) {
		if (strlen(type) <= 1)
			return -1;
		char value[MAX_VAR];
		Q_strncpyz(value, type + 1, sizeof(value));
		value[strlen(value) - 1] = '\0';
		technology_t* tech = RS_GetTechByID(value);
		if (tech == nullptr)
			return -1;
		if (RS_IsResearched_ptr(tech))
			return 1;
		return 0;
	}

	/* check for nation happiness - also see the lost conditions in the campaign */
	type = Q_strstart(expression, "nationhappiness");
	if (type != nullptr) {
		int nationAmount;

		if (sscanf(type, "[%i]", &nationAmount) != 1)
			return -1;

		int nationBelowLimit = 0;
		for (int j = 0; j < ccs.numNations; j++) {
			const nation_t* nation = NAT_GetNationByIDX(j);
			const nationInfo_t* stats = NAT_GetCurrentMonthInfo(nation);
			if (stats->happiness < ccs.curCampaign->minhappiness) {
				nationBelowLimit++;
				if (nationBelowLimit >= nationAmount)
					return 1;
			}
		}
		return 0;
	}

	/* check that the given average xvi level is reached */
	type = Q_strstart(expression, "averagexvi");
	if (type != nullptr) {
		int xvipercent;
		if (sscanf(type, "[%i]", &xvipercent) != 1)
			return -1;
		if (xvipercent < 0 || xvipercent > 100)
			return -1;
		const int xvi = CP_GetAverageXVIRate();
		if (xvi > ccs.curCampaign->maxAllowedXVIRateUntilLost * xvipercent / 100)
			return 1;
		return 0;
	}

	type = Q_strstart(expression, "difficulty");
	if (type != nullptr) {
		int difficulty;
		if (sscanf(type, "[%i]", &difficulty) != 1)
			return -1;
		return ccs.curCampaign->difficulty == difficulty;
	}

	/* check that these days have passed in the campaign */
	type = Q_strstart(expression, "days");
	if (type != nullptr) {
		int days;
		if (sscanf(type, "[%i]", &days) != 1)
			return -1;
		date_t d = ccs.curCampaign->date;
		d.day += days;
		if (Date_IsDue(&d))
			return 1;
		return 0;
	}

	type = Q_strstart(expression, "alienscaptured");
	if (type != nullptr) {
		if (ccs.campaignStats.capturedAliens > 0)
			return 1;
		return 0;
	}

	type = Q_strstart(expression, "samsitearmed");
	if (type != nullptr) {
		if (!INS_HasType(INSTALLATION_DEFENCE))
			return 1;

		INS_ForeachOfType(installation, INSTALLATION_DEFENCE) {
			if (installation->installationStatus == INSTALLATION_WORKING) {
				for (int i = 0; i < installation->installationTemplate->maxBatteries; i++) {
					const aircraftSlot_t* slot = &installation->batteries[i].slot;
					if (slot->ammoLeft > 0)
						return 1;
				}
			}
		}

		return 0;
	}

	Com_Printf("unknown expression given: '%s'\n", expression);

	return -1;
}

/**
 * @brief Triggers a campaign event with a special type
 * @param[in] type the event type
 * @param[in] userdata Any userdata that is passed to the bep checker function
 */
void CP_TriggerEvent (campaignTriggerEventType_t type, const void* userdata)
{
	int i;

	for (i = 0; i < ccs.numCampaignTriggerEvents; i++) {
		campaignTriggerEvent_t* event = &ccs.campaignTriggerEvents[i];
		if (event->type != type || (!event->active && event->reactivate == nullptr))
			continue;

		if (event->active) {
			if (!cgi->BEP_Evaluate(event->require, CP_CheckTriggerEvent, userdata))
				continue;
			if (Q_strvalid(event->command)) {
				CP_CampaignTriggerFunctions(true);
				cgi->Cmd_ExecuteString("%s", event->command);
				CP_CampaignTriggerFunctions(false);
			}

			if (event->once) {
				event->active = false;
			}
		} else {
			event->active = cgi->BEP_Evaluate(event->reactivate, CP_CheckTriggerEvent, userdata);
		}
	}
}

/** @brief Valid event mail parameters */
static const value_t event_vals[] = {
	{"type", V_INT, offsetof(campaignTriggerEvent_t, type), MEMBER_SIZEOF(campaignTriggerEvent_t, type)},
	{"require", V_HUNK_STRING, offsetof(campaignTriggerEvent_t, require), 0},
	{"reactivate", V_HUNK_STRING, offsetof(campaignTriggerEvent_t, reactivate), 0},
	{"command", V_HUNK_STRING, offsetof(campaignTriggerEvent_t, command), 0},
	{"once", V_BOOL, offsetof(campaignTriggerEvent_t, once), MEMBER_SIZEOF(campaignTriggerEvent_t, once)},

	{nullptr, V_NULL, 0, 0}
};

#define EVENTCONSTANTS_NAMESPACE "eventTrigger::"
static const constListEntry_t eventConstants[] = {
	{EVENTCONSTANTS_NAMESPACE "new_day", NEW_DAY},
	{EVENTCONSTANTS_NAMESPACE "ufo_detection", UFO_DETECTION},
	{EVENTCONSTANTS_NAMESPACE "captured_aliens_died", CAPTURED_ALIENS_DIED},
	{EVENTCONSTANTS_NAMESPACE "captured_aliens", CAPTURED_ALIENS},
	{EVENTCONSTANTS_NAMESPACE "alienbase_discovered", ALIENBASE_DISCOVERED},

	{nullptr, -1}
};

void CP_ParseEventTrigger (const char* name, const char** text)
{
	const char* errhead = "CP_ParseEventTrigger: unexpected end of file (event ";
	const char* token;

	if (ccs.numCampaignTriggerEvents >= MAX_CAMPAIGN_TRIGGER_EVENTS) {
		Com_Printf("CP_ParseEventTrigger: max event def limit hit\n");
		return;
	}

	token = cgi->Com_EParse(text, errhead, name);
	if (!*text)
		return;

	if (!*text || token[0] != '{') {
		Com_Printf("CP_ParseEventTrigger: event def '%s' without body ignored\n", name);
		return;
	}

	cgi->Com_RegisterConstList(eventConstants);

	campaignTriggerEvent_t* event = &ccs.campaignTriggerEvents[ccs.numCampaignTriggerEvents];
	OBJZERO(*event);
	Com_DPrintf(DEBUG_CLIENT, "...found event %s\n", name);
	ccs.numCampaignTriggerEvents++;
	event->active = true;
	event->id = cgi->PoolStrDup(name, cp_campaignPool, 0);

	do {
		token = cgi->Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;
		if (!cgi->Com_ParseBlockToken(name, text, event, event_vals, cp_campaignPool, token)) {
			Com_Printf("CP_ParseEventTrigger: Ignoring unknown event value '%s'\n", token);
		}
	} while (*text);

	cgi->Com_UnregisterConstList(eventConstants);
}

bool CP_TriggerEventSaveXML (xmlNode_t* p)
{
	xmlNode_t* n = cgi->XML_AddNode(p, SAVE_TRIGGEREVENTS_TRIGGEREVENTS);
	int i;

	for (i = 0; i < ccs.numCampaignTriggerEvents; i++) {
		const campaignTriggerEvent_t* event = &ccs.campaignTriggerEvents[i];
		if (event->active)
			continue;
		xmlNode_t* s = cgi->XML_AddNode(n, SAVE_TRIGGEREVENTS_TRIGGEREVENT);

		cgi->XML_AddString(s, SAVE_TRIGGEREVENTS_NAME, event->id);
		cgi->XML_AddBool(s, SAVE_TRIGGEREVENTS_STATE, event->active);
	}

	return true;
}

bool CP_TriggerEventLoadXML (xmlNode_t* p)
{
	xmlNode_t* n, *s;

	n = cgi->XML_GetNode(p, SAVE_TRIGGEREVENTS_TRIGGEREVENTS);
	if (!n)
		return true;

	for (s = cgi->XML_GetNode(n, SAVE_TRIGGEREVENTS_TRIGGEREVENT); s; s = cgi->XML_GetNextNode(s, n, SAVE_TRIGGEREVENTS_TRIGGEREVENT)) {
		const char* id = cgi->XML_GetString(s, SAVE_TRIGGEREVENTS_NAME);
		const bool state = cgi->XML_GetBool(s, SAVE_TRIGGEREVENTS_STATE, true);

		int i;
		for (i = 0; i < ccs.numCampaignTriggerEvents; i++) {
			campaignTriggerEvent_t* event = &ccs.campaignTriggerEvents[i];
			if (Q_streq(event->id, id)) {
				event->active = state;
				break;
			}
		}
	}

	return true;
}

/**
 * @sa CL_ParseScriptFirst
 * @note write into cp_campaignPool - free on every game restart and reparse
 */
void CL_ParseCampaignEvents (const char* name, const char** text)
{
	const char* errhead = "CL_ParseCampaignEvents: unexpected end of file (events ";
	const char* token;
	campaignEvents_t* events;

	if (ccs.numCampaignEventDefinitions >= MAX_CAMPAIGNS) {
		Com_Printf("CL_ParseCampaignEvents: max events def limit hit\n");
		return;
	}

	token = cgi->Com_EParse(text, errhead, name);
	if (!*text)
		return;

	if (!*text || token[0] != '{') {
		Com_Printf("CL_ParseCampaignEvents: events def '%s' without body ignored\n", name);
		return;
	}

	events = &ccs.campaignEvents[ccs.numCampaignEventDefinitions];
	OBJZERO(*events);
	Com_DPrintf(DEBUG_CLIENT, "...found events %s\n", name);
	events->id = cgi->PoolStrDup(name, cp_campaignPool, 0);
	ccs.numCampaignEventDefinitions++;

	do {
		campaignEvent_t* event;
		token = cgi->Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (events->numCampaignEvents >= MAX_CAMPAIGNEVENTS) {
			Com_Printf("CL_ParseCampaignEvents: max events per event definition limit hit\n");
			return;
		}

		/* initialize the eventMail */
		event = &events->campaignEvents[events->numCampaignEvents++];
		OBJZERO(*event);

		event->tech = cgi->PoolStrDup(token, cp_campaignPool, 0);

		token = cgi->Com_EParse(text, errhead, name);
		if (!*text)
			return;

		cgi->Com_EParseValue(event, token, V_INT, offsetof(campaignEvent_t, interest), sizeof(int));

		if (event->interest < 0)
			Sys_Error("Illegal interest value in events definition '%s' for tech '%s'", events->id, event->tech);
	} while (*text);
}

/**
 * @brief Adds the event mail to the message stack. This message is going to be added to the savegame.
 */
void CL_EventAddMail (const char* eventMailId)
{
	eventMail_t* eventMail = CL_GetEventMail(eventMailId);
	if (!eventMail) {
		Com_Printf("CL_EventAddMail: Could not find eventmail with id '%s'\n", eventMailId);
		return;
	}

	if (eventMail->sent) {
		return;
	}

	if (!eventMail->from || !eventMail->to || !eventMail->subject || !eventMail->body) {
		Com_Printf("CL_EventAddMail: mail with id '%s' has incomplete data\n", eventMailId);
		return;
	}

	if (!eventMail->date) {
		dateLong_t date;
		char dateBuf[MAX_VAR] = "";

		CP_DateConvertLong(&ccs.date, &date);
		Com_sprintf(dateBuf, sizeof(dateBuf), _("%i %s %02i"),
			date.year, Date_GetMonthName(date.month - 1), date.day);
		eventMail->date = cgi->PoolStrDup(dateBuf, cp_campaignPool, 0);
	}

	eventMail->sent = true;

	if (!eventMail->skipMessage) {
		uiMessageListNodeMessage_t* m = MS_AddNewMessage("", va(_("You've got a new mail: %s"), _(eventMail->subject)), MSG_EVENT);
		if (m)
			m->eventMail = eventMail;
		else
			Com_Printf("CL_EventAddMail: Could not add message with id: %s\n", eventMailId);
	}

	UP_OpenEventMail(eventMailId);
}

/**
 * @sa UP_OpenMail_f
 * @sa MS_AddNewMessage
 * @sa UP_SetMailHeader
 * @sa UP_OpenEventMail
 */
void CL_EventAddMail_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <event_mail_id>\n", cgi->Cmd_Argv(0));
		return;
	}

	CL_EventAddMail(cgi->Cmd_Argv(1));
}
