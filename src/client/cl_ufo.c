/**
 * @file cl_ufo.c
 * @brief UFOs on geoscape
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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
static void UFO_RemoveUfoFromGeoscape_f(void);
extern void UFO_CampaignCheckEvents(void);
extern void UFO_Reset(void);

/**
 * @brief Translate UFO type to name.
 * @param[in] type UFO type in ufoType_t.
 * @return Translated UFO name.
 */
extern char* UFO_UfoTypeToName (ufoType_t type)
{
	switch(type) {
	case UFO_SCOUT:
		return _("UFO - Scout");
		break;
	case UFO_FIGHTER:
		return _("UFO - Fighter");
		break;
	case UFO_HARVESTER:
		return _("UFO - Harvester");
		break;
	case UFO_CONDOR:
		return _("UFO - Condor");
		break;
	case UFO_CARRIER:
		return _("UFO - Carrier");
		break;
	default:
		Sys_Error("UFO_UfoTypeToName(): Unknown UFO type %i\n", type);
		break;
	}
	return NULL; /* never reached */
}

/**
 * @brief Give a random destination to an ufo, and make him to move there
 */
static void UFO_SetUfoRandomDest (aircraft_t* ufo)
{
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
extern void UFO_CampaignRunUfos (int dt)
{
	aircraft_t*	ufo;

	/* now the ufos are flying around, too */
	for (ufo = gd.ufos + gd.numUfos - 1; ufo >= gd.ufos; ufo--) {
		if (AIR_AircraftMakeMove(dt, ufo))
			UFO_SetUfoRandomDest(ufo);

		/* @todo: Crash */
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
static void UFO_ListUfosOnGeoscape_f (void)
{
	aircraft_t* ufo;

	Com_Printf("There are %i ufos on geoscape\n", gd.numUfos);
	for (ufo = gd.ufos + gd.numUfos - 1; ufo >= gd.ufos; ufo--)
		Com_Printf("ufo: %s - status: %i - pos: %.0f:%0.f\n", gd.ufos[ufo - gd.ufos + 1].name, ufo->status, ufo->pos[0], ufo->pos[1]);
}
#endif

/**
  * @brief Add an ufo in geoscape
  *
  * @todo: Ufos are not assigned unique idx fields. Could be handy...
  * @sa UFO_RemoveUfoFromGeoscape
  * @sa UFO_RemoveUfoFromGeoscape_f
  */
static void UFO_NewUfoOnGeoscape_f (void)
{
	static int newUfoNum = -1;
	aircraft_t *ufo = NULL;
	ufoType_t ufotype = UFO_MAX;

	/* check max amount */
	if (gd.numUfos >= MAX_UFOONGEOSCAPE)
		return;

	if (Cmd_Argc() == 2) {
		ufotype = atoi(Cmd_Argv(1));
		if (ufotype > UFO_MAX || ufotype < 0)
			ufotype = UFO_MAX;
	}

	/* Get next type of ufo in aircrafts list */
	while (++newUfoNum < numAircraft_samples)
		if (aircraft_samples[newUfoNum].type == AIRCRAFT_UFO)
			break;
	if (newUfoNum == numAircraft_samples)
		for (newUfoNum = 0; newUfoNum < numAircraft_samples; newUfoNum++)
			if (aircraft_samples[newUfoNum].type == AIRCRAFT_UFO
			 && (ufotype == UFO_MAX || ufotype == aircraft_samples[newUfoNum].ufotype))
				break;
	if (newUfoNum == numAircraft_samples) {
		if (ufotype != UFO_MAX)
			Com_DPrintf("Could not add ufo type %i to geoscape\n", ufotype);
		return;
	}

	/* Create ufo */
	ufo = gd.ufos + gd.numUfos;
	memcpy(ufo, aircraft_samples + newUfoNum, sizeof(aircraft_t));
	Com_DPrintf("New ufo on geoscape: '%s' (%i)\n", ufo->name, gd.numUfos);
	gd.numUfos++;

	/* Initialise ufo data */
	ufo->visible = qfalse;					/* No visible in radars (just for now) */
	CP_GetRandomPosForAircraft(ufo->pos);	/* Random position */
	UFO_SetUfoRandomDest(ufo);				/* Random destination */
}

/**
 * @brief Remove the specified ufo from geoscape
 * @sa UFO_NewUfoOnGeoscape_f
 */
extern void UFO_RemoveUfoFromGeoscape (aircraft_t* ufo)
{
	int num;
	base_t* base;
	aircraft_t* aircraft;

	/* Remove ufo from ufos list */
	num = ufo - gd.ufos;
	if (num < 0 || num >= gd.numUfos) {
		Com_DPrintf("Cannot remove ufo: '%s'\n", ufo->name);
		return;
	}
	Com_DPrintf("Remove ufo from geoscape: '%s'\n", ufo->name);
	memcpy(gd.ufos + num, gd.ufos + num + 1, (gd.numUfos - num - 1) * sizeof(aircraft_t));
	gd.numUfos--;

	/* Remove ufo from bases and aircrafts radars */
	for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--) {
		Radar_NotifyUfoRemoved(&(base->radar), ufo);

		for (aircraft = base->aircraft + base->numAircraftInBase - 1; aircraft >= base->aircraft; aircraft--)
			Radar_NotifyUfoRemoved(&(aircraft->radar), ufo);
	}

	/* Notications */
	CL_PopupNotifyUfoRemoved(ufo);
	AIR_AircraftsNotifyUfoRemoved(ufo);
	MAP_NotifyUfoRemoved(ufo);
}

/**
  * @brief Remove an ufo from the geoscape
  */
static void UFO_RemoveUfoFromGeoscape_f (void)
{
	if (gd.numUfos > 0)
		UFO_RemoveUfoFromGeoscape(gd.ufos);
}

/**
 * @brief Check events for ufos : appears or disappears on radars
 */
extern void UFO_CampaignCheckEvents (void)
{
	qboolean visible;
	aircraft_t *ufo, *aircraft;
	base_t* base;

	/* For each ufo in geoscape */
	for (ufo = gd.ufos + gd.numUfos - 1; ufo >= gd.ufos; ufo--) {
		visible = ufo->visible;
		ufo->visible = qfalse;

		/* Check for ufo detection by bases */
		for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--)
			ufo->visible |= RADAR_CheckUfoSensored(&(base->radar), base->pos, ufo, visible);

		/* Check for ufo tracking by aircrafts */
		if (visible || ufo->visible)
			for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--)
				for (aircraft = base->aircraft + base->numAircraftInBase - 1; aircraft >= base->aircraft; aircraft--)
					ufo->visible |= RADAR_CheckUfoSensored(&(aircraft->radar), aircraft->pos, ufo, qtrue);

		/* Check if ufo appears or disappears on radar */
		if (visible != ufo->visible) {
			if (ufo->visible) {
				MN_AddNewMessage(_("Notice"), _("UFO appears from our radar"), qfalse, MSG_STANDARD, NULL);
				CL_GameTimeStop();
			}
			else {
				/* FIXME: grammar: from/of/on */
				MN_AddNewMessage(_("Notice"), _("UFO disappears on our radar"), qfalse, MSG_STANDARD, NULL);

				/* Notify that ufo disappeared */
				AIR_AircraftsUfoDisappear(ufo);
				CL_PopupNotifyUfoDisappeared(ufo);
				MAP_NotifyUfoDisappear(ufo);
			}
		}
	}
}

