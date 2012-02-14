/**
 * @file memory.c
 * @brief Small utility that shows the size of some of the structs that are used in UFO:AI
 */

#include "../common/common.h"
#include "../shared/shared.h"
#include "../shared/mutex.h"
#include "../server/server.h"
#include "../client/client.h"
#include "../client/renderer/r_state.h" /* r_state */
#include "../client/ui/ui_main.h"
#include "../client/ui/ui_behaviour.h"
#include "../client/cgame/campaign/cp_campaign.h"
#include "../client/cgame/campaign/cp_map.h"
#include "../client/cgame/campaign/cp_hospital.h"
#include "../client/cgame/campaign/cp_missions.h"
#include "../client/cgame/campaign/cp_nation.h"
#include "../client/cgame/campaign/cp_overlay.h"
#include "../client/cgame/campaign/cp_ufo.h"
#include "../client/cgame/campaign/cp_time.h"
#include "../client/battlescape/cl_battlescape.h"
#include "../client/cgame/campaign/cp_alien_interest.h"

#define STRUCTFORMAT "%24s"
#define SIZEFORMAT "%12s"
#define TYPESIZE(type) printf(STRUCTFORMAT": "SIZEFORMAT" KB\n", #type, MEMORY_HumanReadable(sizeof(type)));

#if defined _WIN64
# define UFO_SIZE_LENGTH_T "%I64u"
#elif defined _WIN32
# define UFO_SIZE_LENGTH_T "%03u"
#else
# define UFO_SIZE_LENGTH_T "%03zu"
#endif

static const char* MEMORY_HumanReadable (size_t size)
{
	static char buf[256];

	const size_t kb = size / 1024 % 1024;
	const size_t b = size % 1024;
	const size_t mb = size / 1024 / 1024;

	snprintf(buf, sizeof(buf) - 1, UFO_SIZE_T"."UFO_SIZE_LENGTH_T"."UFO_SIZE_LENGTH_T, mb, kb, b);
	buf[sizeof(buf) - 1] = '\0';
	return buf;
}

#ifdef main
#undef main
#endif

int main (void)
{
	printf(STRUCTFORMAT"     "SIZEFORMAT"\n\n", "struct", "size");

	TYPESIZE(aircraft_t);
	TYPESIZE(aircraftProjectile_t);
	TYPESIZE(animState_t);
	TYPESIZE(base_t);
	TYPESIZE(baseTemplate_t);
	TYPESIZE(battleParam_t);
	TYPESIZE(campaign_t);
	TYPESIZE(cBspBrush_t);
	TYPESIZE(cBspHead_t);
	TYPESIZE(cBspLeaf_t);
	TYPESIZE(cBspModel_t);
	TYPESIZE(cBspNode_t);
	TYPESIZE(cBspPlane_t);
	TYPESIZE(cBspBrushSide_t);
	TYPESIZE(cBspSurface_t);
	TYPESIZE(ccs_t);
	TYPESIZE(character_t);
	TYPESIZE(chrTemplate_t);
	TYPESIZE(client_t);
	TYPESIZE(client_static_t);
	TYPESIZE(clientBattleScape_t);
	TYPESIZE(clientinfo_t);
	TYPESIZE(components_t);
	TYPESIZE(csi_t);
	TYPESIZE(cvar_t);
	TYPESIZE(damageType_t);
	TYPESIZE(dMapTile_t);
	TYPESIZE(employee_t);
	TYPESIZE(equipDef_t);
	TYPESIZE(eventMail_t);
	TYPESIZE(gametype_t);
	TYPESIZE(image_t);
	TYPESIZE(inventory_t);
	TYPESIZE(inventoryInterface_t);
	TYPESIZE(le_t);
	TYPESIZE(lighting_t);
	TYPESIZE(localModel_t);
	TYPESIZE(mAliasModel_t);
	TYPESIZE(material_t);
	TYPESIZE(materialStage_t);
	TYPESIZE(mapData_t);
	TYPESIZE(mapDef_t);
	TYPESIZE(mapTile_t);
	TYPESIZE(mapTiles_t);
	TYPESIZE(mBspModel_t)
	TYPESIZE(mission_t);
	TYPESIZE(model_t);
	TYPESIZE(nation_t);
	TYPESIZE(pathing_t);
	TYPESIZE(ptl_t);
	TYPESIZE(routing_t);
	TYPESIZE(serverInstanceGame_t);
	TYPESIZE(serverInstanceStatic_t);
	TYPESIZE(sv_edict_t);
	TYPESIZE(sv_model_t);
	TYPESIZE(teamDef_t);
	TYPESIZE(technology_t);
	TYPESIZE(tnode_t);
	TYPESIZE(uiNode_t);
	TYPESIZE(uiBehaviour_t);
	TYPESIZE(uiSharedData_t);
	TYPESIZE(worldSector_t);

	return EXIT_SUCCESS;
}
