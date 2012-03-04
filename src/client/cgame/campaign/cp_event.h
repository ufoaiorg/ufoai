/**
 * @file cp_event.h
 * @brief Header for geoscape event related stuff.
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef CLIENT_CL_EVENT
#define CLIENT_CL_EVENT

#define MAX_EVENTMAILS 64
#define MAX_CAMPAIGNEVENTS 128

/**
 * @brief available mails for a tech - mail and mail_pre in script files
 * @sa techMail_t
 * @note Parsed via CL_ParseEventMails
 * @note You can add a mail to the message system and mail client
 * by using e.g. the mission triggers with the script command 'addeventmail \<id\>'
 */
typedef struct eventMail_s {
	char *id;			/**< script id */
	char *from;			/**< sender (_mail_from_paul_navarre, _mail_from_dr_connor) */
	char *to;			/**< recipient (_mail_to_base_commander) */
	char *cc;			/**< copy recipient (_mail_to_base_commander) */
	char *subject;		/**< mail subject line - if mail and mail_pre are available
						 * this will be filled with Proposal: (mail_pre) and Re: (mail)
						 * automatically */
	char *date;			/**< date string, if empty use the date of research */
	char *body;			/**< the body of the event mail */
	char *icon;			/**< icon in the mailclient */
	char *model;		/**< model name of the sender */
	qboolean read;		/**< already read the mail? */
} eventMail_t;

void CL_EventAddMail_f(void);
void CL_ParseEventMails(const char *name, const char **text);
eventMail_t* CL_GetEventMail(const char *id, qboolean createCopy);
eventMail_t* CL_NewEventMail(const char *id, const char *newID, const char *body);
void CP_FreeDynamicEventMail(void);

/**
 * @brief Defines campaign events when story related technologies should be researched
 */
typedef struct campaignEvent_s {
	char *tech;			/**< technology id that should be researched if the overall interest is reached */
	int interest;		/**< the interest value (see @c ccs.oberallInterest) */
} campaignEvent_t;

typedef struct campaignEvents_s {
	campaignEvent_t campaignEvents[MAX_CAMPAIGNEVENTS];	/**< holds all campaign events */
	int numCampaignEvents;	/**< how many events (script-id: events) parsed */
	char *id;				/**< script id */
} campaignEvents_t;

void CP_CheckCampaignEvents(struct campaign_s *campaign);
void CL_ParseCampaignEvents(const char *name, const char **text);
const campaignEvents_t *CP_GetEventsByID(const char *name);

#endif /* CLIENT_CL_EVENT */
