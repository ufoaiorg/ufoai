#include "m_nodes.h"
#include "m_parse.h"
#include "m_input.h"
#include "m_main.h"
#include "m_node_image.h"
#include "../cl_input.h"

static void MN_ControlsNodeClick (menuNode_t *node, int x, int y)
{
	if (mouseSpace == MS_MENU) {
		mouseSpace = MS_DRAGMENU;
	}
}

static void MN_ControlsNodeDraw(menuNode_t *node)
{
	if (mouseSpace == MS_DRAGMENU){
		MN_SetNewMenuPos(node->menu, mousePosX - node->pos[0], mousePosY - node->pos[1]);
	}
	MN_DrawImageNode(node);
}

void MN_RegisterNodeControls (nodeBehaviour_t *behaviour)
{
	behaviour->name = "controls";
	behaviour->leftClick = MN_ControlsNodeClick;
	behaviour->draw = MN_ControlsNodeDraw;
}
