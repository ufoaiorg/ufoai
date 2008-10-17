#include "m_nodes.h"
#include "m_parse.h"

/**
 * @todo move it to a m_node_linestrip.c
 */
void MN_DrawLineStripNode(menuNode_t *node) {
	int i;
	if (node->linestrips.numStrips > 0) {
		/* Draw all linestrips. */
		for (i = 0; i < node->linestrips.numStrips; i++) {
			/* Draw this line if it's valid. */
			if (node->linestrips.pointList[i] && (node->linestrips.numPoints[i] > 0)) {
				R_ColorBlend(node->linestrips.color[i]);
				R_DrawLineStrip(node->linestrips.numPoints[i], node->linestrips.pointList[i]);
			}
		}
	}
}

void MN_RegisterNodeLineStrip(nodeBehaviour_t *behaviour) {
	behaviour->draw = MN_DrawLineStripNode;
}
