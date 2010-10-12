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
#include "../client/renderer/r_state.h"
#include "../client/ui/ui_main.h"
#include "../client/campaign/cp_campaign.h"
#include "../client/campaign/cp_map.h"

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

	cl_genericPool = Mem_CreatePool("Client: Generic");
	cp_campaignPool = Mem_CreatePool("Client: Local (per game)");
	cp_campaign = Cvar_Get("cp_campaign", "main", 0, NULL);
	cp_missiontest = Cvar_Get("cp_missiontest", "0", 0, NULL);
	vid_imagePool = Mem_CreatePool("Vid: Image system");

	r_state.active_texunit = &r_state.texunits[0];
	R_FontInit();
	UI_Init();

	memset(&cls, 0, sizeof(cls));
	Com_ParseScripts(qfalse);

	Cmd_AddCommand("msgoptions_set", Cmd_Dummy_f, NULL);

	CL_SetClientState(ca_disconnected);
	cls.realtime = Sys_Milliseconds();

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
	UI_Shutdown();
	TEST_Shutdown();
	return 0;
}

static void testEmployeeHandling (void)
{
	employeeType_t type;

	ResetInventoryList();

	for (type = 0; type < MAX_EMPL; type++) {
		if (type != EMPL_ROBOT) {
			int cnt;
			employee_t *e = E_CreateEmployee(type, NULL, NULL);
			CU_ASSERT_PTR_NOT_NULL(e);

			cnt = E_CountUnhired(type);
			CU_ASSERT_EQUAL(cnt, 1);

			E_DeleteEmployee(e);

			cnt = E_CountUnhired(type);
			CU_ASSERT_EQUAL(cnt, 0);
		}
	}

	{
		const ugv_t *ugvType = Com_GetUGVByID("ugv_ares_w");
		employee_t *e = E_CreateEmployee(EMPL_ROBOT, NULL, ugvType);
		CU_ASSERT_PTR_NOT_NULL(e);
		CU_ASSERT_TRUE(E_DeleteEmployee(e));
	}

	/** @todo activate this once the employees are a linked list */
#if 0
	{
		int i, cnt;
		employee_t *e;

		for (i = 0; i < MAX_EMPLOYEES; i++) {
			e = E_CreateEmployee(EMPL_SOLDIER, NULL, NULL);
			CU_ASSERT_PTR_NOT_NULL(e);

			cnt = E_CountUnhired(EMPL_SOLDIER);
			CU_ASSERT_EQUAL(cnt, i + 1);
		}
		e = NULL;
		while ((e = E_GetNext(EMPL_SOLDIER, e)) != NULL) {
			CU_ASSERT_TRUE(E_DeleteEmployee(e));
		}

		cnt = E_CountUnhired(EMPL_SOLDIER);
		CU_ASSERT_EQUAL(cnt, 0);
	}
#endif
}

static void testBaseBuilding (void)
{
	vec2_t pos = {0, 0};
	base_t *base = B_GetFirstUnfoundedBase();
	campaign_t *campaign = CL_GetCampaign(cp_campaign->string);
	employeeType_t type;

	ResetInventoryList();

	/* the global pointer should not be used anywhere inside the called functions */
	ccs.curCampaign = NULL;
	ccs.credits = 10000000;

	RS_InitTree(campaign, qfalse);

	B_SetUpBase(campaign, base, pos, "testbase");

	B_Destroy(base);

	for (type = 0; type < MAX_EMPL; type++)
		CU_ASSERT_EQUAL(E_CountHired(base, type), 0);

	E_DeleteAllEmployees(NULL);
}

