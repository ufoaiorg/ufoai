
#include "m_nodes.h"
#include "m_font.h"
#include "../client.h"

/**
 */
void MN_DrawStringNode(menuNode_t *node) {
	vec2_t nodepos;
	menu_t *menu = node->menu;
	const char *font = MN_GetFont(menu, node);
	const char* ref = MN_GetNodeReference(node);
	if (!ref || !*ref)
		return;
	MN_GetNodeAbsPos(node, nodepos);
	ref += node->horizontalScroll;
	/* blinking */
	if (!node->mousefx || cl.time % 1000 < 500)
		R_FontDrawString(font, node->align, nodepos[0], nodepos[1], nodepos[0], nodepos[1], node->size[0], 0, node->texh[0], ref, 0, 0, NULL, qfalse);
	else
		R_FontDrawString(font, node->align, nodepos[0], nodepos[1], nodepos[0], nodepos[1], node->size[0], node->size[1], node->texh[0], va("%s*\n", ref), 0, 0, NULL, qfalse);
}
