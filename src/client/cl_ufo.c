/**
 * @file cl_ufos.c
 * @brief UFOs on geoscape
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

extern void UFO_GetUfosList(aircraft_t*** first_ufo, int* numUfo);
static void UFO_SetUfoRandomDest(aircraft_t* ufo);
extern void UFO_CampaignRunUfos(int dt);
#ifdef DEBUG
static void UFO_ListUfosOnGeoscape(void);
#endif
static void UFO_NewUfoOnGeoscape(void);
static void UFO_RemoveUfoFromGeoscape(aircraft_t* ufo);
static void UFO_RemoveUfoFromGeoscape_f(void);
extern void UFO_CampaignCheckEvents(void);
extern void UFO_Reset(void);

#define MAX_UFOONGEOSCAPE 8								/**< Max UFOs on geoscape */

static aircraft_t *ufoOnGeoscape[MAX_UFOONGEOSCAPE];	/**< UFOs on geoscape */


/**
 * @brief Return the ufo on geoscape list and count
 */
extern void UFO_GetUfosList(aircraft_t*** first_ufo, int* numUfo) {
	*first_ufo = ufoOnGeoscape;
	*numUfo = ccs.numUfoOnGeoscape;
}

/**
 * @brief Give a random destination to an ufo, and make him to move there
 */
static void UFO_SetUfoRandomDest(aircraft_t* ufo) {
	vec2_t pos;

	ufo->status = AIR_TRANSIT;
	ufo->time = 0;
	ufo->point = 0;
	CP_GetRandomPosForAircraft(pos);
	MAP_MapCalcLine(ufo->pos, pos, &(ufo->route));
}

/**
 * @brief Make the UFOs run
 */
extern void UFO_CampaignRunUfos(int dt) {
	aircraft_t** ufos;

	/* now the ufos are flying around, too */
	for (ufos = ufoOnGeoscape + ccs.numUfoOnGeoscape - 1 ; ufos >= ufoOnGeoscape ; ufos--) {
		if (CL_AircraftMakeMove(dt, *ufos))
			UFO_SetUfoRandomDest(*ufos);

		/* TODO: Crash */
		if ((*ufos)->fuel <= 0)
			(*ufos)->fuel = (*ufos)->fuelSize;
	}
}

#ifdef DEBUG
/**
  * @brief Write the ufo list, for debugging
  *
  * listufo
  * only for debugging
  */
static void UFO_ListUfosOnGeoscape(void)
{
	int i;
	aircraft_t *a;

	Com_Printf("There are %i ufos on geoscape\n", ccs.numUfoOnGeoscape);
	for (i = 0; i < ccs.numUfoOnGeoscape; i++) {
		a = ufoOnGeoscape[i];
		Com_Printf("ufo: %s - status: %i - pos: %.0f:%0.f\n", a->id, a->status, a->pos[0], a->pos[1]);
	}
}
#endif

/**
  * @brief Add an ufo in geoscape
  *
  * NOTE: These ufos are not saved - and i don't think we need this
  * TODO: Ufos are not assigned unique idx fields. Could be handy...
  */
static void UFO_NewUfoOnGeoscape(void)
{
	static int newUfoNum = -1;
	aircraft_t *ufo = NULL;

	/* check max amount */
	if (ccs.numUfoOnGeoscape >= MAX_UFOONGEOSCAPE)
		return;

	/* Get next type of ufo in aircrafts list */
	while(++newUfoNum < numAircraft_samples)
		if (aircraft_samples[newUfoNum].type == AIRCRAFT_UFO)
			break;
	if (newUfoNum == numAircraft_samples)
		for (newUfoNum = 0 ; newUfoNum < numAircraft_samples ; newUfoNum++)			
			if (aircraft_samples[newUfoNum].type == AIRCRAFT_UFO)
				break;
	if (newUfoNum == numAircraft_samples)
		return;

	/* Create ufo */
	if ((ufo = malloc(sizeof(aircraft_t))) == NULL)
		return;
	ufo = memcpy(ufo,  aircraft_samples + newUfoNum, sizeof(aircraft_t));
	ufoOnGeoscape[ccs.numUfoOnGeoscape] = ufo;
	Com_DPrintf("New ufo on geoscape: '%s' (%i)\n", ufo->name, ccs.numUfoOnGeoscape);
	ccs.numUfoOnGeoscape++;
	
	/* Initialise ufo data */
	ufo->visible = qfalse;					/* No visible in radars (just for now) */
	CP_GetRandomPosForAircraft(ufo->pos);	/* Random position */
	UFO_SetUfoRandomDest(ufo);				/* Random destination */
}

/**
 * @brief Remove the specified ufo from geoscape
 */
static void UFO_RemoveUfoFromGeoscape(aircraft_t* ufo) {
	/* Remove ufo from ufos list */
	if (CL_ListRemoveItem((void**)ufoOnGeoscape, &ccs.numUfoOnGeoscape, ufo))
		Com_DPrintf("Remove ufo from geoscape: '%s'\n", ufoOnGeoscape[ccs.numUfoOnGeoscape]->name);		
	
	/* Notications */
	B_NotifyAircraftRemove(ufo);
	CL_PopupNotifyUfoRemoved(ufo);
	
	/* Free memory */
	free(ufo);	
}

/**
  * @brief Remove an ufo from the geoscape
  */
static void UFO_RemoveUfoFromGeoscape_f(void)
{
	if (ccs.numUfoOnGeoscape > 0)
		UFO_RemoveUfoFromGeoscape(*ufoOnGeoscape);
}

/**
 * @brief Check events for ufos : appears or disappears on radars
 */
extern void UFO_CampaignCheckEvents(void)
{
	int i, j;
	qboolean visible;
	aircraft_t*	ufo;
	
	/* For each ufo in geoscape */
	for (i = 0; i < ccs.numUfoOnGeoscape; i++) {
		ufo = ufoOnGeoscape[i];
		visible = ufo->visible;
		ufo->visible = qfalse;

		/* Check for ufo detection by bases */
		for (j = 0; j < gd.numBases; j++)
			ufo->visible |= B_CheckAircraftSensored(gd.bases + j, ufoOnGeoscape[i]);

		/* Check if ufo appears or disappears on radar */
		if (visible != ufo->visible) {
			if (ufo->visible) {
				MN_AddNewMessage(_("Notice"), _("UFO appears on our radar"), qfalse, MSG_STANDARD, NULL);
				CL_GameTimeStop();
			}
			else {
				/* FIXME: grammar: from/of/on */
				MN_AddNewMessage(_("Notice"), _("UFO disappears on our radar"), qfalse, MSG_STANDARD, NULL);
			}
		}	
	}
}

/**
  * @brief
  */
extern void UFO_Reset(void)
{
	Cmd_AddCommand("addufo", UFO_NewUfoOnGeoscape);
	Cmd_AddCommand("removeufo", UFO_RemoveUfoFromGeoscape_f);
#ifdef DEBUG
	Cmd_AddCommand("listufo", UFO_ListUfosOnGeoscape);
#endif
}