static void testAutoMissions (void)
{
	missionResults_t result;
	battleParam_t battleParameters;
	aircraft_t* aircraft = ccs.aircraftTemplates;
	mission_t *mission = CP_CreateNewMission(INTERESTCATEGORY_RECON, qfalse);
	campaign_t *campaign;
	employee_t *pilot, *e1, *e2;

	memset(&result, 0, sizeof(result));
	memset(&battleParameters, 0, sizeof(battleParameters));

	/* the global pointer should not be used anywhere inside the called functions */
	ccs.curCampaign = NULL;

	ResetInventoryList();

	campaign = CL_GetCampaign(cp_campaign->string);
	CU_ASSERT_TRUE_FATAL(campaign != NULL);

	CU_ASSERT_PTR_NOT_NULL(aircraft);
	CU_ASSERT_PTR_NOT_NULL(mission);

	pilot = E_CreateEmployee(EMPL_PILOT, NULL, NULL);
	CU_ASSERT_PTR_NOT_NULL(pilot);
	AIR_SetPilot(aircraft, pilot);

	e1 = E_CreateEmployee(EMPL_SOLDIER, NULL, NULL);
	CU_ASSERT_TRUE(AIR_AddToAircraftTeam(aircraft, e1));
	e2 = E_CreateEmployee(EMPL_SOLDIER, NULL, NULL);
	CU_ASSERT_TRUE(AIR_AddToAircraftTeam(aircraft, e2));

	CU_ASSERT_EQUAL(AIR_GetTeamSize(aircraft), 2);

	battleParameters.probability = -1.0;

	CL_GameAutoGo(mission, aircraft, campaign, &battleParameters, &result);

	CU_ASSERT_EQUAL(result.won, battleParameters.probability < result.winProbability);

	CU_ASSERT_TRUE(AIR_SetPilot(aircraft, NULL));
	CU_ASSERT_PTR_NULL(AIR_GetPilot(aircraft));

	CU_ASSERT_TRUE(AIR_RemoveEmployee(e1, aircraft));
	CU_ASSERT_EQUAL(AIR_GetTeamSize(aircraft), 1);

	CU_ASSERT_TRUE(AIR_RemoveEmployee(e2, aircraft));
	CU_ASSERT_EQUAL(AIR_GetTeamSize(aircraft), 0);

	/** @todo this is crap - we have to delete in this order, otherwise the pointers are
	 * garbage (remove this once the employees are stored in a linked list) */
	CU_ASSERT_TRUE(E_DeleteEmployee(e2));
	CU_ASSERT_TRUE(E_DeleteEmployee(e1));
	CU_ASSERT_TRUE(E_DeleteEmployee(pilot));

	CU_ASSERT_EQUAL(E_CountUnhired(EMPL_SOLDIER), 0);
	CU_ASSERT_EQUAL(E_CountUnhired(EMPL_PILOT), 0);
}

static void testTransfer (void)
{
}

static void testResearch (void)
{
}

static void testProduction (void)
{
}

static void testAirFight (void)
{
}

static void testGeoscape (void)
{
}

static void testNation (void)
{
}

static void testMarket (void)
{
}

static void testXVI (void)
{
}

static void testSaveLoad (void)
{
	vec2_t pos = {0, 0};
	base_t *base = B_GetFirstUnfoundedBase();
	campaign_t *campaign = CL_GetCampaign(cp_campaign->string);

	memset(&ccs.campaignStats, 0, sizeof(ccs.campaignStats));

	SAV_Init();

	ResetInventoryList();

	cl_geoscape_overlay = Cvar_Get("cl_geoscape_overlay", "0", 0, NULL);
	ccs.credits = 10000000;
	ccs.curCampaign = campaign;

	Cvar_Set("save_compressed", "0");

	RS_InitTree(campaign, qfalse);

	{
		B_SetUpBase(campaign, base, pos, "unittestbase");

		CU_ASSERT(base->founded);
		CU_ASSERT_EQUAL(base->baseStatus, BASE_WORKING);

		Cmd_ExecuteString("game_quicksave");
	}
	{
		B_Destroy(base);

		CU_ASSERT_EQUAL(base->baseStatus, BASE_DESTROYED);

		E_DeleteAllEmployees(NULL);

		B_SetName(base, "unittestbase2");
	}
	{
		Cmd_ExecuteString("game_quickload");

		/** @todo check that the savegame was successfully loaded */

		CU_ASSERT_EQUAL(base->baseStatus, BASE_WORKING);
		CU_ASSERT_STRING_EQUAL(base->name, "unittestbase");

		B_Destroy(base);

		E_DeleteAllEmployees(NULL);
	}
}

static void testCampaignRun (void)
{
	ccs.curCampaign = CL_GetCampaign(cp_campaign->string);
	cls.frametime = 1;
	ccs.gameTimeScale = 1;

	/*CL_CampaignRun();*/
}

int UFO_AddCampaignTests (void)
{
	/* add a suite to the registry */
	CU_pSuite campaignSuite = CU_add_suite("CampaignTests", UFO_InitSuiteCampaign, UFO_CleanSuiteCampaign);

	if (campaignSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(campaignSuite, testAutoMissions) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testBaseBuilding) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testEmployeeHandling) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testTransfer) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testResearch) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testProduction) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testAirFight) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testGeoscape) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testNation) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testMarket) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testXVI) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testSaveLoad) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testCampaignRun) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
