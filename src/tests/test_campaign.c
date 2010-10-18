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
#include "../client/campaign/cp_missions.h"
#include "../client/campaign/cp_nations.h"
#include "../client/campaign/cp_overlay.h"
#include "../client/campaign/cp_ufo.h"

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

static campaign_t *GetCampaign (void)
{
	return CL_GetCampaign(cp_campaign->string);
}

static void ResetCampaignData (void)
{
	campaign_t *campaign;

	CL_ResetSinglePlayerData();
	CL_ReadSinglePlayerData();

	ResetInventoryList();

	CL_UpdateCredits(MAX_CREDITS);

	MAP_Shutdown();
	campaign = GetCampaign();
	MAP_Init(campaign->map);
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

	cl_geoscape_overlay = Cvar_Get("cl_geoscape_overlay", "0", 0, NULL);

	CL_SetClientState(ca_disconnected);
	cls.realtime = Sys_Milliseconds();

	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteCampaign (void)
{
	UI_Shutdown();
	CL_ResetSinglePlayerData();
	TEST_Shutdown();
	return 0;
}

static base_t* CreateBase (const char *name, const vec2_t pos)
{
	campaign_t *campaign = GetCampaign();
	base_t *base = B_GetFirstUnfoundedBase();

	CU_ASSERT_PTR_NOT_NULL_FATAL(campaign);
	CU_ASSERT_PTR_NOT_NULL_FATAL(base);

	RS_InitTree(campaign, qfalse);
	B_SetUpBase(campaign, base, pos, name);

	return base;
}

static void testEmployeeHandling (void)
{
	employeeType_t type;

	ResetCampaignData();

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
	base_t *base;
	campaign_t *campaign;
	employeeType_t type;

	ResetCampaignData();

	campaign = GetCampaign();

	ccs.credits = 10000000;

	base = CreateBase("unittestcreatebase", pos);

	B_Destroy(base);

	for (type = 0; type < MAX_EMPL; type++)
		CU_ASSERT_EQUAL(E_CountHired(base, type), 0);

	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testAutoMissions (void)
{
	missionResults_t result;
	battleParam_t battleParameters;
	aircraft_t* aircraft = ccs.aircraftTemplates;
	mission_t *mission;
	campaign_t *campaign;
	employee_t *pilot, *e1, *e2;

	ResetCampaignData();

	memset(&result, 0, sizeof(result));
	memset(&battleParameters, 0, sizeof(battleParameters));

	mission = CP_CreateNewMission(INTERESTCATEGORY_RECON, qfalse);
	campaign = GetCampaign();
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
	ResetCampaignData();
}

static void testResearch (void)
{
	int n;
	int i;
	const vec2_t pos = {0, 0};
	technology_t *laserTech;
	technology_t *heavyLaserTech;
	base_t *base;
	campaign_t *campaign;
	employee_t *employee;

	ResetCampaignData();

	laserTech = RS_GetTechByID("rs_laser");
	CU_ASSERT_PTR_NOT_NULL_FATAL(laserTech);
	heavyLaserTech = RS_GetTechByID("rs_weapon_heavylaser");
	CU_ASSERT_PTR_NOT_NULL_FATAL(heavyLaserTech);
	campaign = GetCampaign();

	base = CreateBase("unittestbase", pos);

	CU_ASSERT_TRUE(laserTech->statusResearchable);

	employee = E_GetUnassignedEmployee(base, EMPL_SCIENTIST);
	CU_ASSERT_PTR_NOT_NULL(employee);
	RS_AssignScientist(laserTech, base, employee);

	CU_ASSERT_EQUAL(laserTech->base, base);
	CU_ASSERT_EQUAL(laserTech->scientists, 1);
	CU_ASSERT_EQUAL(laserTech->statusResearch, RS_RUNNING);

	CU_ASSERT_EQUAL(heavyLaserTech->statusResearchable, qfalse);

	n = laserTech->time * 1.25;
	for (i = 0; i < n; i++) {
		RS_ResearchRun();
	}

	CU_ASSERT_EQUAL(laserTech->statusResearch, RS_RUNNING);
	CU_ASSERT_EQUAL(RS_ResearchRun(), 1);
	CU_ASSERT_EQUAL(laserTech->statusResearch, RS_FINISH);

	CU_ASSERT_EQUAL(heavyLaserTech->statusResearchable, qtrue);

	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testProductionItem (void)
{
	const vec2_t pos = {0, 0};
	base_t *base;
	const objDef_t *od;
	const technology_t *tech;
	int old;
	int i, n;

	ResetCampaignData();

	base = CreateBase("unittestproduction", pos);

	CU_ASSERT_TRUE(B_AtLeastOneExists());
	CU_ASSERT_TRUE(B_GetBuildingStatus(base, B_WORKSHOP));
	CU_ASSERT_TRUE(E_CountHired(base, EMPL_WORKER) > 0);
	CU_ASSERT_TRUE(PR_ProductionAllowed(base));

	od = INVSH_GetItemByID("assault");
	CU_ASSERT_PTR_NOT_NULL_FATAL(od);
	old = base->storage.numItems[od->idx];
	CU_ASSERT_PTR_NOT_NULL(PR_QueueNew(base, od, NULL, NULL, 1));
	tech = RS_GetTechForItem(od);
	n = tech->produceTime * MINUTES_PER_HOUR - 1;
	for (i = 0; i < n; i++)
		PR_ProductionRun();
	CU_ASSERT_EQUAL(old, base->storage.numItems[od->idx]);
	PR_ProductionRun();
	CU_ASSERT_EQUAL(old + 1, base->storage.numItems[od->idx]);

	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testProductionAircraft (void)
{
	ResetCampaignData();
}

static void testMap (void)
{
	vec2_t pos;

	ResetCampaignData();

	Vector2Set(pos, -51, 0);
	CU_ASSERT_TRUE(MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN)));

	Vector2Set(pos, 20, 20);
	CU_ASSERT_TRUE(MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN)));
}

