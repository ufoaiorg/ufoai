/**
 * @file scp_missions.h
 * @brief Singleplayer static campaign mission header
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

#ifndef SCP_MISSIONS_H
#define SCP_MISSIONS_H

#include "../../cl_shared.h"
#include "../campaign/cp_campaign.h"

void SCP_SpawnNewMissions(void);
void SCP_CampaignActivateFirstStage(void);
void SCP_CampaignProgress(const missionResults_t *results);
qboolean SCP_Save(xmlNode_t *parent);
qboolean SCP_Load(xmlNode_t *parent);

#endif
