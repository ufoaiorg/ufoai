/**
 * @file cp_statistics.h
 * @brief Campaign statistic headers
 */

/*
All original material Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#ifndef CP_STATISTICS_H
#define CP_STATISTICS_H

typedef struct stats_s {
	int missions;				/**< all missions (used for unique idx generation) */
	int missionsWon;
	int missionsLost;
	int basesBuilt;
	int basesAttacked;
	int installationsBuild;
	int interceptions;
	int soldiersLost;
	int soldiersNew;			/**< new recruits */
	int killedAliens;
	int rescuedCivilians;
	int researchedTechnologies;
	int moneyInterceptions;
	int moneyBases;
	int moneyResearch;
	int moneyWeapons;
	int ufosDetected;
	int alienBasesBuilt;		/**< all alien bases (used for unique idx generation) */
	int ufosStored;				/**< all UFOS stored in UFO Yards (used for unique idx generation) */
} stats_t;

void CL_StatsUpdate_f(void);
qboolean CP_LoadMapDefStatXML(mxml_node_t *parent);
void STATS_InitStartup(void);

#endif

