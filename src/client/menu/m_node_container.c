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
 * @todo need to think about a common mecanism from drag-drop
 * @todo need a cleanup/marge/refactoring with MN_DrawContainerNode
 */
static void MN_DrawContainerNode2(menuNode_t *node) {
	vec2_t nodepos;
	MN_GetNodeAbsPos(node, nodepos);
	if (menuInventory) {
		const invList_t *itemHover_temp = MN_DrawContainerNode(node);
		qboolean exists;
		int itemX = 0;
		int itemY = 0;

		if (itemHover_temp)
			MN_SetItemHover(itemHover_temp);


		/** We calculate the position of the top-left corner of the dragged
		 * item in oder to compensate for the centered-drawn cursor-item.
		 * Or to be more exact, we calculate the relative offset from the cursor
		 * location to the middle of the top-left square of the item.
		 * @sa m_input.c:MN_Click */
		if (dragInfo.item.t) {
			itemX = C_UNIT * dragInfo.item.t->sx / 2;	/* Half item-width. */
			itemY = C_UNIT * dragInfo.item.t->sy / 2;	/* Half item-height. */

			/* Place relative center in the middle of the square. */
			itemX -= C_UNIT / 2;
			itemY -= C_UNIT / 2;
		}

		/* Store information for preview drawing of dragged items. */
		if (MN_CheckNodeZone(node, mousePosX, mousePosY)
		 ||	MN_CheckNodeZone(node, mousePosX - itemX, mousePosY - itemY)) {
			dragInfo.toNode = node;
			dragInfo.to = node->container;

			dragInfo.toX = (mousePosX - nodepos[0] - itemX) / C_UNIT;
			dragInfo.toY = (mousePosY - nodepos[1] - itemY) / C_UNIT;

			/** Check if the items already exists in the container. i.e. there is already at least one item.
			 * @sa Com_AddToInventory */
			exists = qfalse;
			if (dragInfo.to && dragInfo.toNode
			 && (dragInfo.to->id == csi.idFloor || dragInfo.to->id == csi.idEquip)
			 && (dragInfo.toX  < 0 || dragInfo.toY < 0 || dragInfo.toX >= SHAPE_BIG_MAX_WIDTH || dragInfo.toY >= SHAPE_BIG_MAX_HEIGHT)
			 && Com_ExistsInInventory(menuInventory, dragInfo.to, dragInfo.item)) {
					exists = qtrue;
			 }

			/** Search for a suitable position to render the item at if
			 * the container is "single", the cursor is out of bound of the container.
			 */
			 if (!exists && dragInfo.item.t && (dragInfo.to->single
			  || dragInfo.toX  < 0 || dragInfo.toY < 0
			  || dragInfo.toX >= SHAPE_BIG_MAX_WIDTH || dragInfo.toY >= SHAPE_BIG_MAX_HEIGHT)) {
	#if 0
	/* ... or there is something in the way. */
	/* We would need to check for weapon/ammo as well here, otherwise a preview would be drawn as well when hovering over the correct weapon to reload. */
			  || (Com_CheckToInventory(menuInventory, dragInfo.item.t, dragInfo.to, dragInfo.toX, dragInfo.toY) == INV_DOES_NOT_FIT)) {
	#endif
				Com_FindSpace(menuInventory, &dragInfo.item, dragInfo.to, &dragInfo.toX, &dragInfo.toY, dragInfo.ic);
			}
		}
	}
}

void MN_ContainerClick (menuNode_t *node, int x, int y) {
	MN_Drag(node, baseCurrent, x, y, qfalse);
}

void MN_ContainerRightClick (menuNode_t *node, int x, int y) {
	MN_Drag(node, baseCurrent, x, y, qtrue);
}

void MN_RegisterNodeContainer(nodeBehaviour_t* behaviour) {
	behaviour->name = "container";
	behaviour->draw = MN_DrawContainerNode2;
	behaviour->leftClick = MN_ContainerClick;
	behaviour->rightClick = MN_ContainerRightClick;
}
