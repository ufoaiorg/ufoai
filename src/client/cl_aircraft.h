/**
 * @file cl_aircraft.h
 * @brief Header file for aircraft stuff
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

#ifndef CLIENT_CL_AIRCRAFT_H
#define CLIENT_CL_AIRCRAFT_H

#define MAX_AIRCRAFT	256
#define MAX_CRAFTUPGRADES	64
#define LINE_MAXSEG 64
#define LINE_MAXPTS (LINE_MAXSEG+2)
#define LINE_DPHI	(M_PI/LINE_MAXSEG)

typedef struct mapline_s {
	int n;
	float dist;
	vec2_t p[LINE_MAXPTS];
} mapline_t;

typedef enum {
	AIRCRAFT_TRANSPORTER,
	AIRCRAFT_INTERCEPTOR,
	AIRCRAFT_UFO
} aircraftType_t;

struct actMis_s;

typedef struct aircraft_s {
	int idx;					/* unique id */
	int idx_sample;				/* self-link in aircraft_sample list */
	char id[MAX_VAR];			/* internal id from script file */
	char name[MAX_VAR];			/* translateable name */
	char image[MAX_VAR];		/* image on geoscape */
	aircraftType_t type;
	int status;					/* see aircraftStatus_t */
	float speed;
	int price;
	int fuel;					/* actual fuel */
	int fuelSize;				/* max fuel */
	int size;					/* how many soldiers max */
	vec2_t pos;					/* actual pos on geoscape */
	int point;
	int time;
	int idxInBase;				/* id in base */
	int idxBase;				/* id of base */
	/* pointer to base->numOnTeam[AIRCRAFT_ID] */
	int *teamSize;				/* how many soldiers on board */
	int teamIdxs[MAX_ACTIVETEAM];              /* array of team members on board employee idx*/

	char model[MAX_QPATH];
	char weapon_string[MAX_VAR];
	/* NOTE: these pointers needs reinit after loading a saved game */
	technology_t *weapon;
	char shield_string[MAX_VAR];
	technology_t *shield;
	mapline_t route;
	void *homebase;				/* pointer to homebase */

	char building[MAX_VAR];		/* id of the building needed as hangar */

	int numUpgrades;

	struct actMis_s* mission;	/* The mission the aircraft is moving to */
	int ufo;				/* Ufo's id the aircraft is purchasing (gd.ufos + id) */
	radar_t	radar;			/* Radar to track ufos */

	qboolean visible;		/* The ufo is visible ? */
} aircraft_t;

extern aircraft_t aircraft_samples[MAX_AIRCRAFT]; /* available aircraft types */
extern int numAircraft_samples;

/* script functions */
void CL_ListAircraft_f(void);
void CL_AircraftStart_f(void);
void CL_NewAircraft_f(void);
void MN_NextAircraft_f(void);
void MN_PrevAircraft_f(void);
void CL_AircraftReturnToBase_f(void);
void CL_AircraftEquipmenuMenuInit_f(void);
void CL_AircraftEquipmenuMenuWeaponsClick_f(void);
void CL_AircraftEquipmenuMenuShieldsClick_f(void);
char *CL_AircraftStatusToName(aircraft_t * aircraft);
void CL_AircraftInit(void);
void CL_AircraftSelect(void);
void CL_NewAircraft_f(void);
void CL_DeleteAircraft(aircraft_t *aircraft);

void CL_ResetAircraftTeam(aircraft_t *aircraft);
void CL_AddToAircraftTeam(aircraft_t *aircraft,int idx);
void CL_RemoveFromAircraftTeam(aircraft_t *aircraft,int idx);
void CL_DecreaseAircraftTeamIdxGreaterThan(aircraft_t *aircraft,int idx);
qboolean CL_IsInAircraftTeam(aircraft_t *aircraft,int idx);

void CL_CampaignRunAircraft(int dt);
aircraft_t *CL_GetAircraft(char *name);
extern aircraft_t* CL_AircraftGetFromIdx(int idx);
extern void CP_GetRandomPosForAircraft(float *pos);
extern qboolean CL_AircraftMakeMove(int dt, aircraft_t* aircraft);
void CL_ParseAircraft(char *name, char **text);
extern void CL_AircraftReturnToBase(aircraft_t *aircraft);
extern qboolean CL_SendAircraftToMission(aircraft_t* aircraft, struct actMis_s* mission);
extern void CL_SendAircraftPurchasingUfo(aircraft_t* aircraft, aircraft_t* ufo);
extern void CL_AircraftsNotifyUfoRemoved(const aircraft_t *const ufo);
extern void CL_AircraftsUfoDisappear(const aircraft_t *const ufo);

#endif
