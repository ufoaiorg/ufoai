#include "m_nodes.h"
#include "m_parse.h"

/**
 */
void MN_DrawBarNode(menuNode_t *node) {
	vec4_t color;
	float fac, bar_width;
	vec2_t nodepos;
	menu_t *menu = node->menu;
	MN_GetNodeAbsPos(node, nodepos);
	VectorScale(node->color, 0.8, color);
	color[3] = node->color[3];

	/* in the case of MN_BAR the first three data array values are float values - see menuDataValues_t */
	fac = node->size[0] / (MN_GetReferenceFloat(menu, node->data[0]) - MN_GetReferenceFloat(menu, node->data[1]));
	bar_width = (MN_GetReferenceFloat(menu, node->data[2]) - MN_GetReferenceFloat(menu, node->data[1])) * fac;
	R_DrawFill(nodepos[0], nodepos[1], bar_width, node->size[1], node->align, node->state ? color : node->color);
}

void MN_RegisterNodeBar(nodeBehaviour_t *behaviour) {
	behaviour->draw = MN_DrawBarNode;
}
