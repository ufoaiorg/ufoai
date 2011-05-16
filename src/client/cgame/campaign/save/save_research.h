/**
 * @file save_research.h
 * @brief XML tag constants for savegame.
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

#ifndef SAVE_RESEARCH_H
#define SAVE_RESEARCH_H

#define SAVE_RESEARCH_RESEARCH "research"
#define SAVE_RESEARCH_TECH "tech"
#define SAVE_RESEARCH_ID "id"
#define SAVE_RESEARCH_STATUSCOLLECTED "statusCollected"
#define SAVE_RESEARCH_TIME "time"
#define SAVE_RESEARCH_STATUSRESEARCH "statusResearch"
#define SAVE_RESEARCH_BASE "baseIDX"
#define SAVE_RESEARCH_SCIENTISTS "scientists"
#define SAVE_RESEARCH_STATUSRESEARCHABLE "statusResearchable"
#define SAVE_RESEARCH_PREDATE "preDate"
#define SAVE_RESEARCH_DATE "date"
#define SAVE_RESEARCH_MAILSENT "mailSent"
#define SAVE_RESEARCH_MAIL "mail"
#define SAVE_RESEARCH_MAIL_ID "id"

#define SAVE_RESEARCHSTATUS_NAMESPACE "saveResearchStatus"
static const constListEntry_t saveResearchConstants[] = {
	{SAVE_RESEARCHSTATUS_NAMESPACE"::none", RS_NONE},
	{SAVE_RESEARCHSTATUS_NAMESPACE"::running", RS_RUNNING},
	{SAVE_RESEARCHSTATUS_NAMESPACE"::paused", RS_PAUSED},
	{SAVE_RESEARCHSTATUS_NAMESPACE"::finished", RS_FINISH},
	{NULL, -1}
};

#endif

/*
DTD:

<!ELEMENT research tech*>
<!ELEMENT tech mail* preDate date>
<!ATTLIST tech
	id					CDATA		#REQUIRED
	statusCollected		CDATA		#IMPLIED
	time				CDATA		#IMPLIED
	statusResearch		CDATA		#REQUIRED
	baseIDX				CDATA		#IMPLIED
	scientists			CDATA		#IMPLIED
	statusResearchable	CDATA		#IMPLIED
	mailSent			CDATA		#IMPLIED
>

<!ELEMENT preDate EMPTY>
<!ATTLIST preDate
	day					CDATA		'0'
	sec					CDATA		'0'
>

<!ELEMENT date EMPTY>
<!ATTLIST date
	day					CDATA		'0'
	sec					CDATA		'0'
>

<!ELEMENT mail EMPTY>
<!ATTLIST mail
	id					CDATA		#REQUIRED
>

*/
