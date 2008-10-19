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
 * @todo move it to a m_node_item.c
 * @todo need a cleanup/marge/refactoring with MN_DrawItemNode
 */
void MN_DrawItemNode2 (menuNode_t *node)
{
	menu_t *menu = node->menu;
	const char* ref = MN_GetSaifeReferenceString(node->menu, node->dataImageOrModel);
	if (!ref) {
		return;
	}

	const objDef_t *od = INVSH_GetItemByIDSilent(ref);
	if (od) {
		MN_DrawItemNode(node, ref);
	} else {
		const aircraft_t *aircraft = AIR_GetAircraft(ref);
		if (aircraft) {
			assert(aircraft->tech);
			MN_DrawModelNode(menu, node, ref, aircraft->tech->mdl);
		} else {
			Com_Printf("Unknown item: '%s'\n", ref);
		}
	}
}

void MN_RegisterNodeItem (nodeBehaviour_t *behaviour)
{
	behaviour->name = "item";
	behaviour->draw = MN_DrawItemNode2;
}
