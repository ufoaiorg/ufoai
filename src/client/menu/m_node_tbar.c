#include "m_nodes.h"
#include "m_parse.h"

/**
 */
void MN_DrawTBarNode(menuNode_t *node) {
	/* data[0] is the texture name, data[1] is the minimum value, data[2] is the current value */
	float ps, shx;
	vec2_t nodepos;
	menu_t *menu = node->menu;
	const char* ref = MN_GetNodeReference(node);
	if (!ref || !*ref)
		return;
	MN_GetNodeAbsPos(node, nodepos);
	if (node->pointWidth) {
		ps = MN_GetReferenceFloat(menu, node->data[2]) - MN_GetReferenceFloat(menu, node->data[1]);
		shx = node->texl[0] + round(ps * node->pointWidth) + (ps > 0 ? floor((ps - 1) / 10) * node->gapWidth : 0);
	} else
		shx = node->texh[0];
	R_DrawNormPic(nodepos[0], nodepos[1], node->size[0], node->size[1],
		shx, node->texh[1], node->texl[0], node->texl[1], node->align, node->blend, ref);
}

void MN_RegisterNodeTBar(nodeBehaviour_t *behaviour) {
	behaviour->draw = MN_DrawTBarNode;
}
