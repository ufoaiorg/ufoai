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
void MN_DrawCinematicNode(menuNode_t *node) {
	vec2_t nodepos;
	MN_GetNodeAbsPos(node, nodepos);
	if (node->data[MN_DATA_STRING_OR_IMAGE_OR_MODEL]) {
		assert(cls.playingCinematic != CIN_STATUS_FULLSCREEN);
		if (cls.playingCinematic == CIN_STATUS_NONE)
			CIN_PlayCinematic(node->data[MN_DATA_STRING_OR_IMAGE_OR_MODEL]);
		if (cls.playingCinematic) {
			/* only set replay to true if video was found and is running */
			CIN_SetParameters(nodepos[0], nodepos[1], node->size[0], node->size[1], CIN_STATUS_MENU, qtrue);
			CIN_RunCinematic();
		}
	}
}

void MN_RegisterNodeCinematic(nodeBehaviour_t* behaviour) {
	behaviour->name = "cinematic";
	behaviour->draw = MN_DrawCinematicNode;
}
