/**
 * @file test_campaign.c
 * @brief Test cases for the campaign code
 */

/*
Copyright (C) 2002-2012 UFO: Alien Invasion.

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

#include "test_shared.h"
#include "test_campaign.h"
#include "../client/client.h"
#include "../client/cgame/cl_game.h"
#include "../client/renderer/r_state.h" /* r_state */
#include "../client/ui/ui_main.h"
#include "../client/cgame/campaign/cp_campaign.h"
#include "../client/cgame/campaign/cp_map.h"
#include "../client/cgame/campaign/cp_hospital.h"
#include "../client/cgame/campaign/cp_missions.h"
#include "../client/cgame/campaign/cp_nation.h"
#include "../client/cgame/campaign/cp_overlay.h"
#include "../client/cgame/campaign/cp_ufo.h"
#include "../client/cgame/campaign/cp_time.h"
#include "../client/cgame/campaign/cp_alien_interest.h"
#include "../client/cgame/campaign/cp_auto_mission.h"

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
	return CP_GetCampaign(cp_campaign->string);
}

static void ResetCampaignData (void)
{
	const campaign_t *campaign;

	CP_ResetCampaignData();
	CP_ParseCampaignData();
	campaign = GetCampaign();
	CP_ReadCampaignData(campaign);

	ResetInventoryList();

	CP_UpdateCredits(MAX_CREDITS);

	MAP_Shutdown();
	MAP_Init(campaign->map);

	ccs.curCampaign = campaign;
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
	GAME_InitStartup();

	OBJZERO(cls);
	Com_ParseScripts(qfalse);

	Cmd_ExecuteString("game_setmode campaigns");

	Cmd_AddCommand("msgoptions_set", Cmd_Dummy_f, NULL);

	cl_geoscape_overlay = Cvar_Get("cl_geoscape_overlay", "0", 0, NULL);
	cl_3dmap = Cvar_Get("cl_3dmap", "1", 0, NULL);

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
	CP_ResetCampaignData();
	TEST_Shutdown();
	return 0;
}

static installation_t* CreateInstallation (const char *name, const vec2_t pos)
{
	const installationTemplate_t *installationTemplate = INS_GetInstallationTemplateFromInstallationID("ufoyard");
	installation_t *installation;

	CU_ASSERT_PTR_NOT_NULL_FATAL(installationTemplate);

	installation = INS_Build(installationTemplate, pos, name);
	CU_ASSERT_PTR_NOT_NULL_FATAL(installation);
	CU_ASSERT_EQUAL(installation->installationStatus, INSTALLATION_UNDER_CONSTRUCTION);

	/* fake the build time */
	installation->buildStart = ccs.date.day - installation->installationTemplate->buildTime;
	INS_UpdateInstallationData();
	CU_ASSERT_EQUAL(installation->installationStatus, INSTALLATION_WORKING);

	return installation;
}

static base_t* CreateBase (const char *name, const vec2_t pos)
{
	const campaign_t *campaign = GetCampaign();
	base_t *base;

	CU_ASSERT_PTR_NOT_NULL_FATAL(campaign);

	RS_InitTree(campaign, qfalse);
	E_InitialEmployees(campaign);
	base = B_Build(campaign, pos, name);
	CU_ASSERT_PTR_NOT_NULL_FATAL(base);
	CU_ASSERT_TRUE(base->founded);

	/* First base */
	if (ccs.campaignStats.basesBuilt == 1)
		B_SetUpFirstBase(campaign, base);

	return base;
}

