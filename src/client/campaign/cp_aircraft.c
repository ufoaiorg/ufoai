/**
 * @file cp_aircraft.c
 * @brief Most of the aircraft related stuff.
 * @sa cl_airfight.c
 * @note Aircraft management functions prefix: AIR_
 * @note Aircraft menu(s) functions prefix: AIM_
 * @note Aircraft equipement handling functions prefix: AII_
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion team.

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

#include "../client.h"
#include "../cl_global.h"
#include "../cl_team.h"
#include "../menu/m_popup.h"
#include "../renderer/r_draw.h"
#include "cl_mapfightequip.h"
#include "cl_map.h"
#include "cl_ufo.h"
#include "cl_alienbase.h"
#include "cp_time.h"
#include "cp_missions.h"
#include "../../shared/parse.h"


aircraft_t aircraftTemplates[MAX_AIRCRAFT];		/**< Available aircraft types/templates/samples. */
/**
 * Number of aircraft templates.
 * @todo Should be reset to 0 each time scripts are read anew; also
 * aircraftTemplates memory should be freed at that time,
 * or old memory used for new records
 */
int numAircraftTemplates = 0;

/**
 * @brief Updates hangar capacities for one aircraft in given base.
 * @param[in] aircraftTemplate Aircraft template.
 * @param[in] base Pointer to base.
 * @return AIRCRAFT_HANGAR_BIG if aircraft was placed in big hangar
 * @return AIRCRAFT_HANGAR_SMALL if small
 * @return AIRCRAFT_HANGAR_ERROR if error or not possible
 * @sa AIR_NewAircraft
 * @sa AIR_UpdateHangarCapForAll
 */
static int AIR_UpdateHangarCapForOne (aircraft_t *aircraftTemplate, base_t *base)
{
	int aircraftSize, freespace = 0;

	assert(aircraftTemplate);
	assert(aircraftTemplate == aircraftTemplate->tpl);	/* Make sure it's an aircraft template. */

	aircraftSize = aircraftTemplate->size;

	if (aircraftSize < AIRCRAFT_SMALL) {
#ifdef DEBUG
		Com_Printf("AIR_UpdateHangarCapForOne: aircraft weight is wrong!\n");
#endif
		return AIRCRAFT_HANGAR_ERROR;
	}
	if (!base) {
#ifdef DEBUG
		Com_Printf("AIR_UpdateHangarCapForOne: base does not exist!\n");
#endif
		return AIRCRAFT_HANGAR_ERROR;
	}
	assert(base);
	if (!B_GetBuildingStatus(base, B_HANGAR) && !B_GetBuildingStatus(base, B_SMALL_HANGAR)) {
		Com_Printf("AIR_UpdateHangarCapForOne: base does not have any hangar - error!\n");
		return AIRCRAFT_HANGAR_ERROR;
	}

	if (aircraftSize >= AIRCRAFT_LARGE) {
		if (!B_GetBuildingStatus(base, B_HANGAR)) {
			Com_Printf("AIR_UpdateHangarCapForOne: base does not have big hangar - error!\n");
			return AIRCRAFT_HANGAR_ERROR;
		}
		freespace = base->capacities[CAP_AIRCRAFT_BIG].max - base->capacities[CAP_AIRCRAFT_BIG].cur;
		if (freespace > 0) {
			base->capacities[CAP_AIRCRAFT_BIG].cur++;
			return AIRCRAFT_HANGAR_BIG;
		} else {
			/* No free space for this aircraft. This should never happen here. */
			Com_Printf("AIR_UpdateHangarCapForOne: no free space!\n");
			return AIRCRAFT_HANGAR_ERROR;
		}
	} else {
		if (!B_GetBuildingStatus(base, B_SMALL_HANGAR)) {
			Com_Printf("AIR_UpdateHangarCapForOne: base does not have small hangar - error!\n");
			return AIRCRAFT_HANGAR_ERROR;
		}
		freespace = base->capacities[CAP_AIRCRAFT_SMALL].max - base->capacities[CAP_AIRCRAFT_SMALL].cur;
		if (freespace > 0) {
			base->capacities[CAP_AIRCRAFT_SMALL].cur++;
			return AIRCRAFT_HANGAR_SMALL;
		} else {
			/* No free space for this aircraft. This should never happen here. */
			Com_Printf("AIR_UpdateHangarCapForOne: no free space!\n");
			return AIRCRAFT_HANGAR_ERROR;
		}
	}
}

/**
 * @brief Updates current capacities for hangars in given base.
 * @param[in] base The base we want to update.
 * @note Call this function whenever you sell/loss aircraft in given base.
 * @sa BS_SellAircraft_f
 */
void AIR_UpdateHangarCapForAll (base_t *base)
{
	int i;

	if (!base) {
#ifdef DEBUG
		Com_Printf("AIR_UpdateHangarCapForAll: base does not exist!\n");
#endif
		return;
	}

	/* Reset current capacities for hangar. */
	base->capacities[CAP_AIRCRAFT_BIG].cur = 0;
	base->capacities[CAP_AIRCRAFT_SMALL].cur = 0;

	for (i = 0; i < base->numAircraftInBase; i++) {
		const aircraft_t *aircraft = &base->aircraft[i];
		Com_DPrintf(DEBUG_CLIENT, "AIR_UpdateHangarCapForAll: base: %s, aircraft: %s\n", base->name, aircraft->id);
		AIR_UpdateHangarCapForOne(aircraft->tpl, base);
	}
	Com_DPrintf(DEBUG_CLIENT, "AIR_UpdateHangarCapForAll: base capacities.cur: small: %i big: %i\n", base->capacities[CAP_AIRCRAFT_SMALL].cur, base->capacities[CAP_AIRCRAFT_BIG].cur);
}

#ifdef DEBUG
/**
 * @brief Debug function which lists all aircraft in all bases.
 * @note Use with baseIdx as a parameter to display all aircraft in given base.
 * @note called with debug_listaircraft
 */
void AIR_ListAircraft_f (void)
{
	int i, k, baseIdx;
	base_t *base;
	aircraft_t *aircraft;
	employee_t *employee;
	character_t *chr;

	if (Cmd_Argc() == 2)
		baseIdx = atoi(Cmd_Argv(1));

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;

		Com_Printf("Aircraft in base %s: %i\n", base->name, base->numAircraftInBase);
		for (i = 0; i < base->numAircraftInBase; i++) {
			aircraft = &base->aircraft[i];
			Com_Printf("Aircraft %s\n", aircraft->name);
			Com_Printf("...idx cur/global %i/%i\n", i, aircraft->idx);
			Com_Printf("...homebase: %s\n", aircraft->homebase ? aircraft->homebase->name : "NO HOMEBASE");
			for (k = 0; k < aircraft->maxWeapons; k++) {
				if (aircraft->weapons[k].item) {
					Com_Printf("...weapon slot %i contains %s", k, aircraft->weapons[k].item->id);
					if (!aircraft->weapons[k].installationTime)
						Com_Printf(" (functional)\n");
					else if (aircraft->weapons[k].installationTime > 0)
						Com_Printf(" (%i hours before installation is finished)\n",aircraft->weapons[k].installationTime);
					else
						Com_Printf(" (%i hours before removing is finished)\n",aircraft->weapons[k].installationTime);
					if (aircraft->weapons[k].ammo)
						if (aircraft->weapons[k].ammoLeft > 1)
							Com_Printf("......this weapon is loaded with ammo %s\n", aircraft->weapons[k].ammo->id);
						else
							Com_Printf("......no more ammo (%s)\n", aircraft->weapons[k].ammo->id);
					else
						Com_Printf("......this weapon isn't loaded with ammo\n");
				}
				else
					Com_Printf("...weapon slot %i is empty\n", k);
			}
			if (aircraft->shield.item) {
				Com_Printf("...armour slot contains %s", aircraft->shield.item->id);
				if (!aircraft->shield.installationTime)
						Com_Printf(" (functional)\n");
					else if (aircraft->shield.installationTime > 0)
						Com_Printf(" (%i hours before installation is finished)\n",aircraft->shield.installationTime);
					else
						Com_Printf(" (%i hours before removing is finished)\n",aircraft->shield.installationTime);
			} else
				Com_Printf("...armour slot is empty\n");
			for (k = 0; k < aircraft->maxElectronics; k++) {
				if (aircraft->electronics[k].item) {
					Com_Printf("...electronics slot %i contains %s", k, aircraft->electronics[k].item->id);
					if (!aircraft->electronics[k].installationTime)
						Com_Printf(" (functional)\n");
					else if (aircraft->electronics[k].installationTime > 0)
						Com_Printf(" (%i hours before installation is finished)\n",aircraft->electronics[k].installationTime);
					else
						Com_Printf(" (%i hours before removing is finished)\n",aircraft->electronics[k].installationTime);
				}
				else
					Com_Printf("...electronics slot %i is empty\n", k);
			}
			if (aircraft->pilot) {
				Com_Printf("...pilot: idx: %i name: %s\n", aircraft->pilot->idx, aircraft->pilot->chr.name);
			} else {
				Com_Printf("...no pilot assigned\n");
			}
			Com_Printf("...damage: %i\n", aircraft->damage);
			Com_Printf("...stats: ");
			for (k = 0; k < AIR_STATS_MAX; k++) {
				if (k == AIR_STATS_WRANGE)
					Com_Printf("%.2f ", aircraft->stats[k] / 1000.0f);
				else
					Com_Printf("%i ", aircraft->stats[k]);
			}
			Com_Printf("\n");
			Com_Printf("...name %s\n", aircraft->id);
			Com_Printf("...type %i\n", aircraft->type);
			Com_Printf("...size %i\n", aircraft->maxTeamSize);
			Com_Printf("...fuel %i\n", aircraft->fuel);
			Com_Printf("...status %s\n", AIR_AircraftStatusToName(aircraft));
			Com_Printf("...pos %.0f:%.0f\n", aircraft->pos[0], aircraft->pos[1]);
			Com_Printf("...team: (%i/%i)\n", aircraft->teamSize, aircraft->maxTeamSize);
			for (k = 0; k < aircraft->maxTeamSize; k++)
				if (aircraft->acTeam[k]) {
					employee = aircraft->acTeam[k];
					Com_Printf("......idx (in global array): %i\n", employee->idx);
					chr = &employee->chr;
					if (chr)
						Com_Printf(".........name: %s\n", chr->name);
					else
						Com_Printf(".........ERROR: Could not get character for employee %i\n", employee->idx);
				}
		}
	}
}
#endif

static equipDef_t eTempEq;		/**< Used to count ammo in magazines. */

/**
 * @brief Count and collect ammo from gun magazine.
 * @param[in] magazine Pointer to invList_t being magazine.
 * @param[in] aircraft Pointer to aircraft used in this mission.
 * @sa AII_CollectingItems
 */
static void AII_CollectingAmmo (aircraft_t *aircraft, const invList_t *magazine)
{
	/* Let's add remaining ammo to market. */
	eTempEq.numLoose[magazine->item.m->idx] += magazine->item.a;
	if (eTempEq.numLoose[magazine->item.m->idx] >= magazine->item.t->ammo) {
		/* There are more or equal ammo on the market than magazine needs - collect magazine. */
		eTempEq.numLoose[magazine->item.m->idx] -= magazine->item.t->ammo;
		AII_CollectItem(aircraft, magazine->item.m, 1);
	}
}

/**
 * @brief Add an item to aircraft inventory.
 * @sa AL_AddAliens
 * @sa AII_CollectingItems
 */
void AII_CollectItem (aircraft_t *aircraft, const objDef_t *item, int amount)
{
	int i;
	itemsTmp_t *cargo = aircraft->itemcargo;

	for (i = 0; i < aircraft->itemtypes; i++) {
		if (cargo[i].item == item) {
			Com_DPrintf(DEBUG_CLIENT, "AII_CollectItem: collecting %s (%i) amount %i -> %i\n", item->name, item->idx, cargo[i].amount, cargo[i].amount + amount);
			cargo[i].amount += amount;
			return;
		}
	}
	Com_DPrintf(DEBUG_CLIENT, "AII_CollectItem: adding %s (%i) amount %i\n", item->name, item->idx, amount);
	cargo[i].item = item;
	cargo[i].amount = amount;
	aircraft->itemtypes++;
}


/**
 * @brief Collect items from the battlefield.
 * @note The way we are collecting items:
 * @note 1. Grab everything from the floor and add it to the aircraft cargo here.
 * @note 2. When collecting gun, collect it's remaining ammo as well
 * @note 3. Sell everything or add to base storage when dropship lands in base.
 * @note 4. Items carried by soldiers are processed nothing is being sold.
 * @sa CL_ParseResults
 * @sa AII_CollectingAmmo
 * @sa INV_SellorAddAmmo
 * @sa INV_CarriedItems
 */
void AII_CollectingItems (aircraft_t *aircraft, int won)
{
	int i, j;
	le_t *le;
	invList_t *item;
	itemsTmp_t *cargo;
	itemsTmp_t previtemcargo[MAX_CARGO];
	int previtemtypes;

	/* Save previous cargo */
	memcpy(previtemcargo, aircraft->itemcargo, sizeof(aircraft->itemcargo));
	previtemtypes = aircraft->itemtypes;
	/* Make sure itemcargo is empty. */
	memset(aircraft->itemcargo, 0, sizeof(aircraft->itemcargo));

	/* Make sure eTempEq is empty as well. */
	memset(&eTempEq, 0, sizeof(eTempEq));

	cargo = aircraft->itemcargo;
	aircraft->itemtypes = 0;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		/* Winner collects everything on the floor, and everything carried
		 * by surviving actors. Loser only gets what their living team
		 * members carry. */
		if (!le->inuse)
			continue;
		switch (le->type) {
		case ET_ITEM:
			if (won) {
				for (item = FLOOR(le); item; item = item->next) {
					AII_CollectItem(aircraft, item->item.t, 1);
					if (item->item.t->reload && item->item.a > 0)
						AII_CollectingAmmo(aircraft, item);
				}
			}
			break;
		case ET_ACTOR:
		case ET_ACTOR2x2:
			/* First of all collect armour of dead or stunned actors (if won). */
			if (won) {
				if (LE_IsDead(le) || LE_IsStunned(le)) {
					if (le->i.c[csi.idArmour]) {
						item = le->i.c[csi.idArmour];
						AII_CollectItem(aircraft, item->item.t, 1);
					}
					break;
				}
			}
			/* Now, if this is dead or stunned actor, additional check. */
			if (le->team != cls.team || LE_IsDead(le) || LE_IsStunned(le)) {
				/* The items are already dropped to floor and are available
				 * as ET_ITEM; or the actor is not ours. */
				break;
			}
			/* Finally, the living actor from our team. */
			INV_CarriedItems(le);
			break;
		default:
			break;
		}
	}
	/* Fill the missionresults array. */
	missionresults.itemtypes = aircraft->itemtypes;
	for (i = 0; i < aircraft->itemtypes; i++)
		missionresults.itemamount += cargo[i].amount;

