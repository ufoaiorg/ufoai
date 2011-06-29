/**
 * @file cp_alien_interest.c
 * @brief Alien interest values influence the campaign actions
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

#include "../../cl_shared.h"
#include "cp_campaign.h"
#include "cp_alien_interest.h"
#include "save/save_interest.h"

/**
 * @brief Initialize alien interest values and mission cycle
 * @note Should be used when a new single player campaign starts
 * @sa CP_CampaignInit
 */
void INT_ResetAlienInterest (void)
{
	int i;

	ccs.lastInterestIncreaseDelay = 0;
	ccs.lastMissionSpawnedDelay = 0;
	ccs.overallInterest = INITIAL_OVERALL_INTEREST;

	for (i = 0; i < INTERESTCATEGORY_MAX; i++)
		ccs.interest[i] = 0;
	ccs.interest[INTERESTCATEGORY_RECON] = INITIAL_OVERALL_INTEREST;
}

/**
 * @brief Change individual interest value
 * @param[in] interestFactor Maybe pe > 0 or < 0
 * @param[in] category Category of interest for aliens that should increase / decrease
 * @note Should be used when a mission is over (success or failure)
 */
void INT_ChangeIndividualInterest (float interestFactor, interestCategory_t category)
{
	if (category == INTERESTCATEGORY_MAX) {
		Com_Printf("CP_ChangeIndividualInterest: Unsupported value of category\n");
		return;
	}

	if (interestFactor > 0.0f) {
		const int gain = (int) (interestFactor * ccs.overallInterest);
		const int diff = ccs.overallInterest - ccs.interest[category];
		/* Fraction of individual interest that will be won if
		 * individal interest becomes higher than overall interest. 0 means no increase */
		const float slowerIncreaseFraction = 0.5f;
		/* Value increases: interestFactor is taken from the overall interest value
		 * But it increase slower if the individual interest becomes higher than the overall interest value */
		if (diff > gain)
			/* Final value of individual interest is below overall interest */
			ccs.interest[category] += gain;
		else if (diff > 0)
			/* Initial value of individual interest is below overall interest */
			ccs.interest[category] = ccs.overallInterest + (int) (slowerIncreaseFraction * (gain - diff));
		else
			/* Final value of individual interest is above overall interest */
			ccs.interest[category] += (int) (slowerIncreaseFraction * gain);
	} else {
		/* Value decreases: interestFactor is taken from the individual interest value */
		ccs.interest[category] += (int) (interestFactor * ccs.interest[category]);
		if (ccs.interest[category] < 0) {
			/* this may be reached if interestFactor is below -1 */
			ccs.interest[category] = 0;
		}
	}
}

/**
 * @brief Increase alien overall interest.
 * @sa CP_CampaignRun
 * @note hourly called
 */
void INT_IncreaseAlienInterest (const campaign_t *campaign)
{
	/* Adjust interest increase rate by difficulty. */
	const int delayBetweenIncrease = HOURS_PER_ONE_INTEREST - campaign->difficulty;

	ccs.lastInterestIncreaseDelay++;

	if (ccs.lastInterestIncreaseDelay > delayBetweenIncrease) {
		ccs.overallInterest++;
		ccs.lastInterestIncreaseDelay %= delayBetweenIncrease;
	}
}

/**
 * @brief Save callback for savegames in XML Format
 * @param[out] parent XML Node structure, where we write the information to
 */
qboolean INT_SaveXML (xmlNode_t *parent)
{
	xmlNode_t *interestsNode = XML_AddNode(parent, SAVE_INTERESTS);
	int i;

	XML_AddShortValue(interestsNode, SAVE_INTERESTS_LASTINCREASEDELAY, ccs.lastInterestIncreaseDelay);
	XML_AddShortValue(interestsNode, SAVE_INTERESTS_LASTMISSIONSPAWNEDDELAY, ccs.lastMissionSpawnedDelay);
	XML_AddShortValue(interestsNode, SAVE_INTERESTS_OVERALL, ccs.overallInterest);
	Com_RegisterConstList(saveInterestConstants);
	for (i = 0; i < INTERESTCATEGORY_MAX; i++) {
		xmlNode_t * interestNode = XML_AddNode(interestsNode, SAVE_INTERESTS_INTEREST);
		XML_AddString(interestNode, SAVE_INTERESTS_ID, Com_GetConstVariable(SAVE_INTERESTCAT_NAMESPACE, i));
		XML_AddShort(interestNode, SAVE_INTERESTS_VAL, ccs.interest[i]);
	}
	Com_UnregisterConstList(saveInterestConstants);
	return qtrue;
}

