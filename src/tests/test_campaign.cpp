/**
 * @file
 * @brief Test cases for the campaign code
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "../client/client.h"
#include "../client/cgame/cl_game.h"
#include "../client/renderer/r_state.h" /* r_state */
#include "../client/ui/ui_main.h"
#include "../client/cgame/campaign/cp_campaign.h"
#include "../client/cgame/campaign/cp_geoscape.h"
#include "../client/cgame/campaign/cp_hospital.h"
#include "../client/cgame/campaign/cp_missions.h"
#include "../client/cgame/campaign/cp_nation.h"
#include "../client/cgame/campaign/cp_overlay.h"
#include "../client/cgame/campaign/cp_ufo.h"
#include "../client/cgame/campaign/cp_time.h"
#include "../client/cgame/campaign/cp_alien_interest.h"
#include "../client/cgame/campaign/cp_auto_mission.h"
#include "../shared/parse.h"
#include "../shared/images.h"

static const int TAG_INVENTORY = 1538;

static void FreeInventory (void* data)
{
	Mem_Free(data);
}

static void* AllocInventoryMemory (size_t size)
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
	cls.i.destroyInventoryInterface();
	cls.i.initInventory("testCampaign", &csi, &inventoryImport);
}

static campaign_t* GetCampaign (void)
{
	return CP_GetCampaign(cp_campaign->string);
}

class CampaignTest: public ::testing::Test {
protected:
	static void SetUpTestCase()
	{
		TEST_Init();

		cl_genericPool = Mem_CreatePool("Client: Generic");
		cp_campaignPool = Mem_CreatePool("Client: Local (per game)");
		cp_campaign = Cvar_Get("cp_campaign", "main");
		cp_missiontest = Cvar_Get("cp_missiontest", "0");
		vid_imagePool = Mem_CreatePool("Vid: Image system");

		r_state.active_texunit = &r_state.texunits[0];
		R_FontInit();
		UI_Init();
		GAME_InitStartup();

		OBJZERO(cls);
		Com_ParseScripts(false);

		Cmd_ExecuteString("game_setmode campaign");

		Cmd_AddCommand("msgoptions_set", Cmd_Dummy_f);

		CL_SetClientState(ca_disconnected);
		cls.realtime = Sys_Milliseconds();
	}

	static void TearDownTestCase()
	{
		CP_ResetCampaignData();
		TEST_Shutdown();
	}

	void SetUp()
	{
		campaign_t* campaign;

		CP_ResetCampaignData();
		CP_ParseCampaignData();
		campaign = GetCampaign();
		CP_ReadCampaignData(campaign);

		ResetInventoryList();

		CP_UpdateCredits(MAX_CREDITS);

		GEO_Shutdown();
		GEO_Init(campaign->map);

		ccs.curCampaign = campaign;
	}
};

static installation_t* CreateInstallation (const char* name, const vec2_t pos)
{
	const installationTemplate_t* installationTemplate = INS_GetInstallationTemplateByType(INSTALLATION_UFOYARD);
	installation_t* installation = INS_Build(installationTemplate, pos, name);

	/* fake the build time */
	installation->buildStart = ccs.date.day - installation->installationTemplate->buildTime;
	INS_UpdateInstallationData();

	return installation;
}

static base_t* CreateBase (const char* name, const vec2_t pos)
{
	const campaign_t* campaign = GetCampaign();
	base_t* base;

	RS_InitTree(campaign, false);
	E_InitialEmployees(campaign);
	base = B_Build(campaign, pos, name);

	/* First base */
	if (ccs.campaignStats.basesBuilt == 1)
		B_SetUpFirstBase(campaign, base);

	return base;
}

TEST_F(CampaignTest, testAircraftHandling)
{
	const vec2_t destination = { 10, 10 };
	base_t* base;
	aircraft_t* aircraft;
	aircraft_t* newAircraft;
	aircraft_t* aircraftTemplate;
	int firstIdx;
	int initialCount;
	int count;
	int newFound;

	base = CreateBase("unittestaircraft", destination);
	ASSERT_TRUE(nullptr != base);

	/** @todo we should not assume that initial base has aircraft. It's a campaign parameter */
	aircraft = AIR_GetFirstFromBase(base);
	ASSERT_TRUE(nullptr != aircraft);

	/* aircraft should have a template */
	aircraftTemplate = aircraft->tpl;
	ASSERT_TRUE(nullptr != aircraftTemplate);

	firstIdx = aircraft->idx;
	initialCount = AIR_BaseCountAircraft(base);

	/* test deletion (part 1) */
	AIR_DeleteAircraft(aircraft);
	count = AIR_BaseCountAircraft(base);
	ASSERT_EQ(count, initialCount - 1);

	/* test addition (part 1) */
	newAircraft = AIR_NewAircraft(base, aircraftTemplate);
	ASSERT_TRUE(nullptr != newAircraft);
	count = AIR_BaseCountAircraft(base);
	ASSERT_EQ(count, initialCount);

	/* new aircraft assigned to the right base */
	ASSERT_EQ(newAircraft->homebase, base);

	newFound = 0;
	AIR_Foreach(a) {
		/* test deletion (part 2) */
		ASSERT_NE(firstIdx, a->idx);
		/* for test addition (part 2) */
		if (a->idx == newAircraft->idx)
			newFound++;
	}
	/* test addition (part 2) */
	ASSERT_EQ(newFound, 1);

	/* check if AIR_Foreach iterates through all aircraft */
	AIR_Foreach(a) {
		AIR_DeleteAircraft(a);
	}
	aircraft = AIR_GetFirstFromBase(base);
	ASSERT_TRUE(nullptr == aircraft);
	count = AIR_BaseCountAircraft(base);
	ASSERT_EQ(count, 0);

	/* cleanup for the following tests */
	E_DeleteAllEmployees(nullptr);

	base->founded = false;
}