static void testAirFight (void)
{
	const vec2_t destination = { 10, 10 };
	mission_t *mission;
	aircraft_t *aircraft;
	base_t *base;
	int i, cnt;
	/* just some random delta time value that is high enough
	 * to ensure that all the weapons are reloaded */
	const int deltaTime = 1000;

	ResetCampaignData();

	base = CreateBase("unittestairfight", destination);
	CU_ASSERT_PTR_NOT_NULL_FATAL(base);

	aircraft = AIR_GetNextFromBase(base, NULL);
	CU_ASSERT_PTR_NOT_NULL_FATAL(aircraft);
	aircraft->status = AIR_IDLE;
	CU_ASSERT_TRUE(AIR_IsAircraftOnGeoscape(aircraft));

	cnt = AIR_BaseCountAircraft(base);

	/* prepare the mission */
	mission = CP_CreateNewMission(INTERESTCATEGORY_INTERCEPT, qtrue);
	CU_ASSERT_PTR_NOT_NULL_FATAL(mission);
	CU_ASSERT_EQUAL(mission->stage, STAGE_NOT_ACTIVE);
	CP_InterceptNextStage(mission);
	CU_ASSERT_EQUAL(mission->stage, STAGE_COME_FROM_ORBIT);
	CP_InterceptNextStage(mission);
	CU_ASSERT_EQUAL(mission->stage, STAGE_INTERCEPT);
	mission->ufo->ufotype = UFO_FIGHTER;
	/* we have to update the routing data here to be sure that the ufo is
	 * not spawned on the other side of the globe */
	Vector2Copy(destination, mission->ufo->pos);
	UFO_SendToDestination(mission->ufo, destination);
	CU_ASSERT_TRUE(VectorEqual(mission->ufo->pos, base->pos));
	CU_ASSERT_TRUE(VectorEqual(mission->ufo->pos, aircraft->pos));

	CU_ASSERT_TRUE(aircraft->maxWeapons > 0);
	for (i = 0; i < aircraft->maxWeapons; i++)
		CU_ASSERT_TRUE(aircraft->weapons[i].delayNextShot == 0);

	/* search a target */
	UFO_CampaignRunUFOs(deltaTime);
	CU_ASSERT_PTR_NOT_NULL_FATAL(mission->ufo->aircraftTarget);

	/* ensure that one hit will destroy the craft */
	mission->ufo->aircraftTarget->damage = 1;
	UFO_CheckShootBack(mission->ufo, mission->ufo->aircraftTarget);

	/* one projectile should be spawned */
	CU_ASSERT_EQUAL(ccs.numProjectiles, 1);
	AIRFIGHT_CampaignRunProjectiles(deltaTime);

	/* target is destroyed */
	CU_ASSERT_PTR_NULL(mission->ufo->aircraftTarget);

	/* one aircraft less */
	CU_ASSERT_EQUAL(cnt - 1, AIR_BaseCountAircraft(base));

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testGeoscape (void)
{
	ResetCampaignData();
}

static void testRadar (void)
{
	base_t *base;
	const vec2_t destination = { 10, 10 };
	aircraft_t *ufo;
	mission_t *mission;
	ufoType_t ufoType = UFO_FIGHTER;

	ResetCampaignData();

	base = CreateBase("unittestradar", destination);

	mission = CP_CreateNewMission(INTERESTCATEGORY_INTERCEPT, qtrue);
	ufo = UFO_AddToGeoscape(ufoType, destination, mission);
	Vector2Copy(destination, ufo->pos);
	UFO_SendToDestination(ufo, destination);
	CU_ASSERT_TRUE(VectorEqual(ufo->pos, base->pos));
	CU_ASSERT_TRUE(VectorEqual(ufo->pos, ufo->pos));
	/* to ensure that the UFOs are really detected when they are in range */
	base->radar.ufoDetectionProbability = 1.0;
	CU_ASSERT_TRUE(RADAR_CheckUFOSensored(&base->radar, base->pos, ufo, qfalse));

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testNation (void)
{
	const nation_t *nation;
	campaign_t *campaign;

	ResetCampaignData();

	nation = NAT_GetNationByID("europe");
	CU_ASSERT_PTR_NOT_NULL(nation);

	campaign = GetCampaign();

	CP_NationHandleBudget(campaign);
	/** @todo implement a check here */
}

static void testMarket (void)
{
	campaign_t *campaign;

	ResetCampaignData();

	campaign = GetCampaign();

	RS_InitTree(campaign, qfalse);

	BS_InitMarket(campaign);

	CL_CampaignRunMarket(campaign);
	/** @todo implement a check here */
}

static void testXVI (void)
{
	ResetCampaignData();
}

static void testSaveLoad (void)
{
	const vec2_t pos = {0, 0};
	base_t *base;
	campaign_t *campaign;

	ResetCampaignData();

	campaign = GetCampaign();

	SAV_Init();

	ccs.curCampaign = campaign;

	Cvar_Set("save_compressed", "0");

	base = CreateBase("unittestbase", pos);

	{
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
		ResetCampaignData();

		Cmd_ExecuteString("game_quickload");

		/** @todo check that the savegame was successfully loaded */

		CU_ASSERT_EQUAL(base->baseStatus, BASE_WORKING);
		CU_ASSERT_STRING_EQUAL(base->name, "unittestbase");

		B_Destroy(base);

		E_DeleteAllEmployees(NULL);
	}

	base->founded = qfalse;
}

static void testCampaignRun (void)
{
	ResetCampaignData();

	cls.frametime = 1;
	ccs.gameTimeScale = 1;

	CL_CampaignRun(GetCampaign());
	/** @todo implement a check here */
}

static void testLoad (void)
{
	int i;
	aircraft_t *ufo;

	ResetCampaignData();

	CP_InitOverlay();

	Cvar_Set("mn_unittest1", "foobar");

	ccs.curCampaign = NULL;
	Cmd_ExecuteString("game_load unittest1");
	CU_ASSERT_PTR_NOT_NULL(ccs.curCampaign);

	i = 0;
	ufo = NULL;
	while ((ufo = UFO_GetNextOnGeoscape(ufo)) != NULL)
		i++;

	/* there should be one ufo on the geoscape */
	CU_ASSERT_EQUAL(i, 1);
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

	if (CU_ADD_TEST(campaignSuite, testProductionItem) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testProductionAircraft) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testMap) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testAirFight) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testGeoscape) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testRadar) == NULL)
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

	if (CU_ADD_TEST(campaignSuite, testLoad) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