#ifdef DEBUG
	/* Print all of collected items. */
	for (i = 0; i < aircraft->itemtypes; i++) {
		if (cargo[i].amount > 0)
			Com_DPrintf(DEBUG_CLIENT, "Collected items: idx: %i name: %s amount: %i\n", cargo[i].item->idx, cargo[i].item->name, cargo[i].amount);
	}
#endif

	/* Put previous cargo back */
	for (i = 0; i < previtemtypes; i++) {
		for (j = 0; j < aircraft->itemtypes; j++) {
			if (cargo[j].item == previtemcargo[i].item) {
				cargo[j].amount += previtemcargo[i].amount;
				break;
			}
		}
		if (j == aircraft->itemtypes) {
			cargo[j] = previtemcargo[i];
			aircraft->itemtypes++;
		}
	}
}

/**
 * @brief Translates the aircraft status id to a translateable string
 * @param[in] aircraft Aircraft to translate the status of
 * @return Translation string of given status.
 * @note Called in: CL_AircraftList_f(), AIR_ListAircraft_f(), AIR_AircraftSelect(),
 * @note MAP_DrawMap(), CL_DisplayPopupIntercept()
 */
const char *AIR_AircraftStatusToName (const aircraft_t * aircraft)
{
	assert(aircraft);
	assert(aircraft->homebase);

	/* display special status if base-attack affects aircraft */
	if (aircraft->homebase->baseStatus == BASE_UNDER_ATTACK &&
		AIR_IsAircraftInBase(aircraft))
		return _("ON RED ALERT");

	switch (aircraft->status) {
	case AIR_NONE:
		return _("Nothing - should not be displayed");
	case AIR_HOME:
		return _("at home base");
	case AIR_REFUEL:
		return _("refuelling");
	case AIR_IDLE:
		return _("idle");
	case AIR_TRANSIT:
		return _("in transit");
	case AIR_MISSION:
		return _("enroute to mission");
	case AIR_UFO:
		return _("pursuing a UFO");
	case AIR_DROP:
		return _("ready to drop soldiers");
	case AIR_INTERCEPT:
		return _("intercepting a UFO");
	case AIR_TRANSFER:
		return _("enroute to new home base");
	case AIR_RETURNING:
		return _("returning to base");
	default:
		Com_Printf("Error: Unknown aircraft status for %s\n", aircraft->name);
	}
	return NULL;
}

/**
 * @brief Checks whether given aircraft is in its homebase.
 * @param[in] aircraft Pointer to an aircraft.
 * @return qtrue if given aircraft is in its homebase.
 * @return qfalse if given aircraft is not in its homebase.
 * @todo Add check for AIR_REARM when aircraft items will be implemented.
 */
qboolean AIR_IsAircraftInBase (const aircraft_t * aircraft)
{
	if (aircraft->status == AIR_HOME || aircraft->status == AIR_REFUEL)
		return qtrue;
	return qfalse;
}

/**
 * @brief Checks whether given aircraft is on geoscape.
 * @param[in] aircraft Pointer to an aircraft.
 * @note aircraft may be neither on geoscape, nor in base (like when it's transferred)
 * @return qtrue if given aircraft is on geoscape.
 * @return qfalse if given aircraft is not on geoscape.
 */
qboolean AIR_IsAircraftOnGeoscape (const aircraft_t * aircraft)
{
	switch (aircraft->status) {
	case AIR_IDLE:
	case AIR_TRANSIT:
	case AIR_MISSION:
	case AIR_UFO:
	case AIR_DROP:
	case AIR_INTERCEPT:
	case AIR_RETURNING:
		return qtrue;
	case AIR_NONE:
	case AIR_REFUEL:
	case AIR_HOME:
	case AIR_TRANSFER:
		return qfalse;
	}

	assert(0);
	return qfalse;
}

/**
 * @brief Calculates the amount of aircraft (of the given type) in the selected base
 * @param[in] base The base to count the aircraft in
 * @param[in] aircraftType The type of the aircraft you want
 */
int AIR_CountTypeInBase (const base_t *base, aircraftType_t aircraftType)
{
	int i, count = 0;

	assert(base);

	for (i = 0; i < base->numAircraftInBase; i++) {
		if (base->aircraft[i].type == aircraftType)
			count++;
	}

	return count;
}

/**
 * @brief Returns the string that matches the given aircraft type
 */
const char *AIR_GetAircraftString (aircraftType_t aircraftType)
{
	switch (aircraftType) {
	case AIRCRAFT_INTERCEPTOR:
		return _("Interceptor");
	case AIRCRAFT_TRANSPORTER:
		return _("Transporter");
	case AIRCRAFT_UFO:
		return _("UFO");
	}
	return "";
}

/**
 * @brief Some of the aircraft values needs special calculations when they
 * are shown in the menus
 * @sa CL_AircraftStatToName
 */
int CL_AircraftMenuStatsValues (const int value, const int stat)
{
	switch (stat) {
	case AIR_STATS_SPEED:
	case AIR_STATS_MAXSPEED:
		/* Convert into km/h, and round this value */
		return 10 * (int) (111.2 * value / 10.0f);
	case AIR_STATS_FUELSIZE:
		return value / 1000;
	case AIR_STATS_OP_RANGE:
		/* the 2.0f factor is for going to destination and then come back */
		return 100 * (int) (KILOMETER_PER_DEGREE * value / (2.0f * (float)SECONDS_PER_HOUR * 100.0f));
	default:
		return value;
	}
}

/**
 * @brief check if aircraft has enough fuel to go to destination, and then come back home
 * @param[in] aircraft Pointer to the aircraft
 * @param[in] destination Pointer to the position the aircraft should go to
 * @sa MAP_MapCalcLine
 * @return qtrue if the aircraft can go to the position, qfalse else
 */
qboolean AIR_AircraftHasEnoughFuel (const aircraft_t *aircraft, const vec2_t destination)
{
	base_t *base;
	float distance = 0;

	assert(aircraft);
	base = (base_t *) aircraft->homebase;
	assert(base);

	/* Calculate the line that the aircraft should follow to go to destination */
	distance = MAP_GetDistance(aircraft->pos, destination);

	/* Calculate the line that the aircraft should then follow to go back home */
	distance += MAP_GetDistance(destination, base->pos);

	/* Check if the aircraft has enough fuel to go to destination and then go back home */
	if (distance <= aircraft->stats[AIR_STATS_SPEED] * aircraft->fuel / (float)SECONDS_PER_HOUR)
		return qtrue;

	return qfalse;
}

/**
 * @brief check if aircraft has enough fuel to go to destination
 * @param[in] aircraft Pointer to the aircraft
 * @param[in] destination Pointer to the position the aircraft should go to
 * @sa MAP_MapCalcLine
 * @return qtrue if the aircraft can go to the position, qfalse else
 */
qboolean AIR_AircraftHasEnoughFuelOneWay (const aircraft_t const *aircraft, const vec2_t destination)
{
	base_t *base;
	float distance = 0;

	assert(aircraft);
	base = (base_t *) aircraft->homebase;
	assert(base);

	/* Calculate the line that the aircraft should follow to go to destination */
	distance = MAP_GetDistance(aircraft->pos, destination);

	/* Check if the aircraft has enough fuel to go to destination and then go back home */
	if (distance <= aircraft->stats[AIR_STATS_SPEED] * aircraft->fuel / 3600.0f)
		return qtrue;

	return qfalse;
}

/**
 * @brief Calculates the way back to homebase for given aircraft and returns it.
 * @param[in] aircraft Pointer to aircraft, which should return to base.
 * @note Command to call this: "aircraft_return".
 */
void AIR_AircraftReturnToBase (aircraft_t *aircraft)
{
	base_t *base;

	if (aircraft && AIR_IsAircraftOnGeoscape(aircraft)) {
		base = aircraft->homebase;
		assert(base);
		Com_DPrintf(DEBUG_CLIENT, "return '%s' (%i) to base ('%s').\n", aircraft->name, aircraft->idx, base->name);
		MAP_MapCalcLine(aircraft->pos, base->pos, &aircraft->route);
		aircraft->status = AIR_RETURNING;
		aircraft->time = 0;
		aircraft->point = 0;
		aircraft->mission = NULL;
	}
}

/**
 * @brief Returns the index of the aircraft in the base->aircraft array.
 * @param[in] aircraft The aircraft to get the index for.
 * @return The array index or AIRCRAFT_INBASE_INVALID on error.
 */
int AIR_GetAircraftIdxInBase (const aircraft_t* aircraft)
{
	int i;
	const base_t *base;

	if (!aircraft || !aircraft->homebase)
		return AIRCRAFT_INBASE_INVALID;

	base = aircraft->homebase;

	for (i = 0; i < base->numAircraftInBase; i++) {
		if (&base->aircraft[i] == aircraft)
			return i;
	}

	/* Aircraft not found in base */
	return AIRCRAFT_INBASE_INVALID;
}

/**
 * @brief Searches the global array of aircraft types for a given aircraft.
 * @param[in] name Aircraft id.
 * @return aircraft_t pointer or NULL if not found.
 */
aircraft_t *AIR_GetAircraft (const char *name)
{
	int i;

	assert(name);
	for (i = 0; i < numAircraftTemplates; i++) {
		if (!Q_strncmp(aircraftTemplates[i].id, name, MAX_VAR))
			return &aircraftTemplates[i];
	}

	Com_Printf("Aircraft '%s' not found (%i).\n", name, numAircraftTemplates);
	return NULL;
}

/**
 * @brief Initialise aircraft pointer in each slot of an aircraft.
 * @param[in] aircraft Pointer to the aircraft where slots are.
 */
static void AII_SetAircraftInSlots (aircraft_t *aircraft)
{
	int i;

	/* initialise weapon slots */
	for (i = 0; i < MAX_AIRCRAFTSLOT; i++) {
		aircraft->weapons[i].aircraft = aircraft;
		aircraft->electronics[i].aircraft = aircraft;
	}
		aircraft->shield.aircraft = aircraft;
}

/**
 * @brief Places a new aircraft in the given base.
 * @param[in] base Pointer to base where aircraft should be added.
 * @param[in] name Id of aircraft to add (aircraft->id).
 * @sa B_Load
 */
aircraft_t* AIR_NewAircraft (base_t *base, const char *name)
{
	aircraft_t *aircraft;
	const aircraft_t *aircraftTemplate = AIR_GetAircraft(name);

	if (!aircraftTemplate) {
		Com_Printf("Could not find aircraft with id: '%s'\n", name);
		return NULL;
	}

	assert(base);

	if (base->numAircraftInBase < MAX_AIRCRAFT) {
		/* copy generic aircraft description to individal aircraft in base */
		/* we do this because every aircraft can have its own parameters */
		base->aircraft[base->numAircraftInBase] = *aircraftTemplate;
		/* now lets use the aircraft array for the base to set some parameters */
		aircraft = &base->aircraft[base->numAircraftInBase];
		aircraft->idx = gd.numAircraft;	/**< set a unique index to this aircraft. */
		aircraft->homebase = base;
		/* Update the values of its stats */
		AII_UpdateAircraftStats(aircraft);
		/* initialise aircraft pointer in slots */
		AII_SetAircraftInSlots(aircraft);
		/* give him some fuel */
		aircraft->fuel = aircraft->stats[AIR_STATS_FUELSIZE];
		/* Set HP to maximum */
		aircraft->damage = aircraft->stats[AIR_STATS_DAMAGE];

		/* set initial direction of the aircraft */
		VectorSet(aircraft->direction, 1, 0, 0);

		AIR_ResetAircraftTeam(aircraft);

		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("A new (a %s) class craft is ready in base %s"), _(aircraft->name), base->name);
		MS_AddNewMessage(_("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
		Com_DPrintf(DEBUG_CLIENT, "Setting aircraft to pos: %.0f:%.0f\n", base->pos[0], base->pos[1]);
		Vector2Copy(base->pos, aircraft->pos);
		RADAR_Initialise(&aircraft->radar, RADAR_AIRCRAFTRANGE, RADAR_AIRCRAFTTRACKINGRANGE, 1.0f, qfalse);

		gd.numAircraft++;		/**< Increase the global number of aircraft. */
		base->numAircraftInBase++;	/**< Increase the number of aircraft in the base. */
		/* Update base capacities. */
		Com_DPrintf(DEBUG_CLIENT, "idx_sample: %i name: %s weight: %i\n", aircraft->tpl->idx, aircraft->id, aircraft->size);
		Com_DPrintf(DEBUG_CLIENT, "Adding new aircraft %s with IDX %i for base %s\n", aircraft->name, aircraft->idx, base->name);
		if (!base->aircraftCurrent)
			base->aircraftCurrent = aircraft;
		aircraft->hangar = AIR_UpdateHangarCapForOne(aircraft->tpl, base);
		if (aircraft->hangar == AIRCRAFT_HANGAR_ERROR)
			Com_Printf("AIR_NewAircraft: ERROR, new aircraft but no free space in hangars!\n");
		/* also update the base menu buttons */
		B_BaseMenuInit(base);
		return aircraft;
	}
	return NULL;
}

int AIR_GetCapacityByAircraftWeight (const aircraft_t *aircraft)
{
	switch (aircraft->size) {
	case AIRCRAFT_SMALL:
		return CAP_AIRCRAFT_SMALL;
	case AIRCRAFT_LARGE:
		return CAP_AIRCRAFT_BIG;
	}
	Sys_Error("AIR_GetCapacityByAircraftWeight: Unkown weight of aircraft '%i'\n", aircraft->size);
}

/**
 * @brief Calculate used storage room corresponding to items in an aircraft.
 * @param[in] aircraft Pointer to the aircraft.
 */
static int AIR_GetStorageRoom (const aircraft_t *aircraft)
{
	invList_t *ic;
	int i, container;
	int size = 0;

	for (i = 0; i < aircraft->maxTeamSize; i++) {
		if (aircraft->acTeam[i]) {
			const employee_t const *employee = aircraft->acTeam[i];
			for (container = 0; container < csi.numIDs; container++) {
				for (ic = employee->chr.inv.c[container]; ic; ic = ic->next) {
					objDef_t *obj = ic->item.t;
					size += obj->size;

					obj = ic->item.m;
					if (obj)
						size += obj->size;
				}
			}
		}
	}

	return size;
}

const char *AIR_CheckMoveIntoNewHomebase (const aircraft_t *aircraft, const base_t* base, const int capacity)
{
	if (!B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(capacity)))
		return _("No operational hangars at that base.");

	/* not enough capacity */
	if (base->capacities[capacity].cur >= base->capacities[capacity].max)
		return _("No free hangars at that base.");

	if (aircraft->maxTeamSize + base->capacities[CAP_EMPLOYEES].cur >  base->capacities[CAP_EMPLOYEES].max)
		return _("Insufficient free crew quarter space at that base.");

	if (aircraft->maxTeamSize && base->capacities[CAP_ITEMS].cur + AIR_GetStorageRoom(aircraft) > base->capacities[CAP_ITEMS].max)
		return _("Insufficient storage space at that base.");

	/* check aircraft fuel, because the aircraft has to travel to the new base */
	if (!AIR_AircraftHasEnoughFuelOneWay(aircraft, base->pos))
		return _("That base is beyond this aircraft's range.");

	return NULL;
}