TEST_F(CampaignTest, testEmployeeHandling)
{
	int i;

	for (i = 0; i < MAX_EMPL; i++) {
		employeeType_t type = (employeeType_t)i;
		if (type != EMPL_ROBOT) {
			int cnt;
			Employee* e = E_CreateEmployee(type, nullptr, nullptr);
			ASSERT_TRUE(nullptr != e);

			cnt = E_CountUnhired(type);
			ASSERT_EQ(cnt, 1);

			E_DeleteEmployee(e);

			cnt = E_CountUnhired(type);
			ASSERT_EQ(cnt, 0);
		}
	}

	{
		const int amount = 3;
		for (i = 0; i < amount; i++) {
			Employee* e = E_CreateEmployee(EMPL_SOLDIER, nullptr, nullptr);
			ASSERT_TRUE(nullptr != e);
		}
		{
			int cnt = 0;
			E_Foreach(EMPL_SOLDIER, e) {
				(void)e;
				cnt++;
			}

			ASSERT_EQ(cnt, amount);

			E_Foreach(EMPL_SOLDIER, e) {
				ASSERT_TRUE(E_DeleteEmployee(e));
			}

			cnt = E_CountUnhired(EMPL_SOLDIER);
			ASSERT_EQ(cnt, 0);
		}
	}

	{
		Employee* e = E_CreateEmployee(EMPL_PILOT, nullptr, nullptr);
		int cnt;
		ASSERT_TRUE(nullptr != e);
		cnt = E_RefreshUnhiredEmployeeGlobalList(EMPL_PILOT, false);
		ASSERT_EQ(cnt, 1);
		e = E_GetUnhired(EMPL_PILOT);
		ASSERT_TRUE(nullptr != e);
		ASSERT_TRUE(E_DeleteEmployee(e));
	}

	{
		const ugv_t* ugvType = Com_GetUGVByID("ugv_ares_w");
		Employee* e = E_CreateEmployee(EMPL_ROBOT, nullptr, ugvType);
		ASSERT_TRUE(nullptr != e);
		ASSERT_TRUE(E_DeleteEmployee(e));
	}

	{
		int i, cnt;
		for (i = 0; i < 512; i++) {
			Employee* e = E_CreateEmployee(EMPL_SOLDIER, nullptr, nullptr);
			ASSERT_TRUE(nullptr != e);

			cnt = E_CountUnhired(EMPL_SOLDIER);
			ASSERT_EQ(cnt, i + 1);
		}
		E_DeleteAllEmployees(nullptr);

		cnt = E_CountUnhired(EMPL_SOLDIER);
		ASSERT_EQ(cnt, 0);
	}
}

TEST_F(CampaignTest, testBaseBuilding)
{
	ccs.credits = 10000000;

	vec2_t pos = {0, 0};
	base_t* base = CreateBase("unittestcreatebase", pos);

	ASSERT_EQ(B_GetInstallationLimit(), MAX_INSTALLATIONS_PER_BASE);

	B_Destroy(base);

	for (int i = 0; i < MAX_EMPL; i++) {
		employeeType_t type = (employeeType_t)i;
		ASSERT_EQ(E_CountHired(base, type), 0);
	}

	/* cleanup for the following tests */
	E_DeleteAllEmployees(nullptr);

	base->founded = false;
}

TEST_F(CampaignTest, testAutoMissions)
{
	const vec2_t pos = {-73.2, 18.5};
	base_t* base;
	missionResults_t result;
	battleParam_t battleParameters;
	aircraft_t* aircraft;
	mission_t* mission;
	campaign_t* campaign;

	campaign = GetCampaign();
	ASSERT_TRUE(campaign != nullptr);

	OBJZERO(result);
	OBJZERO(battleParameters);

	ccs.overallInterest = 36;
	INT_ResetAlienInterest();

	base = CreateBase("unittestautomission", pos);
	ASSERT_TRUE(nullptr != base);

	aircraft = nullptr;
	AIR_ForeachFromBase(a, base) {
		if (AIR_GetTeamSize(a) > 0) {
			aircraft = a;
			break;
		}
	}
	ASSERT_TRUE(nullptr != aircraft);
	ASSERT_TRUE(AIR_GetTeamSize(aircraft) > 0);

	mission = CP_CreateNewMission(INTERESTCATEGORY_RECON, false);
	Vector2Copy(pos, mission->pos);
	mission->posAssigned = true;
	mission->mapDef = Com_GetMapDefinitionByID("farm2");
	ASSERT_TRUE(nullptr != mission);

	CP_CreateBattleParameters(mission, &battleParameters, aircraft);
	AM_Go(mission, aircraft, campaign, &battleParameters, &result);

	ASSERT_TRUE(result.state == WON);
}