/**
 * @brief Load callback for savegames in XML Format
 * @param[in] parent XML Node structure, where we get the information from
 */
qboolean INT_LoadXML (xmlNode_t *parent)
{
	xmlNode_t *node;
	xmlNode_t *interestsNode = XML_GetNode(parent, SAVE_INTERESTS);
	qboolean success = qtrue;

	ccs.lastInterestIncreaseDelay = XML_GetInt(interestsNode, SAVE_INTERESTS_LASTINCREASEDELAY, 0);
	ccs.lastMissionSpawnedDelay = XML_GetInt(interestsNode, SAVE_INTERESTS_LASTMISSIONSPAWNEDDELAY, 0);
	ccs.overallInterest = XML_GetInt(interestsNode, SAVE_INTERESTS_OVERALL, 0);
	Com_RegisterConstList(saveInterestConstants);
	for (node = XML_GetNode(interestsNode, SAVE_INTERESTS_INTEREST); node;
			node = XML_GetNextNode(node, interestsNode, SAVE_INTERESTS_INTEREST)) {
		const char *categoryId = XML_GetString(node, SAVE_INTERESTS_ID);
		int cat;

		if (!Com_GetConstInt(categoryId, (int*) &cat)) {
			Com_Printf("Invalid interest category '%s'\n", categoryId);
			success = qfalse;
			break;
		}
		ccs.interest[cat]= XML_GetInt(node, SAVE_INTERESTS_VAL, 0);
	}
	Com_UnregisterConstList(saveInterestConstants);
	return success;
}

#ifdef DEBUG
/**
 * @brief Return Name of the category of a mission.
 */
const char* INT_InterestCategoryToName (interestCategory_t category)
{
	switch (category) {
	case INTERESTCATEGORY_NONE:
		return "None";
	case INTERESTCATEGORY_RECON:
		return "Recon mission";
	case INTERESTCATEGORY_TERROR_ATTACK:
		return "Terror mission";
	case INTERESTCATEGORY_BASE_ATTACK:
		return "Base attack";
	case INTERESTCATEGORY_BUILDING:
		return "Building Base or Subverting Government";
	case INTERESTCATEGORY_SUPPLY:
		return "Supply base";
	case INTERESTCATEGORY_XVI:
		return "XVI propagation";
	case INTERESTCATEGORY_INTERCEPT:
		return "Interception";
	case INTERESTCATEGORY_HARVEST:
		return "Harvest";
	case INTERESTCATEGORY_ALIENBASE:
		return "Alien base discovered";
	case INTERESTCATEGORY_RESCUE:
		return "Rescue mission";
	case INTERESTCATEGORY_MAX:
		return "Unknown mission category";
	}

	/* Can't reach this point */
	return "INVALID";
}

/**
 * @brief List alien interest values.
 * @sa function called with debug_interestlist
 */
static void INT_AlienInterestList_f (void)
{
	interestCategory_t i;

	Com_Printf("Overall interest: %i\n", ccs.overallInterest);
	Com_Printf("Individual interest:\n");
	for (i = INTERESTCATEGORY_NONE; i < INTERESTCATEGORY_MAX; i++)
		Com_Printf("...%i. %s -- %i\n", i, INT_InterestCategoryToName(i), ccs.interest[i]);
}

/**
 * @brief Debug callback to set overall interest level
 * Called: debug_interestset <interestlevel>
 */
static void INT_SetAlienInterest_f (void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <interestlevel>\n", Cmd_Argv(0));
		return;
	}

	ccs.overallInterest = max(0, atoi(Cmd_Argv(1)));
}
#endif

/**
 * @brief Init actions for alien interests-subsystem
 */
void INT_InitStartup (void)
{
#ifdef DEBUG
	Cmd_AddCommand("debug_interestlist", INT_AlienInterestList_f, "Debug function to show alien interest values");
	Cmd_AddCommand("debug_interestset", INT_SetAlienInterest_f, "Set overall interest level.");
#endif
}

/**
 * @brief Closing actions for alien interests-subsystem
 */
void INT_Shutdown (void)
{
#ifdef DEBUG
	Cmd_RemoveCommand("debug_interestlist");
	Cmd_RemoveCommand("debug_interestset");
#endif
}
