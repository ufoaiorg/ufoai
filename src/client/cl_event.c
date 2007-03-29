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
#include "cl_event.h"
#include "cl_global.h"

/**
 * @brief Searches all event mails for a given id
 * @note Might also return NULL - always check the return value
 */
static eventMail_t* CL_GetEventMail (const char *id)
{
	int i;

	for (i = 0; i < gd.numEventMails; i++)
		if (!Q_strcmp(gd.eventMails[i].id, id))
			return &gd.eventMails[i];

	return NULL;
}


/** @brief Valid event mail parameters */
static const value_t eventMail_vals[] = {
	{"subject", V_TRANSLATION2_STRING, offsetof(eventMail_t, subject)}
	,
	{"from", V_TRANSLATION2_STRING, offsetof(eventMail_t, from)}
	,
	{"to", V_TRANSLATION2_STRING, offsetof(eventMail_t, to)}
	,
	{"cc", V_TRANSLATION2_STRING, offsetof(eventMail_t, cc)}
	,
	{"date", V_TRANSLATION2_STRING, offsetof(eventMail_t, date)}
	,

	{NULL, 0, 0}
};

/**
 * @brief
 */
extern void CL_ParseEventMails (const char *name, char **text)
{
	const char *errhead = "CL_ParseEventMails: unexpected end of file (mail ";
	eventMail_t *eventMail;
	const value_t *vp;
	char *token;

	if (gd.numEventMails >= MAX_EVENTMAILS) {
		Com_Printf("CL_ParseEventMails: mail def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the eventMail */
	eventMail = &gd.eventMails[gd.numEventMails++];
	memset(eventMail, 0, sizeof(eventMail_t));

	Com_DPrintf("...found eventMail %s\n", name);
	Q_strncpyz(eventMail->id, name, sizeof(eventMail->id));

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

				Com_ParseValue(eventMail, token, vp->type, vp->ofs);
				break;
			}

		if (!vp->string) {
			Com_Printf("CL_ParseEventMails: unknown token \"%s\" ignored (mail %s)\n", token, name);
			COM_EParse(text, errhead, name);
		}
	} while (*text);
}

/**
 * @brief
 */
extern void CL_EventAddMail_f (void)
{
	const char *eventMailId;
	eventMail_t* eventMail;

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

	/* TODO - add mail to message system */
}