TEST_F(CampaignTest, testTransferItem)
{
	const campaign_t* campaign = GetCampaign();
	const vec2_t pos = {0, 0};
	const vec2_t posTarget = {51, 0};
	base_t* base, *targetBase;
	transfer_t tr;
	const objDef_t* od;
	transfer_t* transfer;

	base = CreateBase("unittesttransferitem", pos);
	ASSERT_TRUE(nullptr != base);
	/* make sure that we get all buildings in our second base, too.
	 * This is needed for starting a transfer */
	targetBase = CreateBase("unittesttransferitemtargetbase", posTarget);
	ASSERT_TRUE(nullptr != targetBase);
	B_SetUpFirstBase(campaign, targetBase);

	od = INVSH_GetItemByID("assault");
	ASSERT_TRUE(nullptr != od);

	OBJZERO(tr);
	tr.itemAmount[od->idx] += 1;
	tr.destBase = targetBase;
	tr.itemAmount[od->idx]++;

	transfer = TR_TransferStart(base, tr);
	ASSERT_TRUE(nullptr != transfer);

	ASSERT_EQ(LIST_Count(ccs.transfers), 1);

	transfer->event = ccs.date;
	transfer->event.day++;

	/* check if it's arrived immediately */
	TR_TransferRun();
	ASSERT_FALSE(LIST_IsEmpty(ccs.transfers));

	/* check if it arrives (even a second) before it should */
	ccs.date.day++;
	ccs.date.sec--;
	TR_TransferRun();
	ASSERT_FALSE(LIST_IsEmpty(ccs.transfers));

	/* check if it arrived right when it should */
	ccs.date.sec++;
	TR_TransferRun();
	ASSERT_TRUE(LIST_IsEmpty(ccs.transfers));

	/* Start another transfer to check higher time lapse */
	transfer = TR_TransferStart(base, tr);
	ASSERT_TRUE(nullptr != transfer);
	ASSERT_EQ(LIST_Count(ccs.transfers), 1);

	transfer->event = ccs.date;
	transfer->event.day++;

	/* check if it arrived when time passed the deadline by days already */
	ccs.date.day += 2;
	TR_TransferRun();
	ASSERT_TRUE(LIST_IsEmpty(ccs.transfers));

	/* Start another transfer to check higher time lapse */
	transfer = TR_TransferStart(base, tr);
	ASSERT_TRUE(nullptr != transfer);
	ASSERT_EQ(LIST_Count(ccs.transfers), 1);

	transfer->event = ccs.date;
	transfer->event.day++;

	/* check if it arrived when time passed the deadline by days already */
	ccs.date.day++;
	ccs.date.sec++;
	TR_TransferRun();
	ASSERT_TRUE(LIST_IsEmpty(ccs.transfers));


	/* cleanup for the following tests */
	E_DeleteAllEmployees(nullptr);

	base->founded = false;
}

TEST_F(CampaignTest, testUFORecovery)
{
	const vec2_t pos = {0, 0};
	const aircraft_t* ufo;
	storedUFO_t* storedUFO;
	installation_t* installation;
	date_t date = ccs.date;

	ufo = AIR_GetAircraft("craft_ufo_fighter");
	ASSERT_TRUE(nullptr != ufo);

	CreateBase("unittestproduction", pos);

	installation = CreateInstallation("unittestuforecovery", pos);

	date.day++;
	storedUFO = US_StoreUFO(ufo, installation, date, 1.0);
	ASSERT_TRUE(nullptr != storedUFO);
	ASSERT_EQ(storedUFO->status, SUFO_RECOVERED);

	UR_ProcessActive();

	ASSERT_EQ(storedUFO->status, SUFO_RECOVERED);

	ccs.date.day++;

	UR_ProcessActive();

	ASSERT_EQ(storedUFO->status, SUFO_STORED);

	/* cleanup for the following tests */
	E_DeleteAllEmployees(nullptr);
}

TEST_F(CampaignTest, testAlienPSIDevice)
{
	RS_MarkResearchable(nullptr, true);

	technology_t* alienPsiDevice = RS_GetTechByID("rs_alien_psi_device");
	RS_MarkOneResearchable(alienPsiDevice);
	ASSERT_TRUE(alienPsiDevice->statusResearchable);
}

TEST_F(CampaignTest, testResearch)
{
	RS_MarkResearchable(nullptr, true);

	technology_t* laserTech = RS_GetTechByID("rs_laser");
	ASSERT_TRUE(nullptr != laserTech);
	technology_t* otherLaserTech = RS_GetTechByID("rs_baselaser");
	ASSERT_TRUE(nullptr != otherLaserTech);

	const vec2_t pos = {0, 0};
	base_t* base = CreateBase("unittestbase", pos);

	ASSERT_TRUE(laserTech->statusResearchable);

	RS_AssignScientist(laserTech, base);

	ASSERT_EQ(laserTech->base, base);
	ASSERT_EQ(laserTech->scientists, 1);
	ASSERT_EQ(laserTech->statusResearch, RS_RUNNING);

	ASSERT_FALSE(otherLaserTech->statusResearchable);

	const int n = laserTech->time * (1.0f / ccs.curCampaign->researchRate);
	for (int i = 0; i < n; i++) {
		const int finished = RS_ResearchRun();
		ASSERT_EQ(finished, 0) << "Did not expect to finish a research (#" << finished << ", i:" << i << ")";
	}

	ASSERT_EQ(laserTech->statusResearch, RS_RUNNING);
	ASSERT_EQ(RS_ResearchRun(), 1);
	ASSERT_EQ(laserTech->statusResearch, RS_FINISH);

	ASSERT_TRUE(otherLaserTech->statusResearchable);

	/* cleanup for the following tests */
	E_DeleteAllEmployees(nullptr);

	base->founded = false;
}