/**
 * @brief Prepares UFO recovery in global recoveries array.
 * @param[in] *base Pointer to the base, where the UFO recovery will be made.
 */
extern void UFO_PrepareRecovery (base_t *base)
{
	int i;
	ufoRecoveries_t *recovery = NULL;
	aircraft_t *ufocraft = NULL;
	date_t event;

	assert (base);

	/* Find ufo sample of given ufotype. */
	for (i = 0; i < numAircraft_samples; i++) {
		ufocraft = &aircraft_samples[i];
		if (ufocraft->type != AIRCRAFT_UFO)
			continue;
		if (ufocraft->ufotype == Cvar_VariableInteger("mission_ufotype"))
			break;
	}
	assert(ufocraft);

	/* Find free uforecovery slot. */
	for (i = 0; i < MAX_RECOVERIES; i++) {
		if (!gd.recoveries[i].active) {
			/* Make sure it is empty here. */
			memset(&gd.recoveries[i], 0, sizeof(gd.recoveries[i]));
			recovery = &gd.recoveries[i];
			break;
		}
	}

	if (!recovery) {
		Com_Printf("UFO_PrepareRecovery()... free recovery slot not found.\n");
		return;
	}
	assert(recovery);

	/* Prepare date of the recovery event - always two days after mission. */
	event = ccs.date;
	event.day += 2;
	/* Prepare recovery. */
	recovery->active = qtrue;
	recovery->baseID = base->idx;
	recovery->ufotype = ufocraft->idx_sample;
	recovery->event = event;

	Com_Printf("UFO_PrepareRecovery()... the recovery entry in global array is done; base: %s, ufotype: %i, date: %i\n",
	gd.bases[recovery->baseID].name, recovery->ufotype, recovery->event.day);
}

/**
 * @brief Function to process active recoveries.
 */
extern void UFO_Recovery (void)
{
	int i, item;
	objDef_t *od;
	base_t *base;
	aircraft_t *ufocraft;
	ufoRecoveries_t *recovery;

	for (i = 0; i < MAX_RECOVERIES; i++) {
		recovery = &gd.recoveries[i];
		if (recovery->event.day == ccs.date.day) {
			base = &gd.bases[recovery->baseID];
			if (!base->founded) {
				/* Destination base was destroyed meanwhile. */
				/* UFO is lost, send proper message to the user.*/
				recovery->active = qfalse;
				/* @todo: prepare MN_AddNewMessage() here */
				return;
			}
			/* Get ufocraft. */
			ufocraft = &aircraft_samples[recovery->ufotype];
			assert(ufocraft);
			/* Get item. */
			/* We can do this because aircraft id is the same as dummy item id. */
			item = Com_GetItemByID(ufocraft->id);
			od = &csi.ods[item];
			assert(od);
			/* Process UFO recovery. */
			base->storage.num[item]++;	/* Add dummy UFO item to enable research topic. */
			RS_MarkCollected(od->tech);	/* Enable research topic. */
		}
	}
}

/**
 * @brief
 * @sa MN_ResetMenus
 */
extern void UFO_Reset (void)
{
	Cmd_AddCommand("addufo", UFO_NewUfoOnGeoscape_f, "Add a new ufo to geoscape");
	Cmd_AddCommand("removeufo", UFO_RemoveUfoFromGeoscape_f, "Remove an ufo from geoscape");
#ifdef DEBUG
	Cmd_AddCommand("debug_listufo", UFO_ListUfosOnGeoscape_f, "Print ufo information to game console");
	Cvar_Get("debug_showufos", "0", 0, "Show all ufos on geoscape");
#endif
}
