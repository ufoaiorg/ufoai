/**
 * @file cl_event.c
 * @brief Geoscape event implementation
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

/**
 * @brief Searches all event mails for a given id
 * @note Might also return NULL - always check the return value
 */
eventMail_t* CL_GetEventMail (const char *id)
{
	int i;

	for (i = 0; i < gd.numEventMails; i++)
		if (!Q_strcmp(gd.eventMails[i].id, id))
			return &gd.eventMails[i];

	return NULL;
}


/** @brief Valid event mail parameters */
static const value_t eventMail_vals[] = {
	{"subject", V_TRANSLATION_MANUAL_STRING, offsetof(eventMail_t, subject), 0},
	{"from", V_TRANSLATION_MANUAL_STRING, offsetof(eventMail_t, from), 0},
	{"to", V_TRANSLATION_MANUAL_STRING, offsetof(eventMail_t, to), 0},
	{"cc", V_TRANSLATION_MANUAL_STRING, offsetof(eventMail_t, cc), 0},
	{"date", V_TRANSLATION_MANUAL_STRING, offsetof(eventMail_t, date), 0},
	{"body", V_TRANSLATION_MANUAL_STRING, offsetof(eventMail_t, body), 0},
	{"icon", V_CLIENT_HUNK_STRING, offsetof(eventMail_t, icon), 0},

	{NULL, 0, 0, 0}
};

/**
 * @sa CL_ParseScriptFirst
 * @note write into cl_localPool - free on every game restart and reparse
 */
void CL_ParseEventMails (const char *name, const char **text)
{
	const char *errhead = "CL_ParseEventMails: unexpected end of file (mail ";
	eventMail_t *eventMail;
	const value_t *vp;
	const char *token;

	if (gd.numEventMails >= MAX_EVENTMAILS) {
		Com_Printf("CL_ParseEventMails: mail def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the eventMail */
	eventMail = &gd.eventMails[gd.numEventMails++];
	memset(eventMail, 0, sizeof(eventMail_t));

	Com_DPrintf(DEBUG_CLIENT, "...found eventMail %s\n", name);
	eventMail->id = Mem_PoolStrDup(name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseEventMails: eventMail def \"%s\" without body ignored\n", name);
		gd.numEventMails--;
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* check for some standard values */
		for (vp = eventMail_vals; vp->string; vp++)
			if (!Q_strcmp(token, vp->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				switch (vp->type) {
				case V_TRANSLATION_MANUAL_STRING:
					token++;
				case V_CLIENT_HUNK_STRING:
					Mem_PoolStrDupTo(token, (char**) ((char*)eventMail + (int)vp->ofs), cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
					break;
				default:
					Com_ParseValue(eventMail, token, vp->type, vp->ofs, vp->size);
					break;
				}
				break;
			}

		if (!vp->string) {
			Com_Printf("CL_ParseEventMails: unknown token \"%s\" ignored (mail %s)\n", token, name);
			COM_EParse(text, errhead, name);
		}
	} while (*text);
}

/**
 * @sa UP_OpenMail_f
 * @sa MN_AddNewMessage
 * @sa UP_SetMailHeader
 * @sa UP_OpenEventMail
 */
void CL_EventAddMail_f (void)
{
	const char *eventMailId;
	eventMail_t* eventMail;
	message_t *m;
	char dateBuf[MAX_VAR] = "";

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <event_mail_id>\n", Cmd_Argv(0));
		return;
	}

	eventMailId = Cmd_Argv(1);

	eventMail = CL_GetEventMail(eventMailId);
	if (!eventMail) {
		Com_Printf("CL_EventAddMail_f: Could not find eventmail with id '%s'\n", eventMailId);
		return;
	}

	if (!eventMail->from || !eventMail->to || !eventMail->subject || !eventMail->body) {
		Com_Printf("CL_EventAddMail_f: mail with id '%s' has incomplete data\n", eventMailId);
		return;
	}

	if (!eventMail->date) {
		int day, month;
		CL_DateConvert(&ccs.date, &day, &month);
		Com_sprintf(dateBuf, sizeof(dateBuf), _("%i %s %02i"),
			ccs.date.day / DAYS_PER_YEAR, CL_DateGetMonthName(month), day);
		eventMail->date = Mem_PoolStrDup(dateBuf, cl_localPool, 0);
	}

	/* the subject double %s: see UP_SetMailHeader */
	m = MN_AddNewMessage(_(eventMail->subject), _(eventMail->body), qfalse, MSG_EVENT, NULL);
	if (m)
		m->eventMail = eventMail;
	else
		Com_Printf("Could not add message with id: %s\n", eventMailId);
}