TEST_F(CampaignTest, testProductionItem)
{
	const vec2_t pos = {0, 0};
	base_t* base;
	const objDef_t* od;
	const technology_t* tech;
	int old;
	int i, n;
	productionData_t data;
	production_t* prod;

	base = CreateBase("unittestproduction", pos);

	ASSERT_TRUE(B_AtLeastOneExists());
	ASSERT_TRUE(B_GetBuildingStatus(base, B_WORKSHOP));
	ASSERT_TRUE(E_CountHired(base, EMPL_WORKER) > 0);
	ASSERT_TRUE(PR_ProductionAllowed(base));

	od = INVSH_GetItemByID("assault");
	ASSERT_TRUE(nullptr != od);

	PR_SetData(&data, PRODUCTION_TYPE_ITEM, od);
	old = base->storage.numItems[od->idx];
	prod = PR_QueueNew(base, &data, 1);
	ASSERT_TRUE(nullptr != prod);
	tech = RS_GetTechForItem(od);
	n = PR_GetRemainingMinutes(prod);
	i = tech->produceTime;
	ASSERT_EQ(i, PR_GetRemainingHours(prod));
	for (i = 0; i < n; i++) {
		PR_ProductionRun();
	}

	ASSERT_EQ(old, base->storage.numItems[od->idx]);
	PR_ProductionRun();
	ASSERT_EQ(old + 1, base->storage.numItems[od->idx]);

	/* cleanup for the following tests */
	E_DeleteAllEmployees(nullptr);

	base->founded = false;
}

TEST_F(CampaignTest, testProductionAircraft)
{
	const vec2_t pos = {0, 0};
	base_t* base;
	const aircraft_t* aircraft;
	int old;
	int i, n;
	productionData_t data;
	production_t* prod;

	base = CreateBase("unittestproduction", pos);

	ASSERT_TRUE(B_AtLeastOneExists());
	ASSERT_TRUE(B_GetBuildingStatus(base, B_WORKSHOP));
	ASSERT_TRUE(E_CountHired(base, EMPL_WORKER) > 0);
	ASSERT_TRUE(PR_ProductionAllowed(base));

	/* Check for production requirements */
	aircraft = AIR_GetAircraft("craft_inter_stingray");
	ASSERT_TRUE(nullptr != aircraft);
	PR_SetData(&data, PRODUCTION_TYPE_AIRCRAFT, aircraft);
	/* no antimatter */
	ASSERT_TRUE(nullptr == PR_QueueNew(base, &data, 1));

	old = CAP_GetCurrent(base, CAP_AIRCRAFT_SMALL);
	aircraft = AIR_GetAircraft("craft_inter_stiletto");
	ASSERT_TRUE(nullptr != aircraft);
	PR_SetData(&data, PRODUCTION_TYPE_AIRCRAFT, aircraft);
	prod = PR_QueueNew(base, &data, 1);
	ASSERT_TRUE(nullptr != prod);

	n = PR_GetRemainingMinutes(prod);
	i = aircraft->tech->produceTime;
	ASSERT_EQ(i, PR_GetRemainingHours(prod));
	for (i = 0; i < n; i++) {
		PR_ProductionRun();
	}

	ASSERT_EQ(old, CAP_GetCurrent(base, CAP_AIRCRAFT_SMALL));
	PR_ProductionRun();
	ASSERT_EQ(old + 1, CAP_GetCurrent(base, CAP_AIRCRAFT_SMALL));

	/* cleanup for the following tests */
	E_DeleteAllEmployees(nullptr);

	base->founded = false;
}

TEST_F(CampaignTest, testDisassembly)
{
	const vec2_t pos = {0, 0};
	base_t* base;
	const aircraft_t* ufo;
	int old;
	int i, n;
	storedUFO_t* storedUFO;
	productionData_t data;
	installation_t* installation;
	production_t* prod;

	base = CreateBase("unittestproduction", pos);

	ASSERT_TRUE(B_AtLeastOneExists());
	ASSERT_TRUE(B_GetBuildingStatus(base, B_WORKSHOP));
	ASSERT_TRUE(E_CountHired(base, EMPL_WORKER) > 0);
	ASSERT_TRUE(PR_ProductionAllowed(base));

	ufo = AIR_GetAircraft("craft_ufo_fighter");
	ASSERT_TRUE(nullptr != ufo);

	installation = CreateInstallation("unittestproduction", pos);

	storedUFO = US_StoreUFO(ufo, installation, ccs.date, 1.0);
	ASSERT_TRUE(nullptr != storedUFO);
	ASSERT_EQ(storedUFO->status, SUFO_RECOVERED);
	PR_SetData(&data, PRODUCTION_TYPE_DISASSEMBLY, storedUFO);
	prod = PR_QueueNew(base, &data, 1);
	ASSERT_TRUE(nullptr != prod);

	old = CAP_GetCurrent(base, CAP_ITEMS);
	n = PR_GetRemainingMinutes(prod);
	i = storedUFO->comp->time;
	ASSERT_EQ(i, PR_GetRemainingHours(prod));
	for (i = 0; i < n; i++) {
		PR_ProductionRun();
	}

	ASSERT_EQ(old, CAP_GetCurrent(base, CAP_ITEMS));
	PR_ProductionRun();
	ASSERT_NE(old, CAP_GetCurrent(base, CAP_ITEMS));

	/* cleanup for the following tests */
	E_DeleteAllEmployees(nullptr);

	base->founded = false;
}

TEST_F(CampaignTest, testMap)
{
	vec2_t pos;
	bool coast = false;

	Vector2Set(pos, -51, 0);
	ASSERT_TRUE(MapIsWater(GEO_GetColor(pos, MAPTYPE_TERRAIN, nullptr)));

	Vector2Set(pos, 51, 0);
	ASSERT_TRUE(!MapIsWater(GEO_GetColor(pos, MAPTYPE_TERRAIN, nullptr)));

	Vector2Set(pos, 20, 20);
	ASSERT_TRUE(MapIsWater(GEO_GetColor(pos, MAPTYPE_TERRAIN, nullptr)));

	Vector2Set(pos, -45, 2.5);
	ASSERT_TRUE(!MapIsWater(GEO_GetColor(pos, MAPTYPE_TERRAIN, &coast)));
	ASSERT_TRUE(coast);
}

