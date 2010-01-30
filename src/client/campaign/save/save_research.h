/**
 * @file save_research.h
 * @brief XML tag constants for savegame.
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#define SAVE_RESEARCH_RESEARCH "research"
#define SAVE_RESEARCH_TECH "tech"
#define SAVE_RESEARCH_ID "id"
#define SAVE_RESEARCH_STATUSCOLLECTED "statusCollected"
#define SAVE_RESEARCH_TIME "time"
#define SAVE_RESEARCH_STATUSRESEARCH "statusResearch"
#define SAVE_RESEARCH_BASE "baseIDX"
#define SAVE_RESEARCH_SCIENTISTS "scientists"
#define SAVE_RESEARCH_STATUSRESEARCHABLE "statusResearchable"
#define SAVE_RESEARCH_PREDAY "preDay"
#define SAVE_RESEARCH_PRESEC "preSec"
#define SAVE_RESEARCH_DAY "day"
#define SAVE_RESEARCH_SEC "sec"
#define SAVE_RESEARCH_MAILSENT "mailSent"
#define SAVE_RESEARCH_MAIL "mail"
#define SAVE_RESEARCH_MAIL_ID "id"

#define SAVE_RESEARCHSTATUS_NAMESPACE "saveResearchStatus"
const constListEntry_t saveResearchConstants[] = {
	{SAVE_RESEARCHSTATUS_NAMESPACE"::none", RS_NONE},
	{SAVE_RESEARCHSTATUS_NAMESPACE"::running", RS_RUNNING},
	{SAVE_RESEARCHSTATUS_NAMESPACE"::paused", RS_PAUSED},
	{SAVE_RESEARCHSTATUS_NAMESPACE"::finished", RS_FINISH},
	{NULL, -1}
};

/*
DTD:

<!ELEMENT research tech*>
<!ELEMENT tech mail*>
<!ATTLIST tech
	id					#CDATA		REQUIRED
	statusCollected		#CDATA		IMPLIED
	time				#NMTOKEN	IMPLIED
	statusResearch		#CDATA		REQUIRED
	baseIDX				#NMTOKEN	IMPLIED
	scientists			#NMTOKEN	IMPLIED
	statusResearchable	#NMTOKEN	IMPLIED
	preDay				#NMTOKEN	IMPLIED
	preSec				#NMTOKEN	IMPLIED
	day					#NMTOKEN	IMPLIED
	sec					#NMTOKEN	IMPLIED
	mailSent			#NMTOKEN	IMPLIED
>

<!ELEMENT mail EMPTY>
<!ATTLIST mail
	id					#CDATA		REQUIRED
>

*/
