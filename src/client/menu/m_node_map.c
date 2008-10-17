#include "../client.h"
#include "../cl_map.h"
#include "m_main.h"
#include "m_draw.h"
#include "m_parse.h"
#include "m_font.h"
#include "m_inventory.h"
#include "m_tooltip.h"
#include "m_nodes.h"
#include "m_node_model.h"

/**
 */
void MN_DrawMapNode(menuNode_t *node) {
	if (curCampaign) {
		/* don't run the campaign in console mode */
		if (cls.key_dest != key_console)
			CL_CampaignRun();	/* advance time */
		MAP_DrawMap(node); /* Draw geoscape */
	}
}

void MN_RegisterNodeMap(nodeBehaviour_t *behaviour) {
	behaviour->name = "map";
	behaviour->draw = MN_DrawMapNode;
}