TEST_F(CampaignTest, testAirFight)
{
	const vec2_t destination = { 10, 10 };
	/* just some random delta time value that is high enough
	 * to ensure that all the weapons are reloaded */
	const int deltaTime = 1000;

	campaign_t* campaign = GetCampaign();

	base_t* base = CreateBase("unittestairfight", destination);
	ASSERT_TRUE(nullptr != base);

	int cnt = AIR_BaseCountAircraft(base);
	int i = 0;
	AIR_ForeachFromBase(a, base)
		i++;
	ASSERT_EQ(i, cnt);

	aircraft_t* aircraft = AIR_GetFirstFromBase(base);
	ASSERT_TRUE(nullptr != aircraft);
	aircraft->status = AIR_IDLE;
	ASSERT_TRUE(AIR_IsAircraftOnGeoscape(aircraft));

	/* ensure that a FIGHTER can spawn */
	ufoType_t ufoTypes[UFO_MAX];
	ASSERT_NE(0, UFO_GetAvailableUFOsForMission(INTERESTCATEGORY_INTERCEPT, ufoTypes, false));
	const aircraft_t* ufoTemplate = UFO_GetByType(ufoTypes[0]);		/* the first interceptor will do */
	ASSERT_TRUE(nullptr != ufoTemplate);
	ccs.overallInterest = ufoTemplate->ufoInterestOnGeoscape + 1;

	/* prepare the mission */
	mission_t* mission = CP_CreateNewMission(INTERESTCATEGORY_INTERCEPT, true);
	ASSERT_TRUE(nullptr != mission);
	ASSERT_EQ(mission->stage, STAGE_NOT_ACTIVE);
	CP_InterceptNextStage(mission);
	ASSERT_EQ(mission->stage, STAGE_COME_FROM_ORBIT);
	CP_InterceptNextStage(mission);
	ASSERT_EQ(mission->stage, STAGE_INTERCEPT);
	aircraft_t* ufo = mission->ufo;
	ASSERT_TRUE(nullptr != ufo);
	ufo->ufotype = ufoTypes[0];
	/* we have to update the routing data here to be sure that the ufo is
	 * not spawned on the other side of the globe */
	Vector2Copy(destination, ufo->pos);
	UFO_SendToDestination(ufo, destination);
	ASSERT_TRUE(VectorEqual(ufo->pos, base->pos));
	ASSERT_TRUE(VectorEqual(ufo->pos, aircraft->pos));

	ASSERT_TRUE(aircraft->maxWeapons > 0);
	for (i = 0; i < aircraft->maxWeapons; i++)
		ASSERT_TRUE(aircraft->weapons[i].delayNextShot == 0);

	/* search a target */
	UFO_CampaignRunUFOs(campaign, deltaTime);
	ASSERT_TRUE(nullptr != ufo->aircraftTarget);

	/* ensure that one hit will destroy the craft */
	ufo->aircraftTarget->damage = 1;
	srand(1);
	/* don't take any pilot skill into account here */
	OBJZERO(ufo->aircraftTarget->pilot->chr.score.skills);
	UFO_CheckShootBack(campaign, ufo, ufo->aircraftTarget);

	/* one projectile should be spawned */
	ASSERT_EQ(ccs.numProjectiles, 1);
	AIRFIGHT_CampaignRunProjectiles(campaign, deltaTime);
	/* don't use mission pointer from here it might been removed */

	/* target is destroyed */
	ASSERT_TRUE(nullptr == ufo->aircraftTarget);

	/* one aircraft less */
	ASSERT_EQ(cnt - 1, AIR_BaseCountAircraft(base));

	/* cleanup for the following tests */
	E_DeleteAllEmployees(nullptr);

	base->founded = false;
}

TEST_F(CampaignTest, testRadar)
{
	const vec2_t destination = { 10, 10 };
	ufoType_t ufoTypes[UFO_MAX];
	ASSERT_NE(0, UFO_GetAvailableUFOsForMission(INTERESTCATEGORY_INTERCEPT, ufoTypes, false));

	base_t* base = CreateBase("unittestradar", destination);
	mission_t* mission = CP_CreateNewMission(INTERESTCATEGORY_INTERCEPT, true);
	aircraft_t* ufo = UFO_AddToGeoscape(ufoTypes[0], destination, mission);
	Vector2Copy(destination, ufo->pos);
	UFO_SendToDestination(ufo, destination);
	ASSERT_TRUE(VectorEqual(ufo->pos, base->pos));
	ASSERT_TRUE(VectorEqual(ufo->pos, ufo->pos));
	/* to ensure that the UFOs are really detected when they are in range */
	base->radar.ufoDetectionProbability = 1.0;
	ASSERT_TRUE(RADAR_CheckUFOSensored(&base->radar, base->pos, ufo, false));

	/* cleanup for the following tests */
	E_DeleteAllEmployees(nullptr);

	base->founded = false;
}

TEST_F(CampaignTest, testNation)
{
	const nation_t* nation;
	campaign_t* campaign;

	nation = NAT_GetNationByID("europe");
	ASSERT_TRUE(nullptr != nation);

	campaign = GetCampaign();

	NAT_HandleBudget(campaign);
	/** @todo implement a check here */
}

TEST_F(CampaignTest, testMarket)
{
	campaign_t* campaign;

	campaign = GetCampaign();

	RS_InitTree(campaign, false);

	BS_InitMarket(campaign);

	CP_CampaignRunMarket(campaign);
	/** @todo implement a check here */
}

