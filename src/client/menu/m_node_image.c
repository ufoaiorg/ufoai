#include "../client.h"
#include "../cl_map.h"
#include "m_main.h"
#include "m_draw.h"
#include "m_parse.h"
#include "m_font.h"
#include "m_inventory.h"
#include "m_tooltip.h"
#include "m_nodes.h"

void MN_DrawImageNode(menuNode_t *node)
{
	vec2_t size;
	vec2_t nodepos;

	int time = cl.time;

	const char* imageName = MN_GetSaifeReferenceString(node->menu, node->dataImageOrModel);
	if (!imageName)
		return;

	MN_GetNodeAbsPos(node, nodepos);
	/* HACK for ekg pics */
	if (!Q_strncmp(node->name, "ekg_", 4)) {
		int pt;

		if (node->name[4] == 'm')
			pt = Cvar_VariableInteger("mn_morale") / 2;
		else
			pt = Cvar_VariableInteger("mn_hp");
		node->texl[1] = (3 - (int) (pt < 60 ? pt : 60) / 20) * 32;
		node->texh[1] = node->texl[1] + 32;
		node->texl[0] = -(int) (0.01 * (node->name[4] - 'a') * time) % 64;
		node->texh[0] = node->texl[0] + node->size[0];
	}
	if ((node->size[0] && !node->size[1])) {
		float scale;
		int w, h;

		R_DrawGetPicSize(&w, &h, imageName);
		scale = w / node->size[0];
		Vector2Set(size, node->size[0], h / scale);
	} else if ((node->size[1] && !node->size[0])) {
		float scale;
		int w, h;

		R_DrawGetPicSize(&w, &h, imageName);
		scale = h / node->size[1];
		Vector2Set(size, w / scale, node->size[1]);
	} else {
		Vector2Copy(node->size, size);
	}
	R_DrawNormPic(nodepos[0], nodepos[1], size[0], size[1],
		node->texh[0], node->texh[1], node->texl[0], node->texl[1], node->align, node->blend, imageName);
}

void MN_RegisterNodeImage (nodeBehaviour_t* behaviour)
{
	behaviour->name = "pic";
	behaviour->draw = MN_DrawImageNode;
}
