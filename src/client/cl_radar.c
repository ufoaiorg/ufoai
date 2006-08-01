/**
 * @file cl_radar.c
 * @brief Radars / sensor stuff, to detect and track ufos
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

static qboolean RADAR_AddUfo(radar_t* radar, int numUfo);
static int RADAR_IsUfoSensored(const radar_t* radar, int numUfo);
extern void RADAR_RemoveUfo(radar_t* radar, const aircraft_t* ufo);
extern void Radar_NotifyUfoRemoved(radar_t* radar, const aircraft_t* ufo);
extern void RADAR_ChangeRange(radar_t* radar, int change);
extern void Radar_Initialise(radar_t* radar, int range);
extern qboolean RADAR_CheckUfoSensored(radar_t* radar, vec2_t posRadar, const aircraft_t* ufo);

/**
 * @brief Add an ufo in the list of sensored ufo
 */
static qboolean RADAR_AddUfo(radar_t* radar, int numUfo)
{
	if (radar->numUfos >= MAX_UFOONGEOSCAPE)
		return qfalse;
	
	radar->ufos[radar->numUfos] = numUfo;
	radar->numUfos++;
	return qtrue;
}

/**
 * @brief Check if ufo is sensored list, and return its position in list (-1 if not)
 */
static int RADAR_IsUfoSensored(const radar_t* radar, int numUfo)
{
	int i;
	
	for (i = 0 ; i < radar->numUfos ; i++)
		if (radar->ufos[i] == numUfo)
			return i;

	return -1;
}

/**
 * @brief Ufo will no more be referenced by radar
 */
extern void RADAR_RemoveUfo(radar_t* radar, const aircraft_t* ufo)
{
	int i, numUfo = ufo - gd.ufos;

	for (i = 0 ; i < radar->numUfos ; i++)
		if (radar->ufos[i] == numUfo) {
			radar->numUfos--;
			radar->ufos[i] = radar->ufos[radar->numUfos];
			return;
		}
}

/**
 * @brief Notify that the specified ufo has been removed from geoscape
 **/
extern void Radar_NotifyUfoRemoved(radar_t* radar, const aircraft_t* ufo)
{
	int i, numUfo = ufo - gd.ufos;
	
	for (i = 0 ; i < radar->numUfos ; i++)
		if (radar->ufos[i] == numUfo) {
			radar->numUfos--;
			radar->ufos[i] = radar->ufos[radar->numUfos];
			i--;	/* Allow the moved value to be checked */
		} else if (radar->ufos[i] > numUfo)
			radar->ufos[i]--;
}

/**
 * @brief Change to radar range
 */
extern void RADAR_ChangeRange(radar_t* radar, int change)
{
	radar->range += change;
	if (radar->range < 0)
		radar->range = 0;
}

/**
 * @brief Initialise radar
 */
extern void Radar_Initialise(radar_t* radar, int range)
{
	radar->range = range;
	radar->numUfos = 0;
}

/**
 * @brief Check if the specified ufo is inside the sensor range of base
 * Return true if the aircraft is inside sensor
 */
extern qboolean RADAR_CheckUfoSensored(radar_t* radar, vec2_t posRadar, const aircraft_t* ufo)
{
	int dist;
	int num;
	int numAircraftSensored;

	/* Get num of ufo in gd.ufos */
	num = ufo - gd.ufos;
	if (num < 0 || num >= gd.numUfos)
		return qfalse;

	numAircraftSensored = RADAR_IsUfoSensored(radar, num);	/* indice of ufo in radar list */
	dist = CP_GetDistance(posRadar, ufo->pos);	/* Distance from radar to ufo */

	if (radar->range >= dist) {
		/* Ufo is inside the radar range */
		if (numAircraftSensored < 0) {
			/* Ufo was not sensored by the radar */
			RADAR_AddUfo(radar, num);
		}
		return qtrue;
	}

	/* Ufo is not in the sensor range */
	if (numAircraftSensored >= 0) {
		/* Ufo was sensored by the radar */
		RADAR_RemoveUfo(radar, ufo);
	}
	return qfalse;
}