TEST_F(CampaignTest, testSaveLoad)
{
	const vec2_t pos = {0, 0};
	base_t* base;
	campaign_t* campaign;

	campaign = GetCampaign();

	SAV_Init();

	ccs.curCampaign = campaign;

	Cvar_Set("save_compressed", "0");

	base = CreateBase("unittestbase", pos);

	{
		ASSERT_TRUE(base->founded);
		ASSERT_EQ(base->baseStatus, BASE_WORKING);

		Cmd_ExecuteString("game_quicksave");
	}
	{
		B_Destroy(base);

		ASSERT_EQ(base->baseStatus, BASE_DESTROYED);

		E_DeleteAllEmployees(nullptr);

		B_SetName(base, "unittestbase2");
	}
#if 0
	{
		ResetCampaignData();

		Cmd_ExecuteString("game_quickload");

		/** @todo check that the savegame was successfully loaded */

		ASSERT_EQ(base->baseStatus, BASE_WORKING);
		ASSERT_STREQ(base->name, "unittestbase");

		B_Destroy(base);

		E_DeleteAllEmployees(nullptr);
	}
#endif
	base->founded = false;
}

TEST_F(CampaignTest, testSaveMassEmployees)
{
	campaign_t* campaign = GetCampaign();

	SAV_Init();

	ccs.curCampaign = campaign;

	Cvar_Set("save_compressed", "0");

	const vec2_t pos = {0, 0};
	base_t* base = CreateBase("unittestmassemployees", pos);

	nation_t* nation = NAT_GetNationByID("europe");
	ASSERT_TRUE(nullptr != nation);

	const int employees = 10000;
	for (int i = 0; i < employees; i++) {
		Employee* e = E_CreateEmployee(EMPL_SOLDIER, nation);
		if (CAP_GetFreeCapacity(base, CAP_EMPLOYEES) > 0)
			ASSERT_TRUE(E_HireEmployee(base, e));
	}

	Cmd_ExecuteString("game_quicksave");

	/* cleanup for the following tests */
	E_DeleteAllEmployees(nullptr);

	base->founded = false;
}

TEST_F(CampaignTest, testLoadMassEmployees)
{
	Cmd_ExecuteString("game_quickload");

	/* cleanup for the following tests */
	E_DeleteAllEmployees(nullptr);
}

TEST_F(CampaignTest, testCampaignRun)
{
	const vec2_t destination = { 10, 10 };
	const int days = 10;
	const int seconds = days * SECONDS_PER_DAY;

	base_t* base = CreateBase("unittestcampaignrun", destination);

	campaign_t* campaign = GetCampaign();

	RS_InitTree(campaign, false);

	BS_InitMarket(campaign);

	int startDay = ccs.date.day;
	for (int i = 0; i < seconds; i++) {
		ccs.gameTimeScale = 1;
		CP_CampaignRun(campaign, 1);
	}
	ASSERT_EQ(ccs.date.day - startDay, days);

	/* cleanup for the following tests */
	E_DeleteAllEmployees(nullptr);

	base->founded = false;
}

TEST_F(CampaignTest, testLoad)
{
	int i;
	aircraft_t* ufo;
	const char* error;

	ccs.curCampaign = nullptr;
	ASSERT_TRUE(SAV_GameLoad("unittest1", &error));
	ASSERT_TRUE(nullptr != ccs.curCampaign);

	i = 0;
	ufo = nullptr;
	while ((ufo = UFO_GetNextOnGeoscape(ufo)) != nullptr)
		i++;

	/* there should be one ufo on the geoscape */
	ASSERT_EQ(i, 1);
}

TEST_F(CampaignTest, testDateHandling)
{
	date_t date;
	date.day = 300;
	date.sec = 300;

	ccs.date = date;

	ASSERT_TRUE(Date_IsDue(&date));
	ASSERT_FALSE(Date_LaterThan(&ccs.date, &date));

	date.day = 299;
	date.sec = 310;

	ASSERT_TRUE(Date_IsDue(&date));
	ASSERT_TRUE(Date_LaterThan(&ccs.date, &date));

	date.day = 301;
	date.sec = 0;

	ASSERT_FALSE(Date_IsDue(&date));
	ASSERT_FALSE(Date_LaterThan(&ccs.date, &date));

	date.day = 300;
	date.sec = 299;

	ASSERT_TRUE(Date_IsDue(&date));
	ASSERT_TRUE(Date_LaterThan(&ccs.date, &date));

	date.day = 300;
	date.sec = 301;

	ASSERT_FALSE(Date_IsDue(&date));
	ASSERT_FALSE(Date_LaterThan(&ccs.date, &date));
}

/**
 * see bug #3166234
 */
TEST_F(CampaignTest, testCampaignDateHandling)
{
	campaign_t* campaign;
	base_t* base;
	const vec2_t destination = { 10, 10 };

	base = CreateBase("unittestcampaigntime", destination);

	campaign = GetCampaign();

	RS_InitTree(campaign, false);

	BS_InitMarket(campaign);

	/* one hour till month change */
	ccs.date.day = 30;
	ccs.date.sec = 23 * 60 * 60;
	/** @todo fix magic number */
	ccs.gameLapse = 7;
	ccs.paid = true;
	CP_UpdateTime();
	CP_CampaignRun(campaign, 1);
	ASSERT_FALSE(ccs.paid);
	ASSERT_TRUE(CP_IsTimeStopped());

	/* cleanup for the following tests */
	E_DeleteAllEmployees(nullptr);

	base->founded = false;
}