static void testAircraftHandling (void)
{
	const vec2_t destination = { 10, 10 };
	base_t *base;
	aircraft_t *aircraft;
	aircraft_t *newAircraft;
	aircraft_t *aircraftTemplate;
	int firstIdx;
	int initialCount;
	int count;
	int newFound;

	ResetCampaignData();

	base = CreateBase("unittestaircraft", destination);
	CU_ASSERT_PTR_NOT_NULL_FATAL(base);

	/** @todo we should not assume that initial base has aircraft. It's a campaign parameter */
	aircraft = AIR_GetFirstFromBase(base);
	CU_ASSERT_PTR_NOT_NULL_FATAL(aircraft);

	/* aircraft should have a template */
	aircraftTemplate = aircraft->tpl;
	CU_ASSERT_PTR_NOT_NULL_FATAL(aircraftTemplate);

	firstIdx = aircraft->idx;
	initialCount = AIR_BaseCountAircraft(base);

	/* test deletion (part 1) */
	AIR_DeleteAircraft(aircraft);
	count = AIR_BaseCountAircraft(base);
	CU_ASSERT_EQUAL(count, initialCount - 1);

	/* test addition (part 1) */
	newAircraft = AIR_NewAircraft(base, aircraftTemplate);
	CU_ASSERT_PTR_NOT_NULL_FATAL(newAircraft);
	count = AIR_BaseCountAircraft(base);
	CU_ASSERT_EQUAL(count, initialCount);

	/* new aircraft assigned to the right base */
	CU_ASSERT_EQUAL(newAircraft->homebase, base);

	newFound = 0;
	AIR_Foreach(a) {
		/* test deletion (part 2) */
		CU_ASSERT_NOT_EQUAL(firstIdx, a->idx);
		/* for test addition (part 2) */
		if (a->idx == newAircraft->idx)
			newFound++;
	}
	/* test addition (part 2) */
	CU_ASSERT_EQUAL(newFound, 1);

	/* check if AIR_Foreach iterates through all aircraft */
	AIR_Foreach(a) {
		AIR_DeleteAircraft(a);
	}
	aircraft = AIR_GetFirstFromBase(base);
	CU_ASSERT_PTR_NULL_FATAL(aircraft);
	count = AIR_BaseCountAircraft(base);
	CU_ASSERT_EQUAL(count, 0);

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testEmployeeHandling (void)
{
	employeeType_t type;

	ResetCampaignData();

	for (type = 0; type < MAX_EMPL; type++) {
		int cnt;
		employee_t *e = E_CreateEmployee(type, NULL);
		CU_ASSERT_PTR_NOT_NULL(e);

		cnt = E_CountUnhired(type);
		CU_ASSERT_EQUAL(cnt, 1);

		E_DeleteEmployee(e);

		cnt = E_CountUnhired(type);
		CU_ASSERT_EQUAL(cnt, 0);
	}

	{
		int i;
		const int amount = 3;
		for (i = 0; i < amount; i++) {
			employee_t *e = E_CreateEmployee(EMPL_SOLDIER, NULL);
			CU_ASSERT_PTR_NOT_NULL(e);
		}
		{
			int cnt = 0;
			E_Foreach(EMPL_SOLDIER, e) {
				(void)e;
				cnt++;
			}

			CU_ASSERT_EQUAL(cnt, amount);

			E_Foreach(EMPL_SOLDIER, e) {
				CU_ASSERT_TRUE(E_DeleteEmployee(e));
			}

			cnt = E_CountUnhired(EMPL_SOLDIER);
			CU_ASSERT_EQUAL(cnt, 0)
		}
	}

	{
		employee_t *e = E_CreateEmployee(EMPL_PILOT, NULL);
		int cnt;
		CU_ASSERT_PTR_NOT_NULL(e);
		cnt = E_RefreshUnhiredEmployeeGlobalList(EMPL_PILOT, qfalse);
		CU_ASSERT_EQUAL(cnt, 1);
		CU_ASSERT_TRUE(E_DeleteEmployee(e));
	}

	{
		int i, cnt;
		employee_t *e;

		for (i = 0; i < MAX_EMPLOYEES; i++) {
			e = E_CreateEmployee(EMPL_SOLDIER, NULL);
			CU_ASSERT_PTR_NOT_NULL(e);

			cnt = E_CountUnhired(EMPL_SOLDIER);
			CU_ASSERT_EQUAL(cnt, i + 1);
		}
		E_DeleteAllEmployees(NULL);

		cnt = E_CountUnhired(EMPL_SOLDIER);
		CU_ASSERT_EQUAL(cnt, 0);
	}
}

static void testBaseBuilding (void)
{
	vec2_t pos = {0, 0};
	base_t *base;
	employeeType_t type;

	ResetCampaignData();

	ccs.credits = 10000000;

	base = CreateBase("unittestcreatebase", pos);

	CU_ASSERT_EQUAL(B_GetInstallationLimit(), MAX_INSTALLATIONS_PER_BASE);

	B_Destroy(base);

	for (type = 0; type < MAX_EMPL; type++)
		CU_ASSERT_EQUAL(E_CountHired(base, type), 0);

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testAutoMissions (void)
{
	const vec2_t pos = {-73.2, 18.5};
	base_t *base;
	missionResults_t result;
	battleParam_t battleParameters;
	aircraft_t* aircraft;
	mission_t *mission;
	campaign_t *campaign;

	ResetCampaignData();

	campaign = GetCampaign();
	CU_ASSERT_TRUE_FATAL(campaign != NULL);

	OBJZERO(result);
	OBJZERO(battleParameters);

	ccs.overallInterest = 36;
	INT_ResetAlienInterest();

	base = CreateBase("unittestautomission", pos);
	CU_ASSERT_PTR_NOT_NULL_FATAL(base);

	aircraft = NULL;
	AIR_ForeachFromBase(a, base) {
		if (AIR_GetTeamSize(a) > 0) {
			aircraft = a;
			break;
		}
	}
	CU_ASSERT_PTR_NOT_NULL_FATAL(aircraft);
	CU_ASSERT_TRUE(AIR_GetTeamSize(aircraft) > 0);

	mission = CP_CreateNewMission(INTERESTCATEGORY_RECON, qfalse);
	Vector2Copy(pos, mission->pos);
	mission->posAssigned = qtrue;
	mission->mapDef = Com_GetMapDefinitionByID("farm2");
	CU_ASSERT_PTR_NOT_NULL(mission);

	CP_CreateBattleParameters(mission, &battleParameters, aircraft);
	AM_Go(mission, aircraft, campaign, &battleParameters, &result);

	CU_ASSERT_TRUE(result.won);
}

static void testTransferItem (void)
{
	const campaign_t *campaign = GetCampaign();
	const vec2_t pos = {0, 0};
	const vec2_t posTarget = {51, 0};
	base_t *base, *targetBase;
	transferData_t td;
	const objDef_t *od;
	transfer_t *transfer;

	ResetCampaignData();

	base = CreateBase("unittesttransferitem", pos);
	CU_ASSERT_PTR_NOT_NULL_FATAL(base);
	/* make sure that we get all buildings in our second base, too.
	 * This is needed for starting a transfer */
	targetBase = CreateBase("unittesttransferitemtargetbase", posTarget);
	CU_ASSERT_PTR_NOT_NULL_FATAL(targetBase);
	B_SetUpFirstBase(campaign, targetBase);

	od = INVSH_GetItemByID("assault");
	CU_ASSERT_PTR_NOT_NULL_FATAL(od);

	OBJZERO(td);
	td.trItemsTmp[od->idx] += 1;
	td.transferBase = targetBase;
	TR_AddData(&td, CARGO_TYPE_ITEM, od);

	transfer = TR_TransferStart(base, &td);
	CU_ASSERT_PTR_NOT_NULL_FATAL(transfer);

	CU_ASSERT_EQUAL(LIST_Count(ccs.transfers), 1);

	transfer->event = ccs.date;
	transfer->event.day++;

	/* check if it's arrived immediately */
	TR_TransferRun();
	CU_ASSERT_FALSE(LIST_IsEmpty(ccs.transfers));

	/* check if it arrives (even a second) before it should */
	ccs.date.day++;
	ccs.date.sec--;
	TR_TransferRun();
	CU_ASSERT_FALSE(LIST_IsEmpty(ccs.transfers));

	/* check if it arrived right when it should */
	ccs.date.sec++;
	TR_TransferRun();
	CU_ASSERT_TRUE(LIST_IsEmpty(ccs.transfers));

	/* Start another transfer to check higher time lapse */
	transfer = TR_TransferStart(base, &td);
	CU_ASSERT_PTR_NOT_NULL_FATAL(transfer);
	CU_ASSERT_EQUAL(LIST_Count(ccs.transfers), 1);

	transfer->event = ccs.date;
	transfer->event.day++;

	/* check if it arrived when time passed the deadline by days already */
	ccs.date.day += 2;
	TR_TransferRun();
	CU_ASSERT_TRUE(LIST_IsEmpty(ccs.transfers));

	/* Start another transfer to check higher time lapse */
	transfer = TR_TransferStart(base, &td);
	CU_ASSERT_PTR_NOT_NULL_FATAL(transfer);
	CU_ASSERT_EQUAL(LIST_Count(ccs.transfers), 1);

	transfer->event = ccs.date;
	transfer->event.day++;

	/* check if it arrived when time passed the deadline by days already */
	ccs.date.day++;
	ccs.date.sec++;
	TR_TransferRun();
	CU_ASSERT_TRUE(LIST_IsEmpty(ccs.transfers));


	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testTransferAircraft (void)
{
	ResetCampaignData();
}

static void testTransferEmployee (void)
{
	ResetCampaignData();

	/** @todo test the transfer of employees and make sure that they can't be used for
	 * anything in the base while they are transfered */
}

static void testUFORecovery (void)
{
	const vec2_t pos = {0, 0};
	const aircraft_t *ufo;
	storedUFO_t *storedUFO;
	installation_t *installation;
	date_t date = ccs.date;

	ResetCampaignData();

	ufo = AIR_GetAircraft("craft_ufo_fighter");
	CU_ASSERT_PTR_NOT_NULL_FATAL(ufo);

	CreateBase("unittestproduction", pos);

	installation = CreateInstallation("unittestuforecovery", pos);

	date.day++;
	storedUFO = US_StoreUFO(ufo, installation, date, 1.0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(storedUFO);
	CU_ASSERT_EQUAL(storedUFO->status, SUFO_RECOVERED);

	UR_ProcessActive();

	CU_ASSERT_EQUAL(storedUFO->status, SUFO_RECOVERED);

	ccs.date.day++;

	UR_ProcessActive();

	CU_ASSERT_EQUAL(storedUFO->status, SUFO_STORED);

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);
}

static void testResearch (void)
{
	int n;
	int i;
	const vec2_t pos = {0, 0};
	technology_t *laserTech;
	technology_t *heavyLaserTech;
	base_t *base;
	employee_t *employee;

	ResetCampaignData();
	RS_MarkResearchable(qtrue, NULL);

	laserTech = RS_GetTechByID("rs_laser");
	CU_ASSERT_PTR_NOT_NULL_FATAL(laserTech);
	heavyLaserTech = RS_GetTechByID("rs_weapon_heavylaser");
	CU_ASSERT_PTR_NOT_NULL_FATAL(heavyLaserTech);

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

	/* cleanup for the following tests */
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
	productionData_t data;
	production_t *prod;

	ResetCampaignData();

	base = CreateBase("unittestproduction", pos);

	CU_ASSERT_TRUE(B_AtLeastOneExists());
	CU_ASSERT_TRUE(B_GetBuildingStatus(base, B_WORKSHOP));
	CU_ASSERT_TRUE(E_CountHired(base, EMPL_WORKER) > 0);
	CU_ASSERT_TRUE(PR_ProductionAllowed(base));

	od = INVSH_GetItemByID("assault");
	CU_ASSERT_PTR_NOT_NULL_FATAL(od);

	PR_SetData(&data, PRODUCTION_TYPE_ITEM, od);
	old = base->storage.numItems[od->idx];
	prod = PR_QueueNew(base, &data, 1);
	CU_ASSERT_PTR_NOT_NULL(prod);
	tech = RS_GetTechForItem(od);
	n = PR_GetRemainingMinutes(prod);
	i = tech->produceTime;
	CU_ASSERT_EQUAL(i, PR_GetRemainingHours(prod));
	for (i = 0; i < n; i++) {
		PR_ProductionRun();
	}

	CU_ASSERT_EQUAL(old, base->storage.numItems[od->idx]);
	PR_ProductionRun();
	CU_ASSERT_EQUAL(old + 1, base->storage.numItems[od->idx]);

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testProductionAircraft (void)
{
	const vec2_t pos = {0, 0};
	base_t *base;
	const aircraft_t *aircraft;
	int old;
	int i, n;
	const building_t *buildingTemplate;
	building_t *building;
	productionData_t data;
	production_t *prod;

	ResetCampaignData();

	base = CreateBase("unittestproduction", pos);

	CU_ASSERT_TRUE(B_AtLeastOneExists());
	CU_ASSERT_TRUE(B_GetBuildingStatus(base, B_WORKSHOP));
	CU_ASSERT_TRUE(E_CountHired(base, EMPL_WORKER) > 0);
	CU_ASSERT_TRUE(PR_ProductionAllowed(base));

	aircraft = AIR_GetAircraft("craft_drop_firebird");
	CU_ASSERT_PTR_NOT_NULL_FATAL(aircraft);

	PR_SetData(&data, PRODUCTION_TYPE_AIRCRAFT, aircraft);
	old = CAP_GetCurrent(base, CAP_AIRCRAFT_SMALL);
	/* not enough space in hangars */
	CU_ASSERT_PTR_NULL(PR_QueueNew(base, &data, 1));

	buildingTemplate = B_GetBuildingTemplate("building_intercept");
	CU_ASSERT_PTR_NOT_NULL_FATAL(buildingTemplate);
	building = B_SetBuildingByClick(base, buildingTemplate, 1, 1);
	CU_ASSERT_PTR_NOT_NULL_FATAL(building);
	/* hangar is not yet done */
	CU_ASSERT_PTR_NULL(PR_QueueNew(base, &data, 1));

	/** @todo make this a function once base stuff is refactored */
	building->buildingStatus = B_STATUS_WORKING;
	B_UpdateBaseCapacities(B_GetCapacityFromBuildingType(building->buildingType), base);

	aircraft = AIR_GetAircraft("craft_inter_stingray");
	CU_ASSERT_PTR_NOT_NULL_FATAL(aircraft);
	PR_SetData(&data, PRODUCTION_TYPE_AIRCRAFT, aircraft);
	/* no antimatter */
	CU_ASSERT_PTR_NULL(PR_QueueNew(base, &data, 1));

	aircraft = AIR_GetAircraft("craft_inter_stiletto");
	CU_ASSERT_PTR_NOT_NULL_FATAL(aircraft);
	PR_SetData(&data, PRODUCTION_TYPE_AIRCRAFT, aircraft);
	prod = PR_QueueNew(base, &data, 1);
	CU_ASSERT_PTR_NOT_NULL(prod);

	n = PR_GetRemainingMinutes(prod);
	i = aircraft->tech->produceTime;
	CU_ASSERT_EQUAL(i, PR_GetRemainingHours(prod));
	for (i = 0; i < n; i++) {
		PR_ProductionRun();
	}

	CU_ASSERT_EQUAL(old, CAP_GetCurrent(base, CAP_AIRCRAFT_SMALL));
	PR_ProductionRun();
	CU_ASSERT_EQUAL(old + 1, CAP_GetCurrent(base, CAP_AIRCRAFT_SMALL));

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testDisassembly (void)
{
	const vec2_t pos = {0, 0};
	base_t *base;
	const aircraft_t *ufo;
	int old;
	int i, n;
	storedUFO_t *storedUFO;
	productionData_t data;
	installation_t *installation;
	production_t *prod;

	ResetCampaignData();

	base = CreateBase("unittestproduction", pos);

	CU_ASSERT_TRUE(B_AtLeastOneExists());
	CU_ASSERT_TRUE(B_GetBuildingStatus(base, B_WORKSHOP));
	CU_ASSERT_TRUE(E_CountHired(base, EMPL_WORKER) > 0);
	CU_ASSERT_TRUE(PR_ProductionAllowed(base));

	ufo = AIR_GetAircraft("craft_ufo_fighter");
	CU_ASSERT_PTR_NOT_NULL_FATAL(ufo);

	installation = CreateInstallation("unittestproduction", pos);

	storedUFO = US_StoreUFO(ufo, installation, ccs.date, 1.0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(storedUFO);
	CU_ASSERT_EQUAL(storedUFO->status, SUFO_RECOVERED);
	PR_SetData(&data, PRODUCTION_TYPE_DISASSEMBLY, storedUFO);
	prod = PR_QueueNew(base, &data, 1);
	CU_ASSERT_PTR_NOT_NULL(prod);

	old = CAP_GetCurrent(base, CAP_ITEMS);
	n = PR_GetRemainingMinutes(prod);
	i = storedUFO->comp->time;
	CU_ASSERT_EQUAL(i, PR_GetRemainingHours(prod));
	for (i = 0; i < n; i++) {
		PR_ProductionRun();
	}

	CU_ASSERT_EQUAL(old, CAP_GetCurrent(base, CAP_ITEMS));
	PR_ProductionRun();
	CU_ASSERT_NOT_EQUAL(old, CAP_GetCurrent(base, CAP_ITEMS));

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testMap (void)
{
	vec2_t pos;
	qboolean coast = qfalse;

	ResetCampaignData();

	Vector2Set(pos, -51, 0);
	CU_ASSERT_TRUE(MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN, NULL)));

	Vector2Set(pos, 51, 0);
	CU_ASSERT_TRUE(!MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN, NULL)));

	Vector2Set(pos, 20, 20);
	CU_ASSERT_TRUE(MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN, NULL)));

	Vector2Set(pos, -45, 2.5);
	CU_ASSERT_TRUE(!MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN, &coast)));
	CU_ASSERT_TRUE(coast);
}