/**
 * @brief Transfer items carried by a soldier from one base to another.
 * @param[in] employee Pointer to employee.
 * @param[in] sourceBase Base where employee comes from.
 * @param[in] destBase Base where employee is going.
 * @todo this is campaign mode only - doesn't belong here
 */
static void AIR_TransferItemsCarriedByCharacterToBase (character_t *chr, base_t *sourceBase, base_t* destBase)
{
	invList_t *ic;
	int container;

	for (container = 0; container < csi.numIDs; container++) {
		for (ic = chr->inv.c[container]; ic; ic = ic->next) {
			objDef_t *obj = ic->item.t;
			B_UpdateStorageAndCapacity(sourceBase, obj, -1, qfalse, qfalse);
			B_UpdateStorageAndCapacity(destBase, obj, 1, qfalse, qfalse);

			obj = ic->item.m;
			if (obj) {
				B_UpdateStorageAndCapacity(sourceBase, obj, -1, qfalse, qfalse);
				B_UpdateStorageAndCapacity(destBase, obj, 1, qfalse, qfalse);
			}
		}
	}
}

/**
 * @brief Moves a given aircraft to a new base (also the employees and inventory)
 * @note Also checks for a working hangar
 * @param[in] aircraft The aircraft to move into a new base
 * @param[in] base The base to move the aircraft into
 */
qboolean AIR_MoveAircraftIntoNewHomebase (aircraft_t *aircraft, base_t *base)
{
	base_t *oldBase;
	baseCapacities_t capacity;
	int i;

	assert(aircraft);
	assert(base);
	assert(base != aircraft->homebase);

	Com_DPrintf(DEBUG_CLIENT, "AIR_MoveAircraftIntoNewHomebase: Change homebase of '%s' to '%s'\n", aircraft->id, base->name);

	capacity = AIR_GetCapacityByAircraftWeight(aircraft);
	if (AIR_CheckMoveIntoNewHomebase(aircraft, base, capacity))
		return qfalse;

	oldBase = aircraft->homebase;
	assert(oldBase);

	/* Transfer employees */
	for (i = 0; i < aircraft->maxTeamSize; i++) {
		if (aircraft->acTeam[i]) {
			employee_t *employee = aircraft->acTeam[i];
			assert(employee);
			employee->baseHired = base;
			base->capacities[CAP_EMPLOYEES].cur++;
			oldBase->capacities[CAP_EMPLOYEES].cur--;
			/* Transfer items carried by soldiers from oldBase to base */
			AIR_TransferItemsCarriedByCharacterToBase(&employee->chr, oldBase, base);
		}
	}

	/* Move aircraft to new base */
	base->aircraft[base->numAircraftInBase] = *aircraft;
	base->capacities[capacity].cur++;
	base->numAircraftInBase++;

	/* Remove aircraft from old base */
	oldBase->numAircraftInBase--;
	oldBase->capacities[capacity].cur--;
	i = AIR_GetAircraftIdxInBase(aircraft);
	/* move other aircraft if the deleted aircraft was not the last one of the base */
	if (i != AIRCRAFT_INBASE_INVALID)
		memmove(aircraft, aircraft + 1, (oldBase->numAircraftInBase - i) * sizeof(*aircraft));
	/* wipe the now vacant last slot */
	memset(&oldBase->aircraft[oldBase->numAircraftInBase], 0, sizeof(oldBase->aircraft[oldBase->numAircraftInBase]));

	/* Reset aircraft */
	aircraft = &base->aircraft[base->numAircraftInBase - 1];

	/* Change homebase of aircraft */
	aircraft->homebase = base;

	/* No need to update global IDX of every aircraft: the global IDX of this aircraft did not change */

	return qtrue;
}

/**
 * @brief Removes an aircraft from its base and the game.
 * @param[in] aircraft Pointer to aircraft that should be removed.
 * @note The assigned soldiers (if any) are removed/unassinged from the aircraft - not fired.
 * @sa AIR_DestroyAircraft
 * @note If you want to do something different (kill, fire, etc...) do it before calling this function.
 * @todo Return status of deletion for better error handling.
 * @note This function has the side effect, that the base aircraft number is
 * reduced by one, also the gd.employees pointers are
 * moved to fill the gap of the removed employee. Thus pointers like acTeam in
 * the aircraft can point to wrong employees now. This has to be taken into
 * account
 */
void AIR_DeleteAircraft (base_t *base, aircraft_t *aircraft)
{
	int i, j;
	transfer_t *transfer;
	/* Check if aircraft is on geoscape while it's not destroyed yet */
	const qboolean aircraftIsOnGeoscape = AIR_IsAircraftOnGeoscape(aircraft);

	assert(aircraft);
	base = aircraft->homebase;
	assert(base);

	MAP_NotifyAircraftRemoved(aircraft, qtrue);
	TR_NotifyAircraftRemoved(aircraft);

	/* Remove all soldiers from the aircraft (the employees are still hired after this). */
	if (aircraft->teamSize > 0)
		AIR_RemoveEmployees(aircraft);

	/* base is NULL here because we don't want to readd this to the inventory
	 * If you want this in the inventory you really have to call these functions
	 * before you are destroying a craft */
	for (i = 0; i < MAX_AIRCRAFTSLOT; i++) {
		AII_RemoveItemFromSlot(NULL, aircraft->weapons, qfalse);
		AII_RemoveItemFromSlot(NULL, aircraft->electronics, qfalse);
	}
	AII_RemoveItemFromSlot(NULL, &aircraft->shield, qfalse);

	/** @todo This might be duplicate, see TR_NotifyAircraftRemoved() call
	 * on the top of this function */
	for (i = 0, transfer = gd.alltransfers; i < MAX_TRANSFERS; i++, transfer++) {
		if (!transfer->active)
			continue;
		if (!transfer->hasAircraft)
			continue;
		for (j = 0; j < MAX_AIRCRAFT; j++) {
			if (transfer->aircraftArray[j] > aircraft->idx)
				transfer->aircraftArray[j]--;
			else if (transfer->aircraftArray[j] == aircraft->idx)
				/** @todo This might not work as expected - maybe we
				 * have to memmove the array @sa E_DeleteEmployee */
				transfer->aircraftArray[j] = TRANS_LIST_EMPTY_SLOT;
		}
	}

	for (i = aircraft->idx + 1; i < gd.numAircraft; i++) {
		/* Decrease the global index of aircraft that have a higher index than the deleted one. */
		aircraft_t *aircraft_temp = AIR_AircraftGetFromIDX(i);
		if (aircraft_temp) {
			aircraft_temp->idx--;
		} else {
			/* For some reason there was no aircraft found for this index. */
			Com_DPrintf(DEBUG_CLIENT, "AIR_DeleteAircraft: No aircraft found for this global index: %i\n", i);
		}
	}

	gd.numAircraft--;	/**< Decrease the global number of aircraft. */

	/* Finally remove the aircraft-struct itself from the base-array and update the order. */
	/**
	 * @todo We need to update _all_ aircraft references here.
	 * Every single index that points to an aircraft after this one will need to be updated.
	 * attackingAircraft, aimedAircraft for airfights
	 * mission_t->ufo
	 * baseWeapon_t->target
	 */
	base->numAircraftInBase--;
	/* Update index of aircraftCurrent in base if it is affected by the index-change. */
	/* We have to check that we do NOT decrease the counter under the first Aircraft.... */
	if (base->aircraftCurrent >= aircraft && base->aircraftCurrent->homebase == aircraft->homebase && !(base->aircraftCurrent == &base->aircraft[0]))
		base->aircraftCurrent--;

	/* rearrange the aircraft-list (in base) */
	/* Find the index of aircraft in base */
	i = AIR_GetAircraftIdxInBase(aircraft);
	/* move other aircraft if the deleted aircraft was not the last one of the base */
	if (i != AIRCRAFT_INBASE_INVALID)
		memmove(aircraft, aircraft + 1, (base->numAircraftInBase - i) * sizeof(*aircraft));
	/* don't use the aircraft pointer after this memmove */
	aircraft = NULL;
	/* wipe the now vacant last slot */
	memset(&base->aircraft[base->numAircraftInBase], 0, sizeof(base->aircraft[base->numAircraftInBase]));

	if (base->numAircraftInBase < 1) {
		Cvar_SetValue("mn_equipsoldierstate", 0);
		Cvar_Set("mn_aircraftstatus", "");
		Cvar_Set("mn_aircraftinbase", "0");
		Cvar_Set("mn_aircraftname", "");
		Cvar_Set("mn_aircraft_model", "");
		base->aircraftCurrent = NULL;
	}

	/* also update the base menu buttons */
	B_BaseMenuInit(base);

	/* update hangar capacities */
	AIR_UpdateHangarCapForAll(base);

	/* Update Radar overlay */
	if (aircraftIsOnGeoscape)
		RADAR_UpdateWholeRadarOverlay();
}

/**
 * @brief Removes an aircraft from its base and the game.
 * @param[in] aircraft Pointer to aircraft that should be removed.
 * @note aircraft and assigned soldiers (if any) are removed from game.
 * @todo Return status of deletion for better error handling.
 */
void AIR_DestroyAircraft (aircraft_t *aircraft)
{
	int i;

	assert(aircraft);

	/* this must be a reverse loop because the employee array is changed for
	 * removing employees, thus the acTeam will point to another employee after
	 * E_DeleteEmployee (sideeffect) was called */
	for (i = aircraft->maxTeamSize - 1; i >= 0; i--) {
		if (aircraft->acTeam[i]) {
			employee_t *employee = aircraft->acTeam[i];
			E_DeleteEmployee(employee, employee->type);
			assert(aircraft->acTeam[i] == NULL);
		}
	}
	/* the craft may no longer have any employees assigned */
	assert(aircraft->teamSize == 0);
	/* remove the pilot */
	if (aircraft->pilot) {
		E_DeleteEmployee(aircraft->pilot, aircraft->pilot->type);
	} else {
		/* This shouldn't ever happen. */
		Com_DPrintf(DEBUG_CLIENT, "AIR_DestroyAircraft: aircraft id %s had no pilot\n", aircraft->id);
	}

	AIR_DeleteAircraft(aircraft->homebase, aircraft);
}

/**
 * @brief Moves given aircraft.
 * @param[in] dt time delta
 * @param[in] aircraft Pointer to aircraft on its way.
 * @return true if the aircraft reached its destination.
 */
qboolean AIR_AircraftMakeMove (int dt, aircraft_t* aircraft)
{
	float dist;

	/* calc distance */
	aircraft->time += dt;
	aircraft->fuel -= dt;

	dist = (float) aircraft->stats[AIR_STATS_SPEED] * aircraft->time / (float)SECONDS_PER_HOUR;

	/* Check if destination reached */
	if (dist >= aircraft->route.distance * (aircraft->route.numPoints - 1)) {
		return qtrue;
	} else {
		/* calc new position */
		float frac = dist / aircraft->route.distance;
		const int p = (int) frac;
		frac -= p;
		aircraft->point = p;
		aircraft->pos[0] = (1 - frac) * aircraft->route.point[p][0] + frac * aircraft->route.point[p + 1][0];
		aircraft->pos[1] = (1 - frac) * aircraft->route.point[p][1] + frac * aircraft->route.point[p + 1][1];

		MAP_CheckPositionBoundaries(aircraft->pos);
	}

	aircraft->hasMoved = qtrue;
	aircraft->numInterpolationPoints = 0;

	dist = (float) aircraft->stats[AIR_STATS_SPEED] * (aircraft->time + dt) / (float)SECONDS_PER_HOUR;

	/* Now calculate the projected position. This is the position that the aircraft should get on
	 * next frame if its route didn't change in meantime. */
	if (dist >= aircraft->route.distance * (aircraft->route.numPoints - 1)) {
		VectorSet(aircraft->projectedPos, 0.0f, 0.0f, 0.0f);
	} else {
		/* calc new position */
		float frac = dist / aircraft->route.distance;
		const int p = (int) frac;
		frac -= p;
		aircraft->projectedPos[0] = (1 - frac) * aircraft->route.point[p][0] + frac * aircraft->route.point[p + 1][0];
		aircraft->projectedPos[1] = (1 - frac) * aircraft->route.point[p][1] + frac * aircraft->route.point[p + 1][1];

		MAP_CheckPositionBoundaries(aircraft->projectedPos);
	}

	return qfalse;
}

/**
 * @brief Handles aircraft movement and actions in geoscape mode
 * @sa CL_CampaignRun
 * @param[in] dt time delta (may be 0 if radar overlay should be updated but no aircraft moves)
 * @param[in] updateRadarOverlay True if radar overlay should be updated (not needed if geoscape isn't updated)
 */