TEST_F(CampaignTest, testHospital)
{
	base_t* base;
	const vec2_t destination = { 10, 10 };
	int i, n;

	base = CreateBase("unittesthospital", destination);

	n = 0;
	/* hurt all employees */
	for (i = 0; i < MAX_EMPL; i++) {
		employeeType_t type = (employeeType_t)i;
		E_Foreach(type, employee) {
			if (!employee->isHired())
				continue;
			employee->chr.HP = employee->chr.maxHP - 10;
			n++;
		}
	}
	ASSERT_NE(n, 0);

	HOS_HospitalRun();

	for (i = 0; i < MAX_EMPL; i++) {
		employeeType_t type = (employeeType_t)i;
		E_Foreach(type, employee) {
			if (!employee->isHired())
				continue;
			ASSERT_NE(employee->chr.HP, employee->chr.maxHP - 10) << employee->chr.HP << "/" << employee->chr.maxHP;
		}
	}

	/* cleanup for the following tests */
	E_DeleteAllEmployees(nullptr);

	base->founded = false;
}

TEST_F(CampaignTest, testBuildingConstruction)
{
	const vec2_t pos = {0, 0};
	int x, y;
	base_t* base;
	const building_t* buildingTemplate;
	building_t* entrance, *building1, *building2;

	/* day 0 has special meaning! */
	/* if building->startTime is 0 no buildTime checks done! */
	ccs.date.day++;
	base = CreateBase("unittestbuildingconstruction1", pos);
	ASSERT_TRUE(nullptr != base);
	base = CreateBase("unittestbuildingconstruction2", pos);
	ASSERT_TRUE(nullptr != base);

	/* base should have exactly one building: entrance */
	ASSERT_EQ(ccs.numBuildings[base->idx], 1);
	entrance = &ccs.buildings[base->idx][0];

	/* try to build powerplant that is not connected */
	buildingTemplate = B_GetBuildingTemplate("building_powerplant");
	ASSERT_TRUE(nullptr != buildingTemplate);

	/* select position */
	x = entrance->pos[0];
	y = entrance->pos[1];
	ASSERT_EQ(entrance, base->map[y][x].building);
	if (x >= 2)
		x -= 2;
	else
		x += 2;
	/* reset blocked status if set */
	base->map[y][x].blocked = false;
	/* try to build (should fail) */
	building1 = B_SetBuildingByClick(base, buildingTemplate, y, x);
	ASSERT_TRUE(nullptr == building1);

	/* next to the entrance it should succeed */
	x = (x + entrance->pos[0]) /2;
	base->map[y][x].blocked = false;
	building1 = B_SetBuildingByClick(base, buildingTemplate, y, x);
	ASSERT_TRUE(nullptr != building1);

	/* try to build to first pos again (still fail, conecting building not ready) */
	if (x < entrance->pos[0])
		x--;
	else
		x++;
	building2 = B_SetBuildingByClick(base, buildingTemplate, y, x);
	ASSERT_TRUE(nullptr == building2);
	/* roll time one day before building finishes */
	ccs.date.day += building1->buildTime - 1;
	B_UpdateBaseData();
	/* building should be under construction */
	ASSERT_EQ(building1->buildingStatus, B_STATUS_UNDER_CONSTRUCTION);
	/* step a day */
	ccs.date.day++;
	B_UpdateBaseData();
	/* building should be ready */
	ASSERT_EQ(building1->buildingStatus, B_STATUS_WORKING);

	/* try build other building (now it should succeed) */
	building2 = B_SetBuildingByClick(base, buildingTemplate, y, x);
	ASSERT_TRUE(nullptr != building2);

	/* try to destroy the second (should success) */
	ASSERT_TRUE(B_BuildingDestroy(building2));
	/* rebuild */
	building2 = B_SetBuildingByClick(base, buildingTemplate, y, x);
	ASSERT_TRUE(nullptr != building2);

	/* try to destroy the first (should fail) */
	ASSERT_FALSE(B_BuildingDestroy(building1));
	/* build up the second */
	ccs.date.day += building2->buildTime;
	B_UpdateBaseData();
	/* try to destroy the first (should fail) */
	ASSERT_FALSE(B_BuildingDestroy(building1));
	/* try to destroy the second (should succeess) */
	ASSERT_TRUE(B_BuildingDestroy(building2));
	/* try to destroy the first (should succeed) */
	ASSERT_TRUE(B_BuildingDestroy(building1));

	/* cleanup for the following tests */
	E_DeleteAllEmployees(nullptr);

	base->founded = false;
}

/* https://sourceforge.net/tracker/index.php?func=detail&aid=3090011&group_id=157793&atid=805242 */
TEST_F(CampaignTest, test3090011)
{
	const char* error = nullptr;
	bool success;

	success = SAV_GameLoad("3090011", &error);
	ASSERT_TRUE(success) << error;
}

static bool skipTest (const mapDef_t* md)
{
	const char* map = md->id;
	return Q_streq(map, "baseattack") || Q_streq(map, "rescue") || Q_streq(map, "alienbase");
}