static void testAirFight (void)
{
	const vec2_t destination = { 10, 10 };
	mission_t *mission;
	aircraft_t *aircraft;
	aircraft_t *ufo;
	base_t *base;
	int i, cnt;
	/* just some random delta time value that is high enough
	 * to ensure that all the weapons are reloaded */
	const int deltaTime = 1000;
	campaign_t *campaign;

	ResetCampaignData();

	CP_InitOverlay();

	campaign = GetCampaign();

	base = CreateBase("unittestairfight", destination);
	CU_ASSERT_PTR_NOT_NULL_FATAL(base);

	cnt = AIR_BaseCountAircraft(base);
	i = 0;
	AIR_ForeachFromBase(a, base)
		i++;
	CU_ASSERT_EQUAL(i, cnt);

	aircraft = AIR_GetFirstFromBase(base);
	CU_ASSERT_PTR_NOT_NULL_FATAL(aircraft);
	aircraft->status = AIR_IDLE;
	CU_ASSERT_TRUE(AIR_IsAircraftOnGeoscape(aircraft));

	/* prepare the mission */
	mission = CP_CreateNewMission(INTERESTCATEGORY_INTERCEPT, qtrue);
	CU_ASSERT_PTR_NOT_NULL_FATAL(mission);
	CU_ASSERT_EQUAL(mission->stage, STAGE_NOT_ACTIVE);
	CP_InterceptNextStage(mission);
	CU_ASSERT_EQUAL(mission->stage, STAGE_COME_FROM_ORBIT);
	CP_InterceptNextStage(mission);
	CU_ASSERT_EQUAL(mission->stage, STAGE_INTERCEPT);
	ufo = mission->ufo;
	CU_ASSERT_PTR_NOT_NULL_FATAL(ufo);
	ufo->ufotype = UFO_FIGHTER;
	/* we have to update the routing data here to be sure that the ufo is
	 * not spawned on the other side of the globe */
	Vector2Copy(destination, ufo->pos);
	UFO_SendToDestination(ufo, destination);
	CU_ASSERT_TRUE(VectorEqual(ufo->pos, base->pos));
	CU_ASSERT_TRUE(VectorEqual(ufo->pos, aircraft->pos));

	CU_ASSERT_TRUE(aircraft->maxWeapons > 0);
	for (i = 0; i < aircraft->maxWeapons; i++)
		CU_ASSERT_TRUE(aircraft->weapons[i].delayNextShot == 0);

	/* search a target */
	UFO_CampaignRunUFOs(campaign, deltaTime);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ufo->aircraftTarget);

	/* ensure that one hit will destroy the craft */
	ufo->aircraftTarget->damage = 1;
	srand(1);
	UFO_CheckShootBack(campaign, ufo, ufo->aircraftTarget);

	/* one projectile should be spawned */
	CU_ASSERT_EQUAL(ccs.numProjectiles, 1);
	AIRFIGHT_CampaignRunProjectiles(campaign, deltaTime);
	/* don't use mission pointer from here it might been removed */

	/* target is destroyed */
	CU_ASSERT_PTR_NULL(ufo->aircraftTarget);

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

	NAT_HandleBudget(campaign);
	/** @todo implement a check here */
}

