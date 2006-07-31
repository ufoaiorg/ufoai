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
	aircraft_t*	ufo;

	/* now the ufos are flying around, too */
	for (ufo = gd.ufos + gd.numUfos - 1 ; ufo >= gd.ufos; ufo--) {
		if (CL_AircraftMakeMove(dt, ufo))
			UFO_SetUfoRandomDest(ufo);

		/* TODO: Crash */
		if (ufo->fuel <= 0)
			ufo->fuel = ufo->fuelSize;
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
	aircraft_t* ufo;

	Com_Printf("There are %i ufos on geoscape\n", gd.numUfos);
	for (ufo = gd.ufos + gd.numUfos - 1 ; ufo >= gd.ufos; ufo--)
		Com_Printf("ufo: %s - status: %i - pos: %.0f:%0.f\n", ufo - gd.ufos + 1, ufo->status, ufo->pos[0], ufo->pos[1]);
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
	if (gd.numUfos >= MAX_UFOONGEOSCAPE)
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
	ufo = gd.ufos + gd.numUfos;
	memcpy(ufo,  aircraft_samples + newUfoNum, sizeof(aircraft_t));
	Com_DPrintf("New ufo on geoscape: '%s' (%i)\n", ufo->name, gd.numUfos);
	gd.numUfos++;
	
	/* Initialise ufo data */
	ufo->visible = qfalse;					/* No visible in radars (just for now) */
	CP_GetRandomPosForAircraft(ufo->pos);	/* Random position */
	UFO_SetUfoRandomDest(ufo);				/* Random destination */
}

/**
 * @brief Remove the specified ufo from geoscape
 */
static void UFO_RemoveUfoFromGeoscape(aircraft_t* ufo) {
	int num;
	base_t* base;
	
	/* Remove ufo from ufos list */
	num = ufo - gd.ufos;
	if (num < 0 || num >= gd.numUfos)
		return Com_DPrintf("Cannot remove ufo: '%s'\n", ufo->name);
	Com_DPrintf("Remove ufo from geoscape: '%s'\n", ufo->name);
	memcpy(gd.ufos + num, gd.ufos + num + 1, (gd.numUfos - num - 1) * sizeof(aircraft_t));	
	gd.numUfos--;

	/* Remove ufo from bases radar */
	for (base = gd.bases + gd.numBases - 1 ; base >= gd.bases ; base--)
		RADAR_RemoveUfo(&(base->radar), ufo);

	/* Notications */
	CL_PopupNotifyUfoRemoved(ufo);
	CL_AircraftsNotifyUfoRemoved(ufo);
}

/**
  * @brief Remove an ufo from the geoscape
  */
static void UFO_RemoveUfoFromGeoscape_f(void)
{
	if (gd.numUfos > 0)
		UFO_RemoveUfoFromGeoscape(gd.ufos);
}

/**
 * @brief Check events for ufos : appears or disappears on radars
 */
extern void UFO_CampaignCheckEvents(void)
{
	qboolean visible;
	aircraft_t*	ufo;
	base_t* base;
	
	/* For each ufo in geoscape */
	for (ufo = gd.ufos + gd.numUfos - 1 ; ufo >= gd.ufos; ufo--) {
		visible = ufo->visible;
		ufo->visible = qfalse;

		/* Check for ufo detection by bases */
		for (base = gd.bases + gd.numBases - 1 ; base >= gd.bases ; base--)
			ufo->visible |= RADAR_CheckUfoSensored(&(base->radar), base->pos, ufo);

		/* Check if ufo appears or disappears on radar */
		if (visible != ufo->visible) {
			if (ufo->visible) {
				MN_AddNewMessage(_("Notice"), _("UFO appears on our radar"), qfalse, MSG_STANDARD, NULL);
				CL_GameTimeStop();
			}
			else {
				/* FIXME: grammar: from/of/on */
				MN_AddNewMessage(_("Notice"), _("UFO disappears on our radar"), qfalse, MSG_STANDARD, NULL);
				
				/* Notify that ufo disappeared */
				CL_AircraftsUfoDisappear(ufo);
				CL_PopupNotifyUfoDisappeared(ufo);
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