TEST_F(CampaignTest, testTerrorMissions)
{
	int numUfoTypes;
	ufoType_t ufoTypes[UFO_MAX];
	mapDef_t* md;
	int i;

	/* Set overall interest level so every UFO can be used for missions */
	for (i = 0; i < UFO_MAX; i++) {
		ufoType_t ufoType = i;
		const aircraft_t* ufo = UFO_GetByType(ufoType);

		if (!ufo)
			continue;

		ccs.overallInterest = std::max(ccs.overallInterest, ufo->ufoInterestOnGeoscape);
	}

	/* Search without UFO */
	LIST_Foreach(ccs.cities, city_t, city) {
		mission_t mission;
		OBJZERO(mission);
		ASSERT_TRUE(CP_ChooseMap(&mission, city->pos)) << "could not find a map for city " << city->id;
	}

	/* Search with UFOs available for Terror missions */
	numUfoTypes = UFO_GetAvailableUFOsForMission(INTERESTCATEGORY_TERROR_ATTACK, ufoTypes, false);
	ASSERT_NE(0, numUfoTypes);
	for (i = 0; i < numUfoTypes; i++) {
		ufoType_t ufoType = i;
		mission_t mission;
		aircraft_t* ufo = UFO_AddToGeoscape(ufoTypes[ufoType], nullptr, &mission);

		OBJZERO(mission);
		mission.ufo = ufo;
		ASSERT_TRUE(nullptr != ufo);
		ufo->mission = &mission;

		LIST_Foreach(ccs.cities, city_t, city) {
			mission.mapDef = nullptr;
#ifdef TEST_BIGUFOS
			ASSERT_TRUE(CP_ChooseMap(&mission, city->pos)) << "could not find map for city " << city->id << " with ufo: " << ufo->id;
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
			bool found = false;
			for (i = 0; i < numUfoTypes; i++) {
				ufoType_t ufoType = i;
				const aircraft_t* ufo = UFO_GetByType(ufoTypes[ufoType]);

				if (!ufo)
					continue;

				if (LIST_ContainsString(md->ufos, ufo->id)) {
					found = true;
					break;
				}
			}
			if (!found)
				continue;
		}
		ASSERT_TRUE(va("%s wasn't used", md->id));
	}
}

TEST_F(CampaignTest, testRandomPosMissions)
{
	const mapDef_t* md;

	MapDef_ForeachSingleplayerCampaign(md) {
		if (!skipTest(md)) {
			mission_t mission;
			bool result;
			OBJZERO(mission);
			result = CP_GetRandomPosOnGeoscapeWithParameters(mission.pos, md->terrains, md->cultures, md->populations, nullptr);
			ASSERT_TRUE(result) << "could not find a mission for mapdef " << md->id;
		}
	}
}

static bool testEventTriggerCalled;
static void testEventTrigger_f (void)
{
	testEventTriggerCalled = true;
}

TEST_F(CampaignTest, testEventTrigger)
{
	testEventTriggerCalled = false;
	for (int i = 0; i < 60; i++) {
		CP_TriggerEvent(NEW_DAY);
		ccs.date.day++;
	}
	Cmd_AddCommand("test_eventtrigger", testEventTrigger_f);
	campaignTriggerEvent_t* event = &ccs.campaignTriggerEvents[ccs.numCampaignTriggerEvents++];
	OBJZERO(*event);
	event->active = true;
	event->type = UFO_DETECTION;
	event->id = Mem_StrDup("test_eventtrigger");
	event->command = Mem_StrDup("test_eventtrigger");
	event->require = Mem_StrDup("ufo[craft_ufo_harvester]");
	CP_TriggerEvent(UFO_DETECTION, "craft_ufo_harvester");
	ASSERT_TRUE(testEventTriggerCalled);
	testEventTriggerCalled = false;
	CP_TriggerEvent(UFO_DETECTION, "xxx");
	ASSERT_FALSE(testEventTriggerCalled);
	event->once = true;
	CP_TriggerEvent(UFO_DETECTION, "craft_ufo_harvester");
	ASSERT_TRUE(testEventTriggerCalled);
	testEventTriggerCalled = false;
	CP_TriggerEvent(UFO_DETECTION, "craft_ufo_harvester");
	ASSERT_FALSE(testEventTriggerCalled);
	--ccs.numCampaignTriggerEvents;
	Cmd_RemoveCommand("test_eventtrigger");
}

TEST_F(CampaignTest, testAssembleMap)
{
	const int expected = 22;
	const int maxSize = BASE_SIZE * BASE_SIZE;
	ASSERT_TRUE(maxSize >= expected);

	const vec2_t destination = { 10, 10 };
	base_t* base = CreateBase("unittestassemble", destination);
	ASSERT_TRUE(nullptr != base);
	char maps[2048];
	char coords[2048];

	ASSERT_TRUE(B_AssembleMap(maps, sizeof(maps), coords, sizeof(coords), base));
	const char* str = coords;
	int coordsAmount = 0;
	do {
		Com_Parse(&str);
		if (str != nullptr)
			++coordsAmount;
	} while (str != nullptr);

	str = maps;
	int mapsAmount = 0;
	do {
		Com_Parse(&str);
		if (str != nullptr)
			++mapsAmount;
	} while (str != nullptr);

	/* we have three components, x, y and z for the coordinates */
	ASSERT_EQ(coordsAmount / 3, expected) << "coords have " << coordsAmount << " entries: '" << coords << "'";
	ASSERT_EQ(mapsAmount, expected) << "maps have " << mapsAmount << " entries: '"<< maps << "'";
}

TEST_F(CampaignTest, testGeoscapeMaps)
{
	const char* types[] = {"terrain", "culture", "population", "nations", nullptr};
	for (int i = 0; i < ccs.numCampaigns; i++) {
		const campaign_t& c = ccs.campaigns[i];
		int refW = -1;
		int refH = -1;

		for (const char **t = types; *t; ++t) {
			const char *image = va("pics/geoscape/%s_%s", c.map, *t);
			SDL_Surface *surf = Img_LoadImage(image);
			ASSERT_TRUE(surf != nullptr);
			const int w = surf->w;
			const int h = surf->h;
			if (refH == -1) {
				refH = h;
				refW = w;
			}
			SDL_FreeSurface(surf);
			ASSERT_EQ(refH, h);
			ASSERT_EQ(refW, w);
		}
	}
}