static void testMarket (void)
{
	campaign_t *campaign;

	ResetCampaignData();

	campaign = GetCampaign();

	RS_InitTree(campaign, qfalse);

	BS_InitMarket(campaign);

	CP_CampaignRunMarket(campaign);
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
	int i;
	int startDay;
	campaign_t *campaign;
	base_t *base;
	const vec2_t destination = { 10, 10 };
	const int days = 10;
	const int seconds = days * SECONDS_PER_DAY;

	ResetCampaignData();

	base = CreateBase("unittestcampaignrun", destination);

	campaign = GetCampaign();

	RS_InitTree(campaign, qfalse);

	BS_InitMarket(campaign);

	startDay = ccs.date.day;
	for (i = 0; i < seconds; i++) {
		ccs.gameTimeScale = 1;
		CP_CampaignRun(campaign, 1);
	}
	CU_ASSERT_EQUAL(ccs.date.day - startDay, days);

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testLoad (void)
{
	int i;
	aircraft_t *ufo;
	const char *error;

	ResetCampaignData();

	CP_InitOverlay();

	ccs.curCampaign = NULL;
	CU_ASSERT_TRUE(SAV_GameLoad("unittest1", &error));
	CU_ASSERT_PTR_NOT_NULL(ccs.curCampaign);

	i = 0;
	ufo = NULL;
	while ((ufo = UFO_GetNextOnGeoscape(ufo)) != NULL)
		i++;

	/* there should be one ufo on the geoscape */
	CU_ASSERT_EQUAL(i, 1);
}

static void testDateHandling (void)
{
	date_t date;
	date.day = 300;
	date.sec = 300;

	ccs.date = date;

	CU_ASSERT_TRUE(Date_IsDue(&date));
	CU_ASSERT_FALSE(Date_LaterThan(&ccs.date, &date));

	date.day = 299;
	date.sec = 310;

	CU_ASSERT_TRUE(Date_IsDue(&date));
	CU_ASSERT_TRUE(Date_LaterThan(&ccs.date, &date));

	date.day = 301;
	date.sec = 0;

	CU_ASSERT_FALSE(Date_IsDue(&date));
	CU_ASSERT_FALSE(Date_LaterThan(&ccs.date, &date));

	date.day = 300;
	date.sec = 299;

	CU_ASSERT_TRUE(Date_IsDue(&date));
	CU_ASSERT_TRUE(Date_LaterThan(&ccs.date, &date));

	date.day = 300;
	date.sec = 301;

	CU_ASSERT_FALSE(Date_IsDue(&date));
	CU_ASSERT_FALSE(Date_LaterThan(&ccs.date, &date));
}

/**
 * see bug #3166234
 */
static void testCampaignDateHandling (void)
{
	campaign_t *campaign;
	base_t *base;
	const vec2_t destination = { 10, 10 };

	ResetCampaignData();

	base = CreateBase("unittestcampaigntime", destination);

	campaign = GetCampaign();

	RS_InitTree(campaign, qfalse);

	BS_InitMarket(campaign);

	/* one hour till month change */
	ccs.date.day = 30;
	ccs.date.sec = 23 * 60 * 60;
	/** @todo fix magic number */
	ccs.gameLapse = 7;
	ccs.paid = qtrue;
	CP_UpdateTime();
	CP_CampaignRun(campaign, 1);
	CU_ASSERT_FALSE(ccs.paid);
	CU_ASSERT_TRUE(CP_IsTimeStopped());

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testHospital (void)
{
	base_t *base;
	const vec2_t destination = { 10, 10 };
	employeeType_t type;
	int i;

	ResetCampaignData();

	base = CreateBase("unittesthospital", destination);

	i = 0;
	/* hurt all employees */
	for (type = 0; type < MAX_EMPL; type++) {
		E_Foreach(type, employee) {
			if (!E_IsHired(employee))
				continue;
			employee->chr.HP = employee->chr.maxHP - 10;
			i++;
		}
	}
	CU_ASSERT_NOT_EQUAL(i, 0);

	HOS_HospitalRun();

	for (type = 0; type < MAX_EMPL; type++) {
		E_Foreach(type, employee) {
			if (!E_IsHired(employee))
				continue;
			if (employee->chr.HP == employee->chr.maxHP - 10)
				UFO_CU_FAIL_MSG_FATAL(va("%i/%i", employee->chr.HP, employee->chr.maxHP));
		}
	}

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testBuildingConstruction (void)
{
	const vec2_t pos = {0, 0};
	int x, y;
	base_t *base;
	const building_t *buildingTemplate;
	building_t *entrance, *building1, *building2;

	ResetCampaignData();

	/* day 0 has special meaning! */
	/* if building->startTime is 0 no buildTime checks done! */
	ccs.date.day++;
	base = CreateBase("unittestbuildingconstruction1", pos);
	base = CreateBase("unittestbuildingconstruction2", pos);
	CU_ASSERT_PTR_NOT_NULL_FATAL(base);

	/* base should have exactly one building: entrance */
	CU_ASSERT_EQUAL(ccs.numBuildings[base->idx], 1);
	entrance = &ccs.buildings[base->idx][0];

	/* try to build powerplant that is not connected */
	buildingTemplate = B_GetBuildingTemplate("building_powerplant");
	CU_ASSERT_PTR_NOT_NULL_FATAL(buildingTemplate);

	/* select position */
	x = entrance->pos[0];
	y = entrance->pos[1];
	CU_ASSERT_PTR_EQUAL(entrance, base->map[y][x].building);
	if (x >= 2)
		x -= 2;
	else
		x += 2;
	/* reset blocked status if set */
	base->map[y][x].blocked = qfalse;
	/* try to build (should fail) */
	building1 = B_SetBuildingByClick(base, buildingTemplate, y, x);
	CU_ASSERT_PTR_NULL_FATAL(building1);

	/* next to the entrance it should succeed */
	x = (x + entrance->pos[0]) /2;
	base->map[y][x].blocked = qfalse;
	building1 = B_SetBuildingByClick(base, buildingTemplate, y, x);
	CU_ASSERT_PTR_NOT_NULL_FATAL(building1);

	/* try to build to first pos again (still fail, conecting building not ready) */
	if (x < entrance->pos[0])
		x--;
	else
		x++;
	building2 = B_SetBuildingByClick(base, buildingTemplate, y, x);
	CU_ASSERT_PTR_NULL_FATAL(building2);
	/* roll time one day before building finishes */
	ccs.date.day += building1->buildTime - 1;
	B_UpdateBaseData();
	/* building should be under construction */
	CU_ASSERT_EQUAL(building1->buildingStatus, B_STATUS_UNDER_CONSTRUCTION);
	/* step a day */
	ccs.date.day++;
	B_UpdateBaseData();
	/* building should be ready */
	CU_ASSERT_EQUAL(building1->buildingStatus, B_STATUS_WORKING);

	/* try build other building (now it should succeed) */
	building2 = B_SetBuildingByClick(base, buildingTemplate, y, x);
	CU_ASSERT_PTR_NOT_NULL_FATAL(building2);

	/* try to destroy the second (should success) */
	CU_ASSERT_TRUE(B_BuildingDestroy(building2));
	/* rebuild */
	building2 = B_SetBuildingByClick(base, buildingTemplate, y, x);
	CU_ASSERT_PTR_NOT_NULL_FATAL(building2);

	/* try to destroy the first (should fail) */
	CU_ASSERT_FALSE(B_BuildingDestroy(building1));
	/* build up the second */
	ccs.date.day += building2->buildTime;
	B_UpdateBaseData();
	/* try to destroy the first (should fail) */
	CU_ASSERT_FALSE(B_BuildingDestroy(building1));
	/* try to destroy the second (should succeess) */
	CU_ASSERT_TRUE(B_BuildingDestroy(building2));
	/* try to destroy the first (should succeed) */
	CU_ASSERT_TRUE(B_BuildingDestroy(building1));

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

/* https://sourceforge.net/tracker/index.php?func=detail&aid=3090011&group_id=157793&atid=805242 */
static void test3090011 (void)
{
	const char *error = NULL;
	qboolean success;

	ResetCampaignData();

	CP_InitOverlay();

	success = SAV_GameLoad("3090011", &error);
	UFO_CU_ASSERT_TRUE_MSG(success, error);
}

static qboolean skipTest (const mapDef_t *md)
{
	const char *map = md->id;
	return Q_streq(map, "baseattack") || Q_streq(map, "rescue") || Q_streq(map, "alienbase");
}

static void testTerrorMissions (void)
{
	ufoType_t ufoType;
	int numUfoTypes;
	ufoType_t ufoTypes[UFO_MAX];
	mapDef_t *md;

	ResetCampaignData();

	/* Set overall interest level so every UFO can be used for missions */
	for (ufoType = UFO_SCOUT; ufoType < UFO_MAX; ufoType++) {
		const aircraft_t *ufo = UFO_GetByType(ufoType);

		if (!ufo)
			continue;

		ccs.overallInterest = max(ccs.overallInterest, ufo->ufoInterestOnGeoscape);
	}

	/* Search without UFO */
	LIST_Foreach(ccs.cities, city_t, city) {
		mission_t mission;
		OBJZERO(mission);
		UFO_CU_ASSERT_TRUE_MSG(CP_ChooseMap(&mission, city->pos), va("could not find a map for city %s", city->id));
	}

	/* Search with UFOs available for Terror missions */
	numUfoTypes = CP_TerrorMissionAvailableUFOs(NULL, ufoTypes);
	for (ufoType = 0; ufoType < numUfoTypes; ufoType++) {
		mission_t mission;
		aircraft_t *ufo = UFO_AddToGeoscape(ufoTypes[ufoType], NULL, &mission);

		OBJZERO(mission);
		mission.ufo = ufo;
		CU_ASSERT_PTR_NOT_NULL_FATAL(ufo);
		ufo->mission = &mission;

		LIST_Foreach(ccs.cities, city_t, city) {
			mission.mapDef = NULL;
#ifdef TEST_BIGUFOS
			UFO_CU_ASSERT_TRUE_MSG(CP_ChooseMap(&mission, city->pos), va("could not find map for city %s with ufo: %s", city->id, ufo->id));
#else
			CP_ChooseMap(&mission, city->pos);
#endif
		}

		UFO_RemoveFromGeoscape(ufo);
	}

	MapDef_ForeachSingleplayerCampaign(md) {
		/* skip mapDefs that were used */
		if (md->timesAlreadyUsed > 0)
			continue;
		/* skip special mapDefs */
		if (skipTest(md))
			continue;
		/* skip mapDefs which don't support UFO types that do terror missions */
		if (!LIST_IsEmpty(md->ufos)) {
			qboolean found = qfalse;
			for (ufoType = 0; ufoType < numUfoTypes; ufoType++) {
				const aircraft_t *ufo = UFO_GetByType(ufoTypes[ufoType]);

				if (!ufo)
					continue;

				if (LIST_ContainsString(md->ufos, ufo->id)) {
					found = qtrue;
					break;
				}
			}
			if (!found)
				continue;
		}
		UFO_CU_ASSERT_MSG(va("%s wasn't used", md->id));
	}
}

static void testRandomPosMissions (void)
{
	const mapDef_t *md;

	ResetCampaignData();

	MapDef_ForeachSingleplayerCampaign(md) {
		if (!skipTest(md)) {
			mission_t mission;
			qboolean result;
			OBJZERO(mission);
			result = CP_GetRandomPosOnGeoscapeWithParameters(mission.pos, md->terrains, md->cultures, md->populations, NULL);
			UFO_CU_ASSERT_TRUE_MSG(result, va("could not find a mission for mapdef %s", md->id));
		}
	}
}

int UFO_AddCampaignTests (void)
{
	/* add a suite to the registry */
	CU_pSuite campaignSuite = CU_add_suite("CampaignTests", UFO_InitSuiteCampaign, UFO_CleanSuiteCampaign);

	if (campaignSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(campaignSuite, testBaseBuilding) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testBuildingConstruction) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testAircraftHandling) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testEmployeeHandling) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testAutoMissions) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testTransferItem) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testTransferAircraft) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testTransferEmployee) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testUFORecovery) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testResearch) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testProductionItem) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testProductionAircraft) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testDisassembly) == NULL)
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

	if (CU_ADD_TEST(campaignSuite, testDateHandling) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testCampaignDateHandling) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testHospital) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, test3090011) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testTerrorMissions) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testRandomPosMissions) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
