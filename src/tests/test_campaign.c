/**
 * @file test_campaign.c
 * @brief Test cases for the campaign code
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "CUnit/Basic.h"
#include "test_shared.h"
#include "test_campaign.h"
#include "../client/client.h"
#include "../client/ui/ui_main.h"
#include "../client/campaign/cp_campaign.h"

static const int TAG_INVENTORY = 1538;

static void FreeInventory (void *data)
{
	Mem_Free(data);
}

static void *AllocInventoryMemory (size_t size)
{
	return Mem_PoolAlloc(size, com_genericPool, TAG_INVENTORY);
}

static void FreeAllInventory (void)
{
	Mem_FreeTag(com_genericPool, TAG_INVENTORY);
}

static const inventoryImport_t inventoryImport = { FreeInventory, FreeAllInventory, AllocInventoryMemory };

static inline void ResetInventoryList (void)
{
	INV_DestroyInventory(&cls.i);
	INV_InitInventory("testCampaign", &cls.i, &csi, &inventoryImport);
}

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteCampaign (void)
{
	TEST_Init();

	memset(&cls, 0, sizeof(cls));

	cl_genericPool = Mem_CreatePool("Client: Generic");
	cp_campaignPool = Mem_CreatePool("Client: Local (per game)");
	cp_campaign = Cvar_Get("cp_campaign", "main", 0, NULL);
	cp_missiontest = Cvar_Get("cp_missiontest", "0", 0, NULL);

	Cmd_AddCommand("msgoptions_set", Cmd_Dummy_f, NULL);

	CL_SetClientState(ca_disconnected);
	cls.realtime = Sys_Milliseconds();

	UI_Init();

	CL_ResetSinglePlayerData();
	CL_ReadSinglePlayerData();
	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteCampaign (void)
{
	TEST_Shutdown();
	return 0;
}

static void testCampaign (void)
{
	missionResults_t result;
	battleParam_t battleParameters;
	aircraft_t* aircraft = ccs.aircraftTemplates;
	mission_t *mission = CP_CreateNewMission(INTERESTCATEGORY_RECON, qfalse);
	campaign_t *campaign;
	employee_t *pilot, *e1, *e2;

	memset(&result, 0, sizeof(result));
	memset(&battleParameters, 0, sizeof(battleParameters));

	ResetInventoryList();

	campaign = CL_GetCampaign(cp_campaign->string);
	CU_ASSERT_TRUE_FATAL(campaign != NULL);
	ccs.curCampaign = campaign;

	CU_ASSERT_PTR_NOT_NULL(aircraft);
	CU_ASSERT_PTR_NOT_NULL(mission);

	pilot = E_CreateEmployee(EMPL_PILOT, NULL, NULL);
	CU_ASSERT_PTR_NOT_NULL(pilot);
	AIR_SetPilot(aircraft, pilot);

	e1 = E_CreateEmployee(EMPL_SOLDIER, NULL, NULL);
	AIR_AddToAircraftTeam(aircraft, e1);
	e2 = E_CreateEmployee(EMPL_SOLDIER, NULL, NULL);
	AIR_AddToAircraftTeam(aircraft, e2);

	CU_ASSERT_EQUAL(AIR_GetTeamSize(aircraft), 2);

	battleParameters.probability = -1.0;

	CL_GameAutoGo(mission, aircraft, &battleParameters, &result);

	CU_ASSERT_EQUAL(result.won, battleParameters.probability < result.winProbability);

	E_DeleteEmployee(pilot);
	E_DeleteEmployee(e1);

	/** @todo this is not working until we have removed the aircraft from
	 * bases - because this test does not have any bases */
	/*CU_ASSERT_EQUAL(AIR_GetTeamSize(aircraft), 1);*/
	CU_ASSERT_PTR_NULL(AIR_GetPilot(aircraft));
}

int UFO_AddCampaignTests (void)
{
	/* add a suite to the registry */
	CU_pSuite campaignSuite = CU_add_suite("CampaignTests", UFO_InitSuiteCampaign, UFO_CleanSuiteCampaign);

	if (campaignSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(campaignSuite, testCampaign) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