void CL_CampaignRunAircraft (int dt, qboolean updateRadarOverlay)
{
	aircraft_t *aircraft;
	int i, j, k;
	static qboolean radarOverlayReset = qfalse;	/**< true if at least one aircraft moved: radar overlay must be updated
												 * This is static because aircraft can move without radar beeing
												 * updated (sa CL_CampaignRun) */

	assert(dt >= 0);

	if (dt > 0) {
		for (j = 0; j < MAX_BASES; j++) {
			base_t *base = B_GetBaseByIDX(j);
			if (!base->founded) {
				if (base->numAircraftInBase) {
					/** @todo if a base was destroyed, but there are still
					 * aircraft on their way... */
				}
				continue;
			}

			/* Run each aircraft */
			for (i = 0, aircraft = base->aircraft; i < base->numAircraftInBase; i++, aircraft++)
				if (aircraft->homebase) {
					if (aircraft->status == AIR_IDLE) {
						/* Aircraft idle out of base */
						aircraft->fuel -= dt;
					} else if (AIR_IsAircraftOnGeoscape(aircraft)) {
						/* Aircraft is moving */
						if (AIR_AircraftMakeMove(dt, aircraft)) {
							/* aircraft reach its destination */
							float *end;

							end = aircraft->route.point[aircraft->route.numPoints - 1];
							Vector2Copy(end, aircraft->pos);
							MAP_CheckPositionBoundaries(aircraft->pos);

							switch (aircraft->status) {
							case AIR_MISSION:
								/* Aircraft reached its mission */
								assert(aircraft->mission);
								aircraft->mission->active = qtrue;
								aircraft->status = AIR_DROP;
								cls.missionaircraft = aircraft;
								MAP_SelectMission(cls.missionaircraft->mission);
								ccs.interceptAircraft = cls.missionaircraft;
								Com_DPrintf(DEBUG_CLIENT, "ccs.interceptAircraft: %i\n", ccs.interceptAircraft->idx);
								CL_GameTimeStop();
								MN_PushMenu("popup_intercept_ready", NULL);
								break;
							case AIR_RETURNING:
								/* aircraft entered in homebase */
								CL_AircraftReturnedToHomeBase(aircraft);
								aircraft->status = AIR_REFUEL;
								break;
							case AIR_TRANSFER:
								case AIR_UFO:
								break;
							default:
								aircraft->status = AIR_IDLE;
								break;
							}
						}
						/* radar overlay should be updated */
						radarOverlayReset = qtrue;
					} else if (aircraft->status == AIR_REFUEL) {
						/* Aircraft is refuelling at base */
						aircraft->fuel += dt * AIRCRAFT_REFUEL_FACTOR;
						if (aircraft->fuel >= aircraft->stats[AIR_STATS_FUELSIZE]) {
							aircraft->fuel = aircraft->stats[AIR_STATS_FUELSIZE];
							aircraft->status = AIR_HOME;
							assert(aircraft->homebase);
							MS_AddNewMessage(_("Notice"), va(_("Craft %s has refuelled at base %s."), _(aircraft->name), aircraft->homebase->name), qfalse, MSG_STANDARD, NULL);
						}
					}

					/* Check aircraft low fuel (only if aircraft is not already returning to base or in base) */
					if ((aircraft->status != AIR_RETURNING) && AIR_IsAircraftOnGeoscape(aircraft) &&
						!AIR_AircraftHasEnoughFuel(aircraft, aircraft->pos)) {
						/** @todo check if aircraft can go to a closer base with free space */
						MS_AddNewMessage(_("Notice"), va(_("Craft %s is low on fuel and must return to base."), _(aircraft->name)), qfalse, MSG_STANDARD, NULL);
						AIR_AircraftReturnToBase(aircraft);
					}

					/* Aircraft purchasing ufo */
					if (aircraft->status == AIR_UFO) {
						/* Solve the fight */
						AIRFIGHT_ExecuteActions(aircraft, aircraft->aircraftTarget);
					}

					/* Update delay to launch next projectile */
					if (AIR_IsAircraftOnGeoscape(aircraft)) {
						for (k = 0; k < aircraft->maxWeapons; k++) {
							if (aircraft->weapons[k].delayNextShot > 0)
								aircraft->weapons[k].delayNextShot -= dt;
						}
					}
				} else {
					/** @todo Maybe even Sys_Error? */
					Com_Printf("CL_CampaignRunAircraft: aircraft with no homebase (base: %i, aircraft '%s')\n", j, aircraft->id);
				}
		}
	}

	if (updateRadarOverlay && radarOverlayReset && (r_geoscape_overlay->integer & OVERLAY_RADAR)) {
		RADAR_UpdateWholeRadarOverlay();
		radarOverlayReset = qfalse;
	}
}

/**
 * @brief Returns the aircraft item in the list of aircraft Items.
 * @note id may not be null or empty
 * @param[in] id the item id in our csi.ods array
 */
objDef_t *AII_GetAircraftItemByID (const char *id)
{
	int i;

#ifdef DEBUG
	if (!id || !*id) {
		Com_Printf("AII_GetAircraftItemByID: Called with empty id\n");
		return NULL;
	}
#endif

	for (i = 0; i < csi.numODs; i++) {	/* i = item index */
		if (!Q_strncmp(id, csi.ods[i].id, MAX_VAR)) {
			if (csi.ods[i].craftitem.type < 0)
				Sys_Error("Same name for a none aircraft item object or not the correct filter-type for this object (%s)\n", id);
			return &csi.ods[i];
		}
	}

	Com_Printf("AII_GetAircraftItemByID: Aircraft Item \"%s\" not found.\n", id);
	return NULL;
}

/**
 * @brief Returns aircraft for a given global index.
 * @param[in] idx Global aircraft index.
 * @return An aircraft pointer (to a struct in a base) that has the given index or NULL if no aircraft found.
 */
aircraft_t* AIR_AircraftGetFromIDX (int idx)
{
	int baseIdx;
	aircraft_t* aircraft;

	if (idx == AIRCRAFT_INVALID || idx >= gd.numAircraft) {
		Com_DPrintf(DEBUG_CLIENT, "AIR_AircraftGetFromIDX: bad aircraft index: %i\n", idx);
		return NULL;
	}

#ifdef PARANOID
	if (gd.numBases < 1) {
		Com_DPrintf(DEBUG_CLIENT, "AIR_AircraftGetFromIDX: no base(s) found!\n");
	}
#endif

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		/* also get none founded bases for multiplayer here */
		base_t *base = B_GetBaseByIDX(baseIdx);
		if (!base)
			continue;
		for (aircraft = base->aircraft; aircraft < (base->aircraft + base->numAircraftInBase); aircraft++) {
			if (aircraft->idx == idx) {
				Com_DPrintf(DEBUG_CLIENT, "AIR_AircraftGetFromIDX: aircraft idx: %i - base idx: %i (%s)\n", aircraft->idx, base->idx, base->name);
				return aircraft;
			}
		}
	}

	return NULL;
}

/**
 * @brief Sends the specified aircraft to specified mission.
 * @param[in] aircraft Pointer to aircraft to send to mission.
 * @param[in] mission Pointer to given mission.
 * @return qtrue if sending an aircraft to specified mission is possible.
 */
qboolean AIR_SendAircraftToMission (aircraft_t *aircraft, mission_t *mission)
{
	if (!aircraft || !mission)
		return qfalse;

	if (!aircraft->teamSize) {
		MN_Popup(_("Notice"), _("Assign one or more soldiers to this aircraft first."));
		return qfalse;
	}

	/* if aircraft was in base */
	if (AIR_IsAircraftInBase(aircraft)) {
		/* reload its ammunition */
		AII_ReloadWeapon(aircraft);
	}

	/* ensure interceptAircraft is set correctly */
	ccs.interceptAircraft = aircraft;

	/* if mission is a base-attack and aircraft already in base, launch
	 * mission immediately */
	if (aircraft->homebase->baseStatus == BASE_UNDER_ATTACK &&
		AIR_IsAircraftInBase(aircraft)) {
		aircraft->mission = mission;
		mission->active = qtrue;
		MN_PushMenu("popup_baseattack", NULL);
		return qtrue;
	}

	if (!AIR_AircraftHasEnoughFuel(aircraft, mission->pos)) {
		MS_AddNewMessage(_("Notice"), _("Insufficient fuel."), qfalse, MSG_STANDARD, NULL);
		return qfalse;
	}

	MAP_MapCalcLine(aircraft->pos, mission->pos, &aircraft->route);
	aircraft->status = AIR_MISSION;
	aircraft->time = 0;
	aircraft->point = 0;
	aircraft->mission = mission;

	return qtrue;
}

/**
 * @brief Initialise all values of an aircraft slot.
 * @param[in] aircraftTemplate Pointer to the aircraft which needs initalisation of its slots.
 */
static void AII_InitialiseAircraftSlots (aircraft_t *aircraftTemplate)
{
	int i;

	/* initialise weapon slots */
	for (i = 0; i < MAX_AIRCRAFTSLOT; i++) {
		AII_InitialiseSlot(aircraftTemplate->weapons + i, aircraftTemplate, NULL, NULL, AC_ITEM_WEAPON);
		AII_InitialiseSlot(aircraftTemplate->electronics + i, aircraftTemplate, NULL, NULL, AC_ITEM_ELECTRONICS);
	}
	AII_InitialiseSlot(&aircraftTemplate->shield, aircraftTemplate, NULL, NULL, AC_ITEM_SHIELD);
}

/**
 * @brief List of valid strings for itemPos_t
 * @note must be in the same order than @c itemPos_t
 */
static const char *air_position_strings[AIR_POSITIONS_MAX] = {
	"nose_left",
	"nose_center",
	"nose_right",
	"wing_left",
	"wing_right",
	"rear_left",
	"rear_center",
	"rear_right"
};

/** @brief Valid aircraft parameter definitions from script files.
 * @note wrange can't be parsed in aircraft definition: this is a property
 * of weapon */
static const value_t aircraft_param_vals[] = {
	{"speed", V_INT, offsetof(aircraft_t, stats[AIR_STATS_SPEED]), MEMBER_SIZEOF(aircraft_t, stats[0])},
	{"maxspeed", V_INT, offsetof(aircraft_t, stats[AIR_STATS_MAXSPEED]), MEMBER_SIZEOF(aircraft_t, stats[0])},
	{"shield", V_INT, offsetof(aircraft_t, stats[AIR_STATS_SHIELD]), MEMBER_SIZEOF(aircraft_t, stats[0])},
	{"ecm", V_INT, offsetof(aircraft_t, stats[AIR_STATS_ECM]), MEMBER_SIZEOF(aircraft_t, stats[0])},
	{"damage", V_INT, offsetof(aircraft_t, stats[AIR_STATS_DAMAGE]), MEMBER_SIZEOF(aircraft_t, stats[0])},
	{"accuracy", V_INT, offsetof(aircraft_t, stats[AIR_STATS_ACCURACY]), MEMBER_SIZEOF(aircraft_t, stats[0])},

	{NULL, 0, 0, 0}
};

/** @brief Valid aircraft definition values from script files. */
static const value_t aircraft_vals[] = {
	{"name", V_TRANSLATION_STRING, offsetof(aircraft_t, name), 0},
	{"shortname", V_TRANSLATION_STRING, offsetof(aircraft_t, shortname), 0},
	{"numteam", V_INT, offsetof(aircraft_t, maxTeamSize), MEMBER_SIZEOF(aircraft_t, maxTeamSize)},
	{"size", V_INT, offsetof(aircraft_t, size), MEMBER_SIZEOF(aircraft_t, size)},
	{"nogeoscape", V_BOOL, offsetof(aircraft_t, notOnGeoscape), MEMBER_SIZEOF(aircraft_t, notOnGeoscape)},

	{"image", V_CLIENT_HUNK_STRING, offsetof(aircraft_t, image), 0},
	{"model", V_CLIENT_HUNK_STRING, offsetof(aircraft_t, model), 0},
	/* price for selling/buying */
	{"price", V_INT, offsetof(aircraft_t, price), MEMBER_SIZEOF(aircraft_t, price)},
	/* this is needed to let the buy and sell screen look for the needed building */
	/* to place the aircraft in */
	{"building", V_CLIENT_HUNK_STRING, offsetof(aircraft_t, building), 0},

	{NULL, 0, 0, 0}
};

/**
 * @brief Parses all aircraft that are defined in our UFO-scripts.
 * @sa CL_ParseClientData
 * @sa CL_ParseScriptSecond
 * @note parses the aircraft into our aircraft_sample array to use as reference
 * @note This parsing function writes into two different memory pools
 * one is the cl_localPool which is cleared on every new game, the other is
 * cl_genericPool which is existant until you close the game
 */
