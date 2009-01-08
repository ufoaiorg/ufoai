/**
 * @file cp_base_callbacks.c
 * @brief Menu related console command callbacks
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

#include "../client.h"
#include "../cl_global.h"
#include "../menu/m_popup.h"
#include "../renderer/r_draw.h"
#include "cp_base_callbacks.h"
#include "cp_base.h"
#include "cl_map.h"

/** @brief Used from menu scripts as parameter for mn_select_base */
#define CREATE_NEW_BASE_ID -1

static cvar_t *mn_base_title;

/**
 * @brief Called when a base is opened or a new base is created on geoscape.
 * For a new base the baseID is -1.
 */
static void B_SelectBase_f (void)
{
	int baseID;
	base_t *base;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", Cmd_Argv(0));
		return;
	}
	baseID = atoi(Cmd_Argv(1));
	/* check against MAX_BASES here! - only -1 will create a new base
	 * if we would check against gd.numBases here, a click on the base summary
	 * base nodes would try to select unfounded bases */
	if (baseID >= 0 && baseID < MAX_BASES) {
		base = B_GetFoundedBaseByIDX(baseID);
		/* don't create a new base if the index was valid */
		if (base)
			B_SelectBase(base);
	} else if (baseID == CREATE_NEW_BASE_ID) {
		/* create a new base */
		B_SelectBase(NULL);
	}
}

/**
 * @brief Cycles to the next base.
 * @sa B_PrevBase
 * @sa B_SelectBase_f
 */
static void B_NextBase_f (void)
{
	int baseID;
	base_t *base = baseCurrent;

	if (!base)
		return;

	baseID = (base->idx + 1) % gd.numBases;
	base = B_GetFoundedBaseByIDX(baseID);
	if (base)
		B_SelectBase(base);
}

/**
 * @brief Cycles to the previous base.
 * @sa B_NextBase
 * @sa B_SelectBase_f
 */
static void B_PrevBase_f (void)
{
	int baseID;
	base_t *base = baseCurrent;

	if (!base)
		return;

	baseID = base->idx;
	if (baseID > 0)
		baseID--;
	else
		baseID = gd.numBases - 1;

	base = B_GetFoundedBaseByIDX(baseID);
	if (base)
		B_SelectBase(base);
}

/**
 * @brief Check conditions for new base and build it.
 * @param[in] pos Position on the geoscape.
 * @return True if the base has been build.
 * @sa B_BuildBase
 */
static qboolean B_NewBase (base_t* base, vec2_t pos)
{
	byte *colorTerrain;

	assert(base);

	if (base->founded) {
		Com_DPrintf(DEBUG_CLIENT, "B_NewBase: base already founded: %i\n", base->idx);
		return qfalse;
	} else if (gd.numBases == MAX_BASES) {
		Com_DPrintf(DEBUG_CLIENT, "B_NewBase: max base limit hit\n");
		return qfalse;
	}

	colorTerrain = MAP_GetColor(pos, MAPTYPE_TERRAIN);

	if (MapIsWater(colorTerrain)) {
		/* This should already have been catched in MAP_MapClick (cl_menu.c), but just in case. */
		MN_AddNewMessage(_("Notice"), _("Could not set up your base at this location"), qfalse, MSG_INFO, NULL);
		return qfalse;
	} else {
		Com_DPrintf(DEBUG_CLIENT, "B_NewBase: zoneType: '%s'\n", MAP_GetTerrainType(colorTerrain));
	}

	Com_DPrintf(DEBUG_CLIENT, "Colorvalues for base terrain: R:%i G:%i B:%i\n", colorTerrain[0], colorTerrain[1], colorTerrain[2]);

	/* build base */
	Vector2Copy(pos, base->pos);

	gd.numBases++;

	/* set up the base with buildings that have the autobuild flag set */
	B_SetUpBase(base, cl_start_employees->integer, cl_start_buildings->integer);

	return qtrue;
}

/**
 * @brief Constructs a new base.
 * @sa B_NewBase
 */
static void B_BuildBase_f (void)
{
	const nation_t *nation;
	base_t *base = baseCurrent;

	if (!base)
		return;

	assert(!base->founded);
	assert(GAME_IsCampaign());
	assert(curCampaign);

	if (ccs.credits - curCampaign->basecost > 0) {
		/** @todo If there is no nation assigned to the current selected position,
		 * tell this the gamer and give him an option to rechoose the location.
		 * If we don't do this, any action that is done for this base has no
		 * influence to any nation happiness/funding/supporting */
		if (B_NewBase(base, newBasePos)) {
			Com_DPrintf(DEBUG_CLIENT, "B_BuildBase_f: numBases: %i\n", gd.numBases);
			base->idx = gd.numBases - 1;
			base->founded = qtrue;
			base->baseStatus = BASE_WORKING;
			campaignStats.basesBuild++;
			gd.mapAction = MA_NONE;
			CL_UpdateCredits(ccs.credits - curCampaign->basecost);
			Q_strncpyz(base->name, mn_base_title->string, sizeof(base->name));
			nation = MAP_GetNation(base->pos);
			if (nation)
				Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("A new base has been built: %s (nation: %s)"), mn_base_title->string, _(nation->name));
			else
				Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("A new base has been built: %s"), mn_base_title->string);
			MN_AddNewMessage(_("Base built"), mn.messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);
			B_ResetAllStatusAndCapacities(base, qtrue);
			AL_FillInContainment(base);
			PR_UpdateProductionCap(base);

			B_SelectBase(base);
			return;
		}
	} else {
		if (r_geoscape_overlay->integer & OVERLAY_RADAR)
			MAP_SetOverlay("radar");
		if (gd.mapAction == MA_NEWBASE)
			gd.mapAction = MA_NONE;

		Com_sprintf(popupText, sizeof(popupText), _("Not enough credits to set up a new base."));
		MN_Popup(_("Notice"), popupText);
	}
}

/**
 * @brief Creates console command to change the name of a base.
 * Copies the value of the cvar mn_base_title over as the name of the
 * current selected base
 */
static void B_ChangeBaseName_f (void)
{
	/* maybe called without base initialized or active */
	if (!baseCurrent)
		return;

	Q_strncpyz(baseCurrent->name, Cvar_VariableString("mn_base_title"), sizeof(baseCurrent->name));
}

/** @todo unify the names into mn_base_* */
void B_InitCallbacks (void)
{
	Cmd_AddCommand("mn_prev_base", B_PrevBase_f, "Go to the previous base");
	Cmd_AddCommand("mn_next_base", B_NextBase_f, "Go to the next base");
	Cmd_AddCommand("mn_select_base", B_SelectBase_f, "Select a founded base by index");
	Cmd_AddCommand("mn_build_base", B_BuildBase_f, NULL);
	Cmd_AddCommand("base_changename", B_ChangeBaseName_f, "Called after editing the cvar base name");
}

/** @todo unify the names into mn_base_* */
void B_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("mn_prev_base");
	Cmd_RemoveCommand("mn_next_base");
	Cmd_RemoveCommand("mn_select_base");
	Cmd_RemoveCommand("mn_build_base");
	Cmd_RemoveCommand("base_changename");
}