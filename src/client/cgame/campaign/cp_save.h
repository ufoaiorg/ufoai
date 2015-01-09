/**
 * @file
 * @brief Defines some savefile structures
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

#pragma once

#include "../../../common/msg.h"
#include "../../../common/xml.h"

#define FOREACH_XMLNODE(var, node, name) \
	for (xmlNode_t* var = cgi->XML_GetNode((node), name); var; var = cgi->XML_GetNextNode(var, node, name))

#define MAX_SAVESUBSYSTEMS 32
#define SAVE_FILE_VERSION 4

typedef struct saveSubsystems_s {
	const char* name;
	bool (*save) (xmlNode_t* parent);	/**< return false if saving failed */
	bool (*load) (xmlNode_t* parent);	/**< return false if loading failed */
} saveSubsystems_t;

#include <zlib.h>

void SAV_Init(void);
bool SAV_AddSubsystem(saveSubsystems_t* subsystem);

/* and now the save and load prototypes for every subsystem */
bool B_SaveXML(xmlNode_t* parent);
bool B_LoadXML(xmlNode_t* parent);
bool CP_SaveXML(xmlNode_t* parent);
bool CP_LoadXML(xmlNode_t* parent);
bool HOS_LoadXML(xmlNode_t* parent);
bool HOS_SaveXML(xmlNode_t* parent);
bool BS_SaveXML(xmlNode_t* parent);
bool BS_LoadXML(xmlNode_t* parent);
bool AIR_SaveXML(xmlNode_t* parent);
bool AIR_LoadXML(xmlNode_t* parent);
bool AC_LoadXML(xmlNode_t* parent);
bool E_SaveXML(xmlNode_t* parent);
bool E_LoadXML(xmlNode_t* parent);
bool RS_SaveXML(xmlNode_t* parent);
bool RS_LoadXML(xmlNode_t* parent);
bool PR_SaveXML(xmlNode_t* parent);
bool PR_LoadXML(xmlNode_t* parent);
bool MS_SaveXML(xmlNode_t* parent);
bool MS_LoadXML(xmlNode_t* parent);
bool STATS_SaveXML(xmlNode_t* parent);
bool STATS_LoadXML(xmlNode_t* parent);
bool NAT_SaveXML(xmlNode_t* parent);
bool NAT_LoadXML(xmlNode_t* parent);
bool TR_SaveXML(xmlNode_t* parent);
bool TR_LoadXML(xmlNode_t* parent);
bool AB_SaveXML(xmlNode_t* parent);
bool AB_LoadXML(xmlNode_t* parent);
bool XVI_SaveXML(xmlNode_t* parent);
bool XVI_LoadXML(xmlNode_t* parent);
bool INS_SaveXML(xmlNode_t* parent);
bool INS_LoadXML(xmlNode_t* parent);
bool MSO_SaveXML(xmlNode_t* parent);
bool MSO_LoadXML(xmlNode_t* parent);
bool US_SaveXML(xmlNode_t* parent);
bool US_LoadXML(xmlNode_t* parent);
bool MIS_LoadXML(xmlNode_t* parent);
bool MIS_SaveXML(xmlNode_t* parent);
bool INT_SaveXML(xmlNode_t* parent);
bool INT_LoadXML(xmlNode_t* parent);

bool B_PostLoadInit(void);
bool AIR_PostLoadInit(void);
bool PR_PostLoadInit(void);

bool SAV_GameLoad(const char* file, const char** error);
void SAV_GameDelete_f(void);