void AIR_ParseAircraft (const char *name, const char **text, qboolean assignAircraftItems)
{
	const char *errhead = "AIR_ParseAircraft: unexpected end of file (aircraft ";
	aircraft_t *aircraftTemplate;
	const value_t *vp;
	const char *token;
	int i;
	technology_t *tech;
	aircraftItemType_t itemType = MAX_ACITEMS;

	if (numAircraftTemplates >= MAX_AIRCRAFT) {
		Com_Printf("AIR_ParseAircraft: too many aircraft definitions; def \"%s\" ignored\n", name);
		return;
	}

	aircraftTemplate = NULL;
	if (!assignAircraftItems) {
		for (i = 0; i < numAircraftTemplates; i++) {
			if (!Q_strcmp(aircraftTemplates[i].id, name)) {
				Com_Printf("AIR_ParseAircraft: Second aircraft with same name found (%s) - second ignored\n", name);
				return;
			}
		}

		/* initialize the menu */
		aircraftTemplate = &aircraftTemplates[numAircraftTemplates];
		memset(aircraftTemplate, 0, sizeof(*aircraftTemplate));

		Com_DPrintf(DEBUG_CLIENT, "...found aircraft %s\n", name);
		/** @todo is this needed here? I think not, because the index of available aircraft
		 * are set when we create these aircraft from the samples - but i might be wrong here
		 * if i'm not wrong, the gd.numAircraft++ from a few lines below can go into trashbin, too */
		aircraftTemplate->idx = numAircraftTemplates;
		aircraftTemplate->tpl = aircraftTemplate;
		aircraftTemplate->id = Mem_PoolStrDup(name, cl_genericPool, CL_TAG_NONE);
		aircraftTemplate->status = AIR_HOME;
		/* default is no ufo */
		aircraftTemplate->ufotype = UFO_MAX;
		AII_InitialiseAircraftSlots(aircraftTemplate);
		/* Initialise UFO sensored */
		RADAR_InitialiseUFOs(&aircraftTemplate->radar);

		numAircraftTemplates++;
	} else {
		for (i = 0; i < numAircraftTemplates; i++) {
			if (!Q_strcmp(aircraftTemplates[i].id, name)) {
				aircraftTemplate = &aircraftTemplates[i];
				/* initialize slot numbers (useful when restarting a single campaign) */
				aircraftTemplate->maxWeapons = 0;
				aircraftTemplate->maxElectronics = 0;

				if (aircraftTemplate->type == AIRCRAFT_UFO)
					aircraftTemplate->ufotype = UFO_ShortNameToID(aircraftTemplate->id);

				break;
			}
		}
		if (i == numAircraftTemplates) {
			for (i = 0; i < numAircraftTemplates; i++) {
				Com_Printf("aircraft id: %s\n", aircraftTemplates[i].id);
			}
			Sys_Error("AIR_ParseAircraft: aircraft not found - can not link (%s) - parsed aircraft amount: %i\n", name, numAircraftTemplates);
		}
	}

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("AIR_ParseAircraft: aircraft def \"%s\" without body ignored\n", name);
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (assignAircraftItems) {
			assert(aircraftTemplate);
			/* write into cl_localPool - this data is reparsed on every new game */
			/* blocks like param { [..] } - otherwise we would leave the loop too early */
			if (*token == '{') {
				FS_SkipBlock(text);
			} else if (!Q_strncmp(token, "shield", 6)) {
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;
				Com_DPrintf(DEBUG_CLIENT, "use shield %s for aircraft %s\n", token, aircraftTemplate->id);
				tech = RS_GetTechByID(token);
				if (tech)
					aircraftTemplate->shield.item = AII_GetAircraftItemByID(tech->provides);
			} else if (!Q_strncmp(token, "slot", 4)) {
				token = COM_EParse(text, errhead, name);
				if (!*text || *token != '{') {
					Com_Printf("AIR_ParseAircraft: Invalid slot value for aircraft: %s\n", name);
					return;
				}
				do {
					token = COM_EParse(text, errhead, name);
					if (!*text)
						break;
					if (*token == '}')
						break;

					if (!Q_strcmp(token, "type")) {
						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						for (i = 0; i < MAX_ACITEMS; i++) {
							if (!Q_strcmp(token, air_slot_type_strings[i])) {
								itemType = i;
								switch (itemType) {
								case AC_ITEM_WEAPON:
									aircraftTemplate->maxWeapons++;
									break;
								case AC_ITEM_ELECTRONICS:
									aircraftTemplate->maxElectronics++;
									break;
								default:
									itemType = MAX_ACITEMS;
								}
								break;
							}
						}
						if (i == MAX_ACITEMS)
							Sys_Error("Unknown value '%s' for slot type\n", token);
					} else if (!Q_strcmp(token, "position")) {
						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						for (i = 0; i < AIR_POSITIONS_MAX; i++) {
							if (!Q_strcmp(token, air_position_strings[i])) {
								switch (itemType) {
								case AC_ITEM_WEAPON:
									aircraftTemplate->weapons[aircraftTemplate->maxWeapons - 1].pos = i;
									break;
								case AC_ITEM_ELECTRONICS:
									aircraftTemplate->electronics[aircraftTemplate->maxElectronics - 1].pos = i;
									break;
								default:
									i = AIR_POSITIONS_MAX;
								}
								break;
							}
						}
						if (i == AIR_POSITIONS_MAX)
							Sys_Error("Unknown value '%s' for slot position\n", token);
					} else if (!Q_strcmp(token, "contains")) {
						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						tech = RS_GetTechByID(token);
						if (tech) {
							switch (itemType) {
							case AC_ITEM_WEAPON:
								aircraftTemplate->weapons[aircraftTemplate->maxWeapons - 1].item = AII_GetAircraftItemByID(tech->provides);
								Com_DPrintf(DEBUG_CLIENT, "use weapon %s for aircraft %s\n", token, aircraftTemplate->id);
								break;
							case AC_ITEM_ELECTRONICS:
								aircraftTemplate->electronics[aircraftTemplate->maxElectronics - 1].item = AII_GetAircraftItemByID(tech->provides);
								Com_DPrintf(DEBUG_CLIENT, "use electronics %s for aircraft %s\n", token, aircraftTemplate->id);
								break;
							default:
								Com_Printf("Ignoring item value '%s' due to unknown slot type\n", token);
							}
						}
					} else if (!Q_strcmp(token, "ammo")) {
						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						tech = RS_GetTechByID(token);
						if (tech) {
							if (itemType == AC_ITEM_WEAPON) {
								aircraftTemplate->weapons[aircraftTemplate->maxWeapons - 1].ammo = AII_GetAircraftItemByID(tech->provides);
								Com_DPrintf(DEBUG_CLIENT, "use ammo %s for aircraft %s\n", token, aircraftTemplate->id);
							} else
								Com_Printf("Ignoring ammo value '%s' due to unknown slot type\n", token);
						}
					} else if (!Q_strncmp(token, "size", MAX_VAR)) {
						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						if (itemType == AC_ITEM_WEAPON) {
							if (!Q_strncmp(token, "light", MAX_VAR))
								aircraftTemplate->weapons[aircraftTemplate->maxWeapons - 1].size = ITEM_LIGHT;
							else if (!Q_strncmp(token, "medium", MAX_VAR))
								aircraftTemplate->weapons[aircraftTemplate->maxWeapons - 1].size = ITEM_MEDIUM;
							else if (!Q_strncmp(token, "heavy", MAX_VAR))
								aircraftTemplate->weapons[aircraftTemplate->maxWeapons - 1].size = ITEM_HEAVY;
							else
								Com_Printf("Unknown size value for aircraft slot: '%s'\n", token);
						} else
							Com_Printf("Ignoring size parameter '%s' for non-weapon aircraft slots\n", token);
					} else
						Com_Printf("AIR_ParseAircraft: Ignoring unknown slot value '%s'\n", token);
				} while (*text); /* dummy condition */
			}
		} else {
			if (!Q_strcmp(token, "shield")) {
				COM_EParse(text, errhead, name);
				continue;
			}
			/* check for some standard values */
			for (vp = aircraft_vals; vp->string; vp++)
				if (!Q_strcmp(token, vp->string)) {
					/* found a definition */
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;
					switch (vp->type) {
					case V_TRANSLATION_STRING:
						token++;
					case V_CLIENT_HUNK_STRING:
						Mem_PoolStrDupTo(token, (char**) ((char*)aircraftTemplate + (int)vp->ofs), cl_genericPool, CL_TAG_NONE);
						break;
					default:
						Com_EParseValue(aircraftTemplate, token, vp->type, vp->ofs, vp->size);
					}

					break;
				}

			if (vp->string && !Q_strncmp(vp->string, "size", 4)) {
				if (aircraftTemplate->maxTeamSize > MAX_ACTIVETEAM) {
					Com_DPrintf(DEBUG_CLIENT, "AIR_ParseAircraft: Set size for aircraft to the max value of %i\n", MAX_ACTIVETEAM);
					aircraftTemplate->maxTeamSize = MAX_ACTIVETEAM;
				}
			}

			if (!Q_strncmp(token, "type", 4)) {
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;
				if (!Q_strncmp(token, "transporter", 11))
					aircraftTemplate->type = AIRCRAFT_TRANSPORTER;
				else if (!Q_strncmp(token, "interceptor", 11))
					aircraftTemplate->type = AIRCRAFT_INTERCEPTOR;
				else if (!Q_strncmp(token, "ufo", 3))
					aircraftTemplate->type = AIRCRAFT_UFO;
			} else if (!Q_strncmp(token, "slot", 5)) {
				token = COM_EParse(text, errhead, name);
				if (!*text || *token != '{') {
					Com_Printf("AIR_ParseAircraft: Invalid slot value for aircraft: %s\n", name);
					return;
				}
				FS_SkipBlock(text);
			} else if (!Q_strncmp(token, "param", 5)) {
				token = COM_EParse(text, errhead, name);
				if (!*text || *token != '{') {
					Com_Printf("AIR_ParseAircraft: Invalid param value for aircraft: %s\n", name);
					return;
				}
				do {
					token = COM_EParse(text, errhead, name);
					if (!*text)
						break;
					if (*token == '}')
						break;

					if (!Q_strncmp(token, "range", 5)) {
						/* this is the range of aircraft, must be translated into fuel */
						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						Com_EParseValue(aircraftTemplate, token, V_INT, offsetof(aircraft_t, stats[AIR_STATS_FUELSIZE]), MEMBER_SIZEOF(aircraft_t, stats[0]));
						if (aircraftTemplate->stats[AIR_STATS_SPEED] == 0)
							Sys_Error("AIR_ParseAircraft: speed value must be entered before range value");
						aircraftTemplate->stats[AIR_STATS_FUELSIZE] = (int) (2.0f * (float)SECONDS_PER_HOUR * aircraftTemplate->stats[AIR_STATS_FUELSIZE]) /
							((float) aircraftTemplate->stats[AIR_STATS_SPEED]);
					} else {
						for (vp = aircraft_param_vals; vp->string; vp++)
							if (!Q_strcmp(token, vp->string)) {
								/* found a definition */
								token = COM_EParse(text, errhead, name);
								if (!*text)
									return;
								switch (vp->type) {
								case V_TRANSLATION_STRING:
									token++;
								case V_CLIENT_HUNK_STRING:
									Mem_PoolStrDupTo(token, (char**) ((char*)aircraftTemplate + (int)vp->ofs), cl_genericPool, CL_TAG_NONE);
									break;
								default:
									Com_EParseValue(aircraftTemplate, token, vp->type, vp->ofs, vp->size);
								}
								break;
							}
					}
					if (!vp->string)
						Com_Printf("AIR_ParseAircraft: Ignoring unknown param value '%s'\n", token);
				} while (*text); /* dummy condition */
			} else if (!vp->string) {
				Com_Printf("AIR_ParseAircraft: unknown token \"%s\" ignored (aircraft %s)\n", token, name);
				COM_EParse(text, errhead, name);
			}
		} /* assignAircraftItems */
	} while (*text);
}

#ifdef DEBUG
/**
 * @brief Debug function that prints aircraft to game console
 */
void AIR_ListAircraftSamples_f (void)
{
	int i = 0, max = numAircraftTemplates;
	const value_t *vp;
	aircraft_t *aircraftTemplate;

	Com_Printf("%i aircraft\n", max);
	if (Cmd_Argc() == 2) {
		max = atoi(Cmd_Argv(1));
		if (max >= numAircraftTemplates || max < 0)
			return;
		i = max - 1;
	}
	for (; i < max; i++) {
		aircraftTemplate = &aircraftTemplates[i];
		Com_Printf("aircraft: '%s'\n", aircraftTemplate->id);
		for (vp = aircraft_vals; vp->string; vp++) {
			Com_Printf("..%s: %s\n", vp->string, Com_ValueToStr(aircraftTemplate, vp->type, vp->ofs));
		}
		for (vp = aircraft_param_vals; vp->string; vp++) {
			Com_Printf("..%s: %s\n", vp->string, Com_ValueToStr(aircraftTemplate, vp->type, vp->ofs));
		}
	}
}
#endif

/**
 * @brief Reload the weapon of an aircraft
 * @param[in] aircraft Pointer to the aircraft to reload
 * @todo check if there is still ammo in storage, and remove them from it
 * @todo this should costs credits
 * @sa AIRFIGHT_AddProjectile for the basedefence reload code
 */
void AII_ReloadWeapon (aircraft_t *aircraft)
{
	int i;

	assert(aircraft);

	/* Reload all ammos of aircraft */
	for (i = 0; i < aircraft->maxWeapons; i++) {
		if (aircraft->ufotype != UFO_MAX) {
			aircraft->weapons[i].ammoLeft = AMMO_STATUS_UNLIMITED;
		} else if (aircraft->weapons[i].ammo && !aircraft->weapons[i].ammo->craftitem.unlimitedAmmo) {
			aircraft->weapons[i].ammoLeft = aircraft->weapons[i].ammo->ammo;
		}
	}
}

/*===============================================
Aircraft functions related to UFOs or missions.
===============================================*/

/**
 * @brief Notify that a mission has been removed.
 * @param[in] mission Pointer to the mission that has been removed.
 */
void AIR_AircraftsNotifyMissionRemoved (const mission_t *const mission)
{
	int baseIdx;
	aircraft_t* aircraft;

	/* Aircraft currently moving to the mission will be redirect to base */
	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;

		for (aircraft = base->aircraft + base->numAircraftInBase - 1;
			aircraft >= base->aircraft; aircraft--) {

			if (aircraft->status == AIR_MISSION && aircraft->mission == mission)
				AIR_AircraftReturnToBase(aircraft);
		}
	}
}

/**
 * @brief Notify that a UFO has been removed.
 * @param[in] ufo Pointer to UFO that has been removed.
 * @param[in] destroyed True if the UFO has been destroyed, false if it only landed.
 */
void AIR_AircraftsNotifyUFORemoved (const aircraft_t *const ufo, qboolean destroyed)
{
	int baseIdx;
	aircraft_t* aircraft;
	int i;

	assert(ufo);

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;

		/* Base currently targeting the specified ufo loose their target */
		for (i = 0; i < base->numBatteries; i++) {
			if (base->batteries[i].target == ufo)
				base->batteries[i].target = NULL;
			else if (destroyed && (base->batteries[i].target > ufo))
				base->batteries[i].target--;
		}
		for (i = 0; i < base->numLasers; i++) {
			if (base->lasers[i].target == ufo)
				base->lasers[i].target = NULL;
			else if (destroyed && (base->lasers[i].target > ufo))
				base->lasers[i].target--;
		}
		/* Aircraft currently purchasing the specified ufo will be redirect to base */
		for (aircraft = base->aircraft;
			aircraft < base->aircraft + base->numAircraftInBase; aircraft++)
			if (aircraft->status == AIR_UFO) {
				if (ufo == aircraft->aircraftTarget)
					AIR_AircraftReturnToBase(aircraft);
				else if (destroyed && (ufo < aircraft->aircraftTarget)) {
					aircraft->aircraftTarget--;
				}
			}
	}
}

/**
 * @brief Notify that a UFO disappear from radars.
 * @param[in] ufo Pointer to a UFO that has disappeared.
 */
