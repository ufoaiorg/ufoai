/**
 * @file cp_save.h
 * @brief Defines some savefile structures
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

#ifndef CP_SAVE_H
#define CP_SAVE_H

#include "../../../common/msg.h"
#include "../../../common/xml.h"

#define MAX_SAVESUBSYSTEMS 32
#define SAVE_FILE_VERSION 4

typedef struct saveSubsystems_s {
	const char *name;
	qboolean (*save) (xmlNode_t *parent);	/**< return false if saving failed */
	qboolean (*load) (xmlNode_t *parent);	/**< return false if loading failed */
} saveSubsystems_t;

#include <zlib.h>

qboolean SAV_QuickSave(void);
void SAV_Init(void);
qboolean SAV_AddSubsystem(saveSubsystems_t *subsystem);

/* and now the save and load prototypes for every subsystem */
qboolean B_SaveXML(xmlNode_t *parent);
qboolean B_LoadXML(xmlNode_t *parent);
qboolean CP_SaveXML(xmlNode_t *parent);
qboolean CP_LoadXML(xmlNode_t *parent);
qboolean HOS_LoadXML(xmlNode_t *parent);
qboolean HOS_SaveXML(xmlNode_t *parent);
qboolean BS_SaveXML(xmlNode_t *parent);
qboolean BS_LoadXML(xmlNode_t *parent);
qboolean AIR_SaveXML(xmlNode_t *parent);
qboolean AIR_LoadXML(xmlNode_t *parent);
qboolean AC_SaveXML(xmlNode_t *parent);
qboolean AC_LoadXML(xmlNode_t *parent);
qboolean E_SaveXML(xmlNode_t *parent);
qboolean E_LoadXML(xmlNode_t *parent);
qboolean RS_SaveXML(xmlNode_t *parent);
qboolean RS_LoadXML(xmlNode_t *parent);
qboolean PR_SaveXML(xmlNode_t *parent);
qboolean PR_LoadXML(xmlNode_t *parent);
qboolean MS_SaveXML(xmlNode_t *parent);
qboolean MS_LoadXML(xmlNode_t *parent);
qboolean STATS_SaveXML(xmlNode_t *parent);
qboolean STATS_LoadXML(xmlNode_t *parent);
qboolean NAT_SaveXML(xmlNode_t *parent);
qboolean NAT_LoadXML(xmlNode_t *parent);
qboolean TR_SaveXML(xmlNode_t *parent);
qboolean TR_LoadXML(xmlNode_t *parent);
qboolean AB_SaveXML(xmlNode_t *parent);
qboolean AB_LoadXML(xmlNode_t *parent);
qboolean XVI_SaveXML(xmlNode_t *parent);
qboolean XVI_LoadXML(xmlNode_t *parent);
qboolean INS_SaveXML(xmlNode_t *parent);
qboolean INS_LoadXML(xmlNode_t *parent);
qboolean MSO_SaveXML(xmlNode_t *parent);
qboolean MSO_LoadXML(xmlNode_t *parent);
qboolean US_SaveXML(xmlNode_t *parent);
qboolean US_LoadXML(xmlNode_t *parent);
qboolean MIS_LoadXML(xmlNode_t *parent);
qboolean MIS_SaveXML(xmlNode_t *parent);
qboolean INT_SaveXML(xmlNode_t *parent);
qboolean INT_LoadXML(xmlNode_t *parent);

qboolean B_PostLoadInit(void);
qboolean AIR_PostLoadInit(void);
qboolean PR_PostLoadInit(void);

qboolean SAV_GameLoad(const char *file, const char **error);

#endif