void AIR_AircraftsUFODisappear (const aircraft_t *const ufo)
{
	int baseIdx;
	aircraft_t* aircraft;

	/* Aircraft currently pursuing the specified UFO will be redirected to base */
	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetBaseByIDX(baseIdx);

		for (aircraft = base->aircraft + base->numAircraftInBase - 1;
			aircraft >= base->aircraft; aircraft--)
			if (aircraft->status == AIR_UFO)
				if (ufo == aircraft->aircraftTarget)
					AIR_AircraftReturnToBase(aircraft);
	}
}

/**
 * @brief Calculates the point where aircraft should go to intecept a moving target.
 * @param[in] shooter Pointer to shooting aircraft.
 * @param[in] target Pointer to target aircraft.
 * @param[out] dest Destination that shooting aircraft should aim to intercept target aircraft.
 * @todo This function is not perfect, because I (Kracken) made the calculations in a plane.
 * We should use here geometry on a sphere, and only compute this calculation every time target
 * change destination, or one of the aircraft speed changes. The calculation here gives good results
 * when both aircraft are quite close (most of the time, it's true).
 * @sa AIR_SendAircraftPursuingUFO
 * @sa UFO_SendPursuingAircraft
 */
void AIR_GetDestinationWhilePursuing (const aircraft_t const *shooter, const aircraft_t const *target, vec2_t *dest)
{
	vec3_t shooterPos, targetPos, targetDestPos, shooterDestPos, rotationAxis;
	float dist;
	float angle;

	dist = MAP_GetDistance(shooter->pos, target->pos);
	/* below calculation gives bad results when aircraft are far away: just go to target location at first
	 * (this is a hack that should be removed when sphere calculation is made)
	 * we also skip the calculation if aircraft are really close one from each other (eg. in airfight map)
	 * otherwise the dotproduct may be greater than 1 due to rounding errors */
	if (dist > 50.0f || dist < .02f) {
		Vector2Copy(target->pos, (*dest));
		return;
	}

	/* Convert aircraft position into cartesian frame */
	PolarToVec(shooter->pos, shooterPos);
	PolarToVec(target->pos, targetPos);
	PolarToVec(target->route.point[target->route.numPoints - 1], targetDestPos);

	/* In the following, we note S the position of the shooter, T the position of the target,
	 * D the destination of the target and I the interception point where shooter should reach target */

	/* Calculate angle between ST and TD (in radian) */
	angle = acos(DotProduct(shooterPos, targetPos));

	/* Calculate the distance target will be able to fly before shooter reaches it */
	dist /= cos(angle) + sqrt(pow(shooter->stats[AIR_STATS_SPEED], 2) / pow(target->stats[AIR_STATS_SPEED], 2) - pow(sin(angle), 2));

	/* Get rotation vector */
	CrossProduct(targetPos, targetDestPos, rotationAxis);
	VectorNormalize(rotationAxis);

	/* Rotate target position of dist to find destination point */
	RotatePointAroundVector(shooterDestPos, rotationAxis, targetPos, dist);
	VecToPolar(shooterDestPos, *dest);

	/* make sure we don't get a NAN value */
	assert((*dest)[0] <= 180.0f && (*dest)[0] >= -180.0f && (*dest)[1] <= 90.0f && (*dest)[1] >= -90.0f);
}

/**
 * @brief Make the specified aircraft purchasing a UFO.
 * @param[in] aircraft Pointer to an aircraft which will hunt for a UFO.
 * @param[in] ufo Pointer to a UFO.
 */
qboolean AIR_SendAircraftPursuingUFO (aircraft_t* aircraft, aircraft_t* ufo)
{
	const int num = ufo - gd.ufos;
	vec2_t dest;

	if (num < 0 || num >= gd.numUFOs || ! aircraft || ! ufo)
		return qfalse;

	/* if aircraft was in base */
	if (AIR_IsAircraftInBase(aircraft)) {
		/* reload its ammunition */
		AII_ReloadWeapon(aircraft);
	}

	/* don't check if the aircraft has enough fuel: maybe UFO will come closer */

	AIR_GetDestinationWhilePursuing(aircraft, ufo, &dest);
	MAP_MapCalcLine(aircraft->pos, dest, &aircraft->route);
	aircraft->status = AIR_UFO;
	aircraft->time = 0;
	aircraft->point = 0;
	aircraft->aircraftTarget = ufo;
	aircraft->baseTarget = NULL;
	return qtrue;
}

/*============================================
Aircraft functions related to team handling.
============================================*/

/**
 * @brief Resets team in given aircraft.
 * @param[in] aircraft Pointer to an aircraft, where the team will be reset.
 * @todo Use memset here.
 */
void AIR_ResetAircraftTeam (aircraft_t *aircraft)
{
	int i;

	for (i = 0; i < MAX_ACTIVETEAM; i++) {
		aircraft->acTeam[i] = NULL;
	}
}

/**
 * @brief Adds given employee to given aircraft.
 * @param[in] aircraft Pointer to an aircraft, to which we will add employee.
 * @param[in] employee The employee to add to the aircraft.
 * @note this is responsible for adding soldiers to a team in dropship
 * @sa baseAttackFakeAircraft
 */
qboolean AIR_AddToAircraftTeam (aircraft_t *aircraft, employee_t* employee)
{
	int i;

	if (!employee) {
		Com_DPrintf(DEBUG_CLIENT, "AIR_AddToAircraftTeam: No employee given!\n");
		return qfalse;
	}

	if (!aircraft) {
		Com_DPrintf(DEBUG_CLIENT, "AIR_AddToAircraftTeam: No aircraft given!\n");
		return qfalse;
	}
	if (aircraft->teamSize < aircraft->maxTeamSize) {
		/* Search for unused place in aircraft and fill it with employee-data */
		for (i = 0; i < aircraft->maxTeamSize; i++)
			if (!aircraft->acTeam[i]) {
				aircraft->acTeam[i] = employee;
				Com_DPrintf(DEBUG_CLIENT, "AIR_AddToAircraftTeam: added idx '%d'\n",
					employee->idx);
				aircraft->teamSize++;
				return qtrue;
			}
		Sys_Error("AIR_AddToAircraftTeam: Couldn't find space");
	}

	Com_DPrintf(DEBUG_CLIENT, "AIR_AddToAircraftTeam: No space in aircraft\n");
	return qfalse;
}

/**
 * @brief Removes given employee from given aircraft team.
 * @param[in] aircraft The aircraft to remove the employee from.
 * @param[in] employee The employee to remove from the team.
 * @note This is responsible for removing soldiers from a team in a dropship.
 */
qboolean AIR_RemoveFromAircraftTeam (aircraft_t *aircraft, const employee_t *employee)
{
	int i;

	assert(aircraft);

	if (aircraft->teamSize <= 0) {
		Com_Printf("AIR_RemoveFromAircraftTeam: teamSize is %i, we should not be here!\n",
			aircraft->teamSize);
		return qfalse;
	}

	for (i = 0; i < aircraft->maxTeamSize; i++) {
		/* Search for this exact employee in the aircraft and remove him from the team. */
		if (aircraft->acTeam[i] && aircraft->acTeam[i] == employee)	{
			aircraft->acTeam[i] = NULL;
			Com_DPrintf(DEBUG_CLIENT, "AIR_RemoveFromAircraftTeam: removed idx '%d' \n",
				employee->idx);
			aircraft->teamSize--;
			return qtrue;
		}
	}
	/* there must be a homebase when there are employees - otherwise this
	 * functions should not be called */
	assert(aircraft->homebase);
	Com_Printf("AIR_RemoveFromAircraftTeam: error: idx '%d' (type: %i) not on aircraft %i (base: %i) in base %i\n",
		employee->idx, employee->type, aircraft->idx, AIR_GetAircraftIdxInBase(aircraft), aircraft->homebase->idx);
	return qfalse;
}

/**
 * @brief Checks whether given employee is in given aircraft (i.e. he/she is onboard).
 * @param[in] aircraft The team-list in theis aircraft is check if it contains the employee.
 * @param[in] employee Employee to check.
 * @return qtrue if an employee with given index is assigned to given aircraft.
 */
qboolean AIR_IsInAircraftTeam (const aircraft_t *aircraft, const employee_t *employee)
{
	int i;

	if (!aircraft) {
		Com_DPrintf(DEBUG_CLIENT, "AIR_IsInAircraftTeam: No aircraft given\n");
		return qfalse;
	}

	if (!employee) {
		Com_Printf("AIR_IsInAircraftTeam: No employee given.\n");
		return qfalse;
	}

	for (i = 0; i < aircraft->maxTeamSize; i++) {
		if (aircraft->acTeam[i] == employee) {
			/** @note This also skips the NULL entries in acTeam[]. */
			return qtrue;
		}
	}

	Com_DPrintf(DEBUG_CLIENT, "AIR_IsInAircraftTeam: not found idx '%d' \n", employee->idx);
	return qfalse;
}

/**
 * @brief Adds the pilot to the first available aircraft at the specified base.
 * @param[in] base Which base has aircraft to add the pilot to.
 * @param[in] type Which pilot to search add.
 */
void AIR_AutoAddPilotToAircraft (base_t* base, employee_t* pilot)
{
	int i;

	for (i = 0; i < base->numAircraftInBase; i++) {
		aircraft_t *aircraft = &base->aircraft[i];
		if (!aircraft->pilot) {
			aircraft->pilot = pilot;
			break;
		}
	}

}

/**
 * @brief Checks to see if the pilot is in any aircraft at this base.
 * If he is then he is removed from that aircraft.
 * @param[in] base Which base has the aircraft to search for the employee in.
 * @param[in] type Which employee to search for.
 */
void AIR_RemovePilotFromAssignedAircraft (base_t* base, const employee_t* pilot)
{
	int i;

	for (i = 0; i < base->numAircraftInBase; i++) {
		aircraft_t *aircraft = &base->aircraft[i];
		if (aircraft->pilot == pilot) {
			aircraft->pilot = NULL;
			break;
		}
	}

}

/**
 * @brief Get the all the unique weapon ranges of this aircraft.
 * @param[in] slot Pointer to the aircrafts weapon slot list.
 * @param[in] maxSlot maximum number of weapon slots in aircraft.
 * @param[out] weaponRanges An array containing a unique list of weapons ranges.
 * @return Number of unique weapons ranges.
 */
int AIR_GetAircraftWeaponRanges (const aircraftSlot_t *slot, int maxSlot, float *weaponRanges)
{
	int idxSlot;
	int idxAllWeap;
	float allWeaponRanges[MAX_AIRCRAFTSLOT];
	int numAllWeaponRanges = 0;
	int numUniqueWeaponRanges = 0;

	assert(slot);

	/* We choose the usable weapon to add to the weapons array */
	for (idxSlot = 0; idxSlot < maxSlot; idxSlot++) {
		const aircraftSlot_t *weapon = slot + idxSlot;
		const objDef_t *ammo = weapon->ammo;

		if (!ammo)
			continue;

		allWeaponRanges[numAllWeaponRanges] = ammo->craftitem.stats[AIR_STATS_WRANGE];
		numAllWeaponRanges++;
	}

	if (numAllWeaponRanges > 0) {
		/* sort the list of all weapon ranges and create an array with only the unique ranges */
		qsort(allWeaponRanges, numAllWeaponRanges, sizeof(allWeaponRanges[0]), Q_FloatSort);

		for (idxAllWeap = 0; idxAllWeap < numAllWeaponRanges; idxAllWeap++) {
			if (allWeaponRanges[idxAllWeap] != weaponRanges[numUniqueWeaponRanges - 1] || idxAllWeap == 0) {
				weaponRanges[numUniqueWeaponRanges] = allWeaponRanges[idxAllWeap];
				numUniqueWeaponRanges++;
			}
		}
	}

	return numUniqueWeaponRanges;
}

/**
 * @brief Save callback for savegames
 * @sa AIR_Load
 * @sa B_Save
 * @sa SAV_GameSave
 */
qboolean AIR_Save (sizebuf_t* sb, void* data)
{
	int i, j;

	/* save the ufos on geoscape */
	for (i = 0; i < presaveArray[PRE_NUMUFO]; i++) {
		MSG_WriteString(sb, gd.ufos[i].id);
		MSG_WriteByte(sb, gd.ufos[i].detected);	/* must be saved because detection is random */
		MSG_WriteByte(sb, gd.ufos[i].landed);
		MSG_WritePos(sb, gd.ufos[i].pos);
		MSG_WriteByte(sb, gd.ufos[i].status);
		MSG_WriteLong(sb, gd.ufos[i].fuel);
		MSG_WriteLong(sb, gd.ufos[i].damage);
		MSG_WriteLong(sb, gd.ufos[i].time);
		MSG_WriteShort(sb, gd.ufos[i].point);
		MSG_WriteShort(sb, gd.ufos[i].route.numPoints);
		MSG_WriteFloat(sb, gd.ufos[i].route.distance);
		for (j = 0; j < gd.ufos[i].route.numPoints; j++)
			MSG_Write2Pos(sb, gd.ufos[i].route.point[j]);
		MSG_WritePos(sb, gd.ufos[i].direction);
#ifdef DEBUG
		if (!gd.ufos[i].mission)
			Com_Printf("Error: UFO '%s' (#%i) is not linked to any mission\n", gd.ufos[i].id, i);
#endif
		MSG_WriteString(sb, gd.ufos[i].mission->id);
		for (j = 0; j < presaveArray[PRE_AIRSTA]; j++) {
#ifdef DEBUG
			/* UFO HP can be < 0 if the UFO has been destroyed */
			if (j != AIR_STATS_DAMAGE && gd.ufos[i].stats[j] < 0)
				Com_Printf("Warning: ufo '%s' stats %i: %i is smaller than 0\n", gd.ufos[i].id, j, gd.ufos[i].stats[j]);
#endif
			MSG_WriteLong(sb, gd.ufos[i].stats[j]);
		}
		/* Save target of the ufo */
		if (gd.ufos[i].baseTarget)
			MSG_WriteShort(sb, gd.ufos[i].baseTarget->idx);
		else
			MSG_WriteShort(sb, -1);
		if (gd.ufos[i].aircraftTarget)
			MSG_WriteShort(sb, gd.ufos[i].aircraftTarget->idx);
		else
			MSG_WriteShort(sb, -1);

		/* save weapon slots */
		MSG_WriteByte(sb, gd.ufos[i].maxWeapons);
		for (j = 0; j < gd.ufos[i].maxWeapons; j++) {
			if (gd.ufos[i].weapons[j].item) {
				MSG_WriteString(sb, gd.ufos[i].weapons[j].item->id);
				MSG_WriteShort(sb, gd.ufos[i].weapons[j].ammoLeft);
				MSG_WriteShort(sb, gd.ufos[i].weapons[j].delayNextShot);
				MSG_WriteShort(sb, gd.ufos[i].weapons[j].installationTime);
				/* if there is no ammo MSG_WriteString will write an empty string */
				MSG_WriteString(sb, gd.ufos[i].weapons[j].ammo->id);
			} else {
				MSG_WriteString(sb, "");
				MSG_WriteShort(sb, 0);
				MSG_WriteShort(sb, 0);
				MSG_WriteShort(sb, 0);
				/* if there is no ammo MSG_WriteString will write an empty string */
				MSG_WriteString(sb, "");
			}
		}
		/* save shield slots - currently only one */
		MSG_WriteByte(sb, 1);
		if (gd.ufos[i].shield.item) {
			MSG_WriteString(sb, gd.ufos[i].shield.item->id);
			MSG_WriteShort(sb, gd.ufos[i].shield.installationTime);
		} else {
			MSG_WriteString(sb, "");
			MSG_WriteShort(sb, 0);
		}
		/* save electronics slots */
		MSG_WriteByte(sb, gd.ufos[i].maxElectronics);
		for (j = 0; j < gd.ufos[i].maxElectronics; j++) {
			if (gd.ufos[i].electronics[j].item) {
				MSG_WriteString(sb, gd.ufos[i].electronics[j].item->id);
				MSG_WriteShort(sb, gd.ufos[i].electronics[j].installationTime);
			} else {
				MSG_WriteString(sb, "");
				MSG_WriteShort(sb, 0);
			}
		}
		/** @todo more? */
	}

	/* Save projectiles. */
	MSG_WriteByte(sb, gd.numProjectiles);
	for (i = 0; i < gd.numProjectiles; i++) {
		MSG_WriteString(sb, gd.projectiles[i].aircraftItem->id);
		for (j = 0; j < presaveArray[PRE_MAXMPR]; j++)
			MSG_Write2Pos(sb, gd.projectiles[i].pos[j]);
		MSG_WritePos(sb, gd.projectiles[i].idleTarget);
		if (gd.projectiles[i].attackingAircraft) {
			MSG_WriteByte(sb, gd.projectiles[i].attackingAircraft->type == AIRCRAFT_UFO);
			if (gd.projectiles[i].attackingAircraft->type == AIRCRAFT_UFO)
				MSG_WriteShort(sb, gd.projectiles[i].attackingAircraft - gd.ufos);
			else
				MSG_WriteShort(sb, gd.projectiles[i].attackingAircraft->idx);
		} else
			MSG_WriteByte(sb, 2);
		if (gd.projectiles[i].aimedBase)
			MSG_WriteShort(sb, gd.projectiles[i].aimedBase->idx);
		else
			MSG_WriteShort(sb, -1);
		if (gd.projectiles[i].aimedAircraft) {
			MSG_WriteByte(sb, gd.projectiles[i].aimedAircraft->type == AIRCRAFT_UFO);
			if (gd.projectiles[i].aimedAircraft->type == AIRCRAFT_UFO)
				MSG_WriteShort(sb, gd.projectiles[i].aimedAircraft - gd.ufos);
			else
				MSG_WriteShort(sb, gd.projectiles[i].aimedAircraft->idx);
		} else
			MSG_WriteByte(sb, 2);
		MSG_WriteLong(sb, gd.projectiles[i].time);
		MSG_WriteFloat(sb, gd.projectiles[i].angle);
		MSG_WriteByte(sb, gd.projectiles[i].bullets);
		MSG_WriteByte(sb, gd.projectiles[i].laser);
	}

	/* Save recoveries. */
	for (i = 0; i < presaveArray[PRE_MAXREC]; i++) {
		MSG_WriteByte(sb, gd.recoveries[i].active);
		MSG_WriteByte(sb, gd.recoveries[i].base ? gd.recoveries[i].base->idx : BYTES_NONE);
		/** @todo At some point we really need to save a unique string here. */
		MSG_WriteByte(sb, gd.recoveries[i].ufoTemplate ? gd.recoveries[i].ufoTemplate->idx : BYTES_NONE);
		MSG_WriteLong(sb, gd.recoveries[i].event.day);
		MSG_WriteLong(sb, gd.recoveries[i].event.sec);
	}
	return qtrue;
}

/**
 * @brief Load callback for savegames
 * @note employees and bases must have been loaded already
 * @sa AIR_Save
 * @sa B_Load
 * @sa SAV_GameLoad
 */
qboolean AIR_Load (sizebuf_t* sb, void* data)
{
	aircraft_t *ufo;
	int i, j;
	int numUFOs = 0;
	const char *s;
	/* vars, if aircraft wasn't found */
	vec3_t tmp_vec3t;
	vec2_t tmp_vec2t;
	int tmp_int;
	technology_t *tech;

	/* load the amount of ufos on geoscape */
	gd.numUFOs = presaveArray[PRE_NUMUFO];
	/* load the ufos on geoscape */
	for (i = 0; i < presaveArray[PRE_NUMUFO]; i++) {
		s = MSG_ReadString(sb);
		ufo = AIR_GetAircraft(s);
		/* maybe aircraft id in .ufo file changed? */
		if (!ufo) {
			Com_Printf("AIR_Load: Could not find ufo '%s'\n", s);
			/* Remove the UFO that couldn't be loaded */
			gd.numUFOs--;

			MSG_ReadByte(sb);			/* detected */
			MSG_ReadByte(sb);			/* landed */
			MSG_ReadPos(sb, tmp_vec3t);	/* pos */
			MSG_ReadByte(sb);			/* status */
			MSG_ReadLong(sb);			/* fuel */
			MSG_ReadLong(sb);			/* damage */
			MSG_ReadLong(sb);			/* time */
			MSG_ReadShort(sb);			/* point */
			tmp_int = MSG_ReadShort(sb);/* numPoints */
			if (ufo->route.numPoints > LINE_MAXPTS) {
				return qfalse;
			}
			MSG_ReadFloat(sb);			/* distance */
			for (j = 0; j < tmp_int; j++)
				MSG_Read2Pos(sb, tmp_vec2t);	/* route points */
			MSG_ReadPos(sb, tmp_vec3t);	/* direction */
			MSG_ReadString(sb);			/* Mission id */
			for (j = 0; j < presaveArray[PRE_AIRSTA]; j++)
				MSG_ReadLong(sb);
			MSG_ReadShort(sb);			/* baseTarget index */
			MSG_ReadShort(sb);			/* aircraftTarget index */
			/* read slots */
			tmp_int = MSG_ReadByte(sb);
			for (j = 0; j < tmp_int; j++) {
				MSG_ReadString(sb);
				MSG_ReadShort(sb);
				MSG_ReadShort(sb);
				MSG_ReadShort(sb);
				MSG_ReadString(sb);
			}
			/* shield */
			tmp_int = MSG_ReadByte(sb);
			if (tmp_int) {
				MSG_ReadString(sb);
				MSG_ReadShort(sb);
			}
			/* electro */
			tmp_int = MSG_ReadByte(sb);
			if (tmp_int) {
				MSG_ReadString(sb);
				MSG_ReadShort(sb);
			}
		} else {
			gd.ufos[i] = *ufo;
			ufo = &gd.ufos[i];					/* Copy all datas that don't need to be saved (tpl, hangar,...) */
			ufo->detected = MSG_ReadByte(sb);
			ufo->landed = MSG_ReadByte(sb);
			MSG_ReadPos(sb, ufo->pos);
			ufo->status = MSG_ReadByte(sb);
			ufo->fuel = MSG_ReadLong(sb);
			ufo->damage = MSG_ReadLong(sb);
			ufo->time = MSG_ReadLong(sb);
			ufo->point = MSG_ReadShort(sb);
			ufo->route.numPoints = MSG_ReadShort(sb);
			if (ufo->route.numPoints > LINE_MAXPTS) {
				Com_Printf("AIR_Load: number of points (%i) for UFO route exceed maximum value (%i)\n", ufo->route.numPoints, LINE_MAXPTS);
				return qfalse;
			}
			ufo->route.distance = MSG_ReadFloat(sb);
			for (j = 0; j < ufo->route.numPoints; j++)
				MSG_Read2Pos(sb, ufo->route.point[j]);
			MSG_ReadPos(sb, ufo->direction);
			gd.ufos[i].mission = CP_GetMissionById(MSG_ReadString(sb));
			for (j = 0; j < presaveArray[PRE_AIRSTA]; j++)
				ufo->stats[j] = MSG_ReadLong(sb);
			tmp_int = MSG_ReadShort(sb);
			if (tmp_int == -1)
				ufo->baseTarget = NULL;
			else
				ufo->baseTarget = B_GetBaseByIDX(tmp_int);
			tmp_int = MSG_ReadShort(sb);
			if (tmp_int == -1)
				ufo->aircraftTarget = NULL;
			else
				ufo->aircraftTarget = AIR_AircraftGetFromIDX(tmp_int);
			/* read weapon slot */
			tmp_int = MSG_ReadByte(sb);
			if (tmp_int > MAX_AIRCRAFTSLOT) {
				Com_Printf("AIR_Load: number of weapon slots (%i) for UFO exceed maximum value (%i)\n", tmp_int, MAX_AIRCRAFTSLOT);
				return qfalse;
			}
			for (j = 0; j < tmp_int; j++) {
				/* check that there are enough slots in this aircraft */
				if (j < ufo->maxWeapons) {
					tech = RS_GetTechByProvided(MSG_ReadString(sb));
					if (tech)
						AII_AddItemToSlot(NULL, tech, ufo->weapons + j, qfalse);
					ufo->weapons[j].ammoLeft = MSG_ReadShort(sb);
					ufo->weapons[j].delayNextShot = MSG_ReadShort(sb);
					ufo->weapons[j].installationTime = MSG_ReadShort(sb);
					tech = RS_GetTechByProvided(MSG_ReadString(sb));
					if (tech)
						ufo->weapons[j].ammo = AII_GetAircraftItemByID(tech->provides);
				} else {
					/* just in case there are less slots in new game than in saved one */
					MSG_ReadString(sb);
					MSG_ReadShort(sb);
					MSG_ReadShort(sb);
					MSG_ReadShort(sb);
					MSG_ReadString(sb);
				}
			}
			/* check for shield slot */
			/* there is only one shield - but who knows - breaking the savegames if this changes
			 * isn't worth it */
			tmp_int = MSG_ReadByte(sb);
			if (tmp_int) {
				tech = RS_GetTechByProvided(MSG_ReadString(sb));
				if (tech)
					AII_AddItemToSlot(NULL, tech, &ufo->shield, qfalse);
				ufo->shield.installationTime = MSG_ReadShort(sb);
			}
			/* read electronics slot */
			tmp_int = MSG_ReadByte(sb);
			if (tmp_int > MAX_AIRCRAFTSLOT) {
				Com_Printf("AIR_Load: number of electronic slots (%i) for UFO exceed maximum value (%i)\n", tmp_int, MAX_AIRCRAFTSLOT);
				return qfalse;
			}
			for (j = 0; j < tmp_int; j++) {
				/* check that there are enough slots in this aircraft */
				if (j < ufo->maxElectronics) {
					tech = RS_GetTechByProvided(MSG_ReadString(sb));
					if (tech)
						AII_AddItemToSlot(NULL, tech, ufo->electronics + j, qfalse);
					ufo->electronics[j].installationTime = MSG_ReadShort(sb);
				} else {
					MSG_ReadString(sb);
					MSG_ReadShort(sb);
				}
			}
			numUFOs++;
		}
		/** @todo more? */
	}

	if (numUFOs != gd.numUFOs)
		Com_Printf("AIR_Load: loaded %i UFOs, but there should be %i\n", numUFOs, gd.numUFOs);

	/* Load projectiles. */
	gd.numProjectiles = MSG_ReadByte(sb);
	if (gd.numProjectiles > MAX_PROJECTILESONGEOSCAPE) {
		Com_Printf("AIR_Load: Too many projectiles on map (%i)\n", gd.numProjectiles);
		return qfalse;
	}


	for (i = 0; i < gd.numProjectiles; i++) {
		tech = RS_GetTechByProvided(MSG_ReadString(sb));
		if (tech) {
			gd.projectiles[i].aircraftItem = AII_GetAircraftItemByID(tech->provides);
			gd.projectiles[i].idx = i;
			for (j = 0; j < presaveArray[PRE_MAXMPR]; j++)
				MSG_Read2Pos(sb, gd.projectiles[i].pos[j]);
			MSG_ReadPos(sb, gd.projectiles[i].idleTarget);
			tmp_int = MSG_ReadByte(sb);
			if (tmp_int == 2)
				gd.projectiles[i].attackingAircraft = NULL;
			else if (tmp_int == 1)
				gd.projectiles[i].attackingAircraft = gd.ufos + MSG_ReadShort(sb);
			else
				gd.projectiles[i].attackingAircraft = AIR_AircraftGetFromIDX(MSG_ReadShort(sb));
			tmp_int = MSG_ReadShort(sb);
			if (tmp_int >= 0)
				gd.projectiles[i].aimedBase = B_GetBaseByIDX(tmp_int);
			else
				gd.projectiles[i].aimedBase = NULL;
			tmp_int = MSG_ReadByte(sb);
			if (tmp_int == 2)
				gd.projectiles[i].aimedAircraft = NULL;
			else if (tmp_int == 1)
				gd.projectiles[i].aimedAircraft = gd.ufos + MSG_ReadShort(sb);
			else
				gd.projectiles[i].aimedAircraft = AIR_AircraftGetFromIDX(MSG_ReadShort(sb));
			gd.projectiles[i].time = MSG_ReadLong(sb);
			gd.projectiles[i].angle = MSG_ReadFloat(sb);
			gd.projectiles[i].bullets = MSG_ReadByte(sb);
			gd.projectiles[i].laser = MSG_ReadByte(sb);
		} else {
			Com_Printf("AIR_Load: Could not get technology of projectile %i\n", i);
			return qfalse;
		}
	}

	/* Load recoveries. */
	for (i = 0; i < presaveArray[PRE_MAXREC]; i++) {
		byte base, ufotype;
		gd.recoveries[i].active = MSG_ReadByte(sb);
		base = MSG_ReadByte(sb);
		gd.recoveries[i].base = (base != BYTES_NONE) ? B_GetBaseByIDX((byte)base) : NULL;
		assert(numAircraftTemplates);
		ufotype = MSG_ReadByte(sb);
		gd.recoveries[i].ufoTemplate = (ufotype != BYTES_NONE) ? &aircraftTemplates[ufotype] : NULL;
		gd.recoveries[i].event.day = MSG_ReadLong(sb);
		gd.recoveries[i].event.sec = MSG_ReadLong(sb);
	}

	for (i = gd.numUFOs - 1; i >= 0; i--) {
		if (gd.ufos[i].time < 0 || gd.ufos[i].stats[AIR_STATS_SPEED] <= 0) {
			Com_Printf("AIR_Load: Found invalid ufo entry - remove it - time: %i - speed: %i\n",
				gd.ufos[i].time, gd.ufos[i].stats[AIR_STATS_SPEED]);
			UFO_RemoveFromGeoscape(&gd.ufos[i]);
		}
	}

	return qtrue;
}

/**
 * @brief Returns true if the current base is able to handle aircraft
 * @sa B_BaseInit_f
 * @note Hangar must be accessible during base attack to make aircraft lift off and to equip soldiers.
 */
qboolean AIR_AircraftAllowed (const base_t* base)
{
	if ((B_GetBuildingStatus(base, B_HANGAR) || B_GetBuildingStatus(base, B_SMALL_HANGAR))) {
		return qtrue;
	} else {
		return qfalse;
	}
}

/**
 * @brief Checks the parsed aircraft for errors
 * @return false if there are errors - true otherwise
 */
qboolean AIR_ScriptSanityCheck (void)
{
	int i, j, k, error = 0;
	int var;
	aircraft_t* a;

	for (i = 0, a = aircraftTemplates; i < numAircraftTemplates; i++, a++) {
		if (!a->name) {
			error++;
			Com_Printf("...... aircraft '%s' has no name\n", a->id);
		}
		if (!a->shortname) {
			error++;
			Com_Printf("...... aircraft '%s' has no shortname\n", a->id);
		}

		/* check that every weapons fits slot */
		for (j = 0; j < a->maxWeapons - 1; j++)
			if (a->weapons[j].item && AII_GetItemWeightBySize(a->weapons[j].item) > a->weapons[j].size) {
				error++;
				Com_Printf("...... aircraft '%s' has an item (%s) too heavy for its slot\n", a->id, a->weapons[j].item->id);
			}

		/* check that every slots has a different location for PHALANX aircraft (not needed for UFOs) */
		if (a->type != AIRCRAFT_UFO) {
			for (j = 0; j < a->maxWeapons - 1; j++) {
				var = a->weapons[j].pos;
				for (k = j + 1; k < a->maxWeapons; k++)
					if (var == a->weapons[k].pos) {
						error++;
						Com_Printf("...... aircraft '%s' has 2 weapons slots at the same location\n", a->id);
					}
			}
			for (j = 0; j < a->maxElectronics - 1; j++) {
				var = a->electronics[j].pos;
				for (k = j + 1; k < a->maxElectronics; k++)
					if (var == a->electronics[k].pos) {
						error++;
						Com_Printf("...... aircraft '%s' has 2 electronics slots at the same location\n", a->id);
					}
			}
		}
	}

	return !error;
}

/**
 * @brief Calculates free space in hangars in given base.
 * @param[in] aircraftTemplate aircraft in aircraftTemplates list.
 * @param[in] base The base to calc the free space in.
 * @param[in] used Additional space "used" in hangars (use that when calculating space for more than one aircraft).
 * @return Amount of free space in hangars suitable for given aircraft type.
 * @note Returns -1 in case of error. Returns 0 if no error but no free space.
 */
int AIR_CalculateHangarStorage (const aircraft_t *aircraftTemplate, const base_t *base, int used)
{
	int aircraftSize = 0, freespace = 0;

	assert(aircraftTemplate);
	assert(aircraftTemplate == aircraftTemplate->tpl);	/* Make sure it's an aircraft template. */

	aircraftSize = aircraftTemplate->size;

	if (aircraftSize < AIRCRAFT_SMALL) {
#ifdef DEBUG
		Com_Printf("AIR_CalculateHangarStorage: aircraft weight is wrong!\n");
#endif
		return -1;
	}
	if (!base) {
#ifdef DEBUG
		Com_Printf("AIR_CalculateHangarStorage: base does not exist!\n");
#endif
		return -1;
	} else if (!base->founded) {
#ifdef DEBUG
		Com_Printf("AIR_CalculateHangarStorage: base is not founded!\n");
#endif
		return -1;
	}
	assert(base);

	/* If this is a small aircraft, we will check space in small hangar. */
	if (aircraftSize == AIRCRAFT_SMALL) {
		freespace = base->capacities[CAP_AIRCRAFT_SMALL].max - base->capacities[CAP_AIRCRAFT_SMALL].cur - used;
		Com_DPrintf(DEBUG_CLIENT, "AIR_CalculateHangarStorage: freespace (small): %i aircraft weight: %i (max: %i, cur: %i)\n", freespace, aircraftSize,
			base->capacities[CAP_AIRCRAFT_SMALL].max, base->capacities[CAP_AIRCRAFT_SMALL].cur);
		if (freespace > 0) {
			return freespace;
		} else {
			/* No free space for this aircraft. */
			return 0;
		}
	} else {
		/* If this is a large aircraft, we will check space in big hangar. */
		freespace = base->capacities[CAP_AIRCRAFT_BIG].max - base->capacities[CAP_AIRCRAFT_BIG].cur - used;
		Com_DPrintf(DEBUG_CLIENT, "AIR_CalculateHangarStorage: freespace (big): %i aircraft weight: %i (max: %i, cur: %i)\n", freespace, aircraftSize,
			base->capacities[CAP_AIRCRAFT_BIG].max, base->capacities[CAP_AIRCRAFT_BIG].cur);
		if (freespace > 0) {
			return freespace;
		} else {
			/* No free space for this aircraft. */
			return 0;
		}
	}
}

/**
 * @brief Removes a soldier from an aircraft.
 * @param[in,out] employee The soldier to be removed from the aircraft.
 * @param[in,out] aircraft The aircraft to remove the soldier from.
 * Use @c NULL to remove the soldier from any aircraft.
 * @sa AIR_AddEmployee
 */
qboolean AIR_RemoveEmployee (employee_t *employee, aircraft_t *aircraft)
{
	if (!employee)
		return qfalse;

	/* If no aircraft is given we search if he is in _any_ aircraft and set
	 * the aircraft pointer to it. */
	if (!aircraft) {
		int i;
		for (i = 0; i < gd.numAircraft; i++) {
			aircraft_t *acTemp = AIR_AircraftGetFromIDX(i);
			if (AIR_IsEmployeeInAircraft(employee, acTemp)) {
				aircraft = acTemp;
				break;
			}
		}
		if (!aircraft)
			return qfalse;
	}

	Com_DPrintf(DEBUG_CLIENT, "AIR_RemoveEmployee: base: %i - aircraft->idx: %i\n",
		aircraft->homebase ? aircraft->homebase->idx : -1, aircraft->idx);

	INVSH_DestroyInventory(&employee->chr.inv);
	return AIR_RemoveFromAircraftTeam(aircraft, employee);
}

/**
 * @brief Tells you if a soldier is assigned to an aircraft.
 * @param[in] employee The soldier to search for.
 * @param[in] aircraft The aircraft to search the soldier in. Use @c NULL to
 * check if the soldier is in @b any aircraft.
 * @return true if the soldier was found in the aircraft otherwise false.
 */
const aircraft_t *AIR_IsEmployeeInAircraft (const employee_t *employee, const aircraft_t* aircraft)
{
	int i;

	if (!employee)
		return NULL;

	if (employee->transfer)
		return NULL;

	/* If no aircraft is given we search if he is in _any_ aircraft and return true if that's the case. */
	if (!aircraft) {
		for (i = 0; i < gd.numAircraft; i++) {
			const aircraft_t *aircraftByIDX = AIR_AircraftGetFromIDX(i);
			if (aircraftByIDX && AIR_IsEmployeeInAircraft(employee, aircraftByIDX))
				return aircraftByIDX;
		}
		return NULL;
	}

	if (employee->type == EMPL_PILOT) {
		if (aircraft->pilot == employee)
			return aircraft;
		return NULL;
	}

	if (AIR_IsInAircraftTeam(aircraft, employee))
		return aircraft;
	else
		return NULL;
}

/**
 * @brief Removes all soldiers from an aircraft.
 * @param[in,out] aircraft The aircraft to remove the soldiers from.
 * @sa AIR_RemoveEmployee
 */
void AIR_RemoveEmployees (aircraft_t *aircraft)
{
	int i;

	if (!aircraft)
		return;

	/* Counting backwards because aircraft->acTeam[] is changed in AIR_RemoveEmployee */
	for (i = aircraft->maxTeamSize; i >= 0; i--) {
		/* use global aircraft index here */
		if (AIR_RemoveEmployee(aircraft->acTeam[i], aircraft)) {
			/* if the acTeam is not NULL the acTeam list and AIR_IsEmployeeInAircraft
			 * is out of sync */
			assert(aircraft->acTeam[i] == NULL);
		} else if (aircraft->acTeam[i]) {
			Com_Printf("AIR_RemoveEmployees: Error, could not remove soldier from aircraft team at pos: %i\n", i);
		}
	}

	if (aircraft->teamSize > 0)
		Sys_Error("AIR_RemoveEmployees: Error, there went something wrong with soldier-removing from aircraft.");
}


/**
 * @brief Move all the equipment carried by the team on the aircraft into the given equipment
 * @param[in] aircraft The craft with the team (and thus equipment) onboard.
 * @param[out] ed The equipment definition which will receive all the stuff from the aircraft-team.
 */
void AIR_MoveEmployeeInventoryIntoStorage (const aircraft_t *aircraft, equipDef_t *ed)
{
	int p, container;

	if (!aircraft) {
		Com_Printf("AIR_MoveEmployeeInventoryIntoStorage: Warning: Called with no aicraft (and thus no carried equipment to add).\n");
		return;
	}
	if (!ed) {
		Com_Printf("AIR_MoveEmployeeInventoryIntoStorage: Warning: Called with no equipment definition at add stuff to.\n");
		return;
	}

	if (aircraft->teamSize <= 0) {
		Com_DPrintf(DEBUG_CLIENT, "AIR_MoveEmployeeInventoryIntoStorage: No team to remove equipment from.\n");
		return;
	}

	for (container = 0; container < csi.numIDs; container++) {
		for (p = 0; p < aircraft->maxTeamSize; p++) {
			if (aircraft->acTeam[p]) {
				character_t *chr = &aircraft->acTeam[p]->chr;
				invList_t *ic = chr->inv.c[container];
				while (ic) {
					const item_t item = ic->item;
					const objDef_t *type = item.t;
					invList_t *next = ic->next;

					ed->num[type->idx]++;
					if (item.a) {
						assert(type->reload);
						assert(item.m);
						ed->numLoose[item.m->idx] += item.a;
						/* Accumulate loose ammo into clips */
						if (ed->numLoose[item.m->idx] >= type->ammo) {
							ed->numLoose[item.m->idx] -= type->ammo;
							ed->num[item.m->idx]++;
						}
					}
					ic = next;
				}
			}
		}
	}
}

/**
 * @brief Assigns a soldier to an aircraft.
 * @param[in] employee The employee to be assigned to the aircraft.
 * @param[in] aircraft What aircraft to assign employee to.
 * @return returns true if a soldier could be assigned to the aircraft.
 * @sa AIR_RemoveEmployee
 * @sa AIR_AddToAircraftTeam
 */
static qboolean AIR_AddEmployee (employee_t *employee, aircraft_t *aircraft)
{
	if (!employee || !aircraft)
		return qfalse;

	if (aircraft->teamSize < MAX_ACTIVETEAM) {
		Com_DPrintf(DEBUG_CLIENT, "AIR_AddEmployee: attempting to find idx '%d'\n", employee->idx);

		/* Check whether the soldier is already on another aircraft */
		if (AIR_IsEmployeeInAircraft(employee, NULL)) {
			Com_DPrintf(DEBUG_CLIENT, "AIR_AddEmployee: found idx '%d' \n", employee->idx);
			return qfalse;
		}

		/* Assign the soldier to the aircraft. */
		if (aircraft->teamSize < aircraft->maxTeamSize) {
			Com_DPrintf(DEBUG_CLIENT, "AIR_AddEmployee: attempting to add idx '%d' \n", employee->idx);
			return AIR_AddToAircraftTeam(aircraft, employee);
		}
#ifdef DEBUG
	} else {
		Com_DPrintf(DEBUG_CLIENT, "AIR_AddEmployee: aircraft full - not added\n");
#endif
	}
	return qfalse;
}

/**
 * @brief Adds or removes a soldier to/from an aircraft.
 * @sa E_EmployeeHire_f
 */
void AIM_AddEmployeeFromMenu (aircraft_t *aircraft, const int num)
{
	employee_t *employee;

	Com_DPrintf(DEBUG_CLIENT, "AIM_AddEmployeeFromMenu: Trying to get employee with hired-idx %i.\n", num);

	/* If this fails it's very likely that employeeList is not filled. */
	employee = E_GetEmployeeByMenuIndex(num);
	if (!employee)
		Sys_Error("AIM_AddEmployeeFromMenu: Could not get employee %i\n", num);

	Com_DPrintf(DEBUG_CLIENT, "AIM_AddEmployeeFromMenu: employee with idx %i selected\n", employee->idx);

	assert(aircraft);

	if (AIR_IsEmployeeInAircraft(employee, aircraft)) {
		/* Remove soldier from aircraft/team. */
		MN_ExecuteConfunc("listdel %i", num);
		/* use the global aircraft index here */
		AIR_RemoveEmployee(employee, aircraft);
		MN_ExecuteConfunc("listholdsnoequip %i", num);
	} else {
		/* Assign soldier to aircraft/team if aircraft is not full */
		if (AIR_AddEmployee(employee, aircraft))
			MN_ExecuteConfunc("listadd %i", num);
		else
			MN_ExecuteConfunc("listdel %i", num);
	}
	/* Select the desired one anyways. */
	CL_UpdateActorAircraftVar(aircraft, employee->type);
	Cbuf_AddText(va("team_select %i\n", num));
}
