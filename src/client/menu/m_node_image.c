
#include "m_nodes.h"

void MN_DrawImageNode (menuNode_t *node, const char *imageName, int time)
{
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
	R_DrawNormPic(node->pos[0], node->pos[1], node->size[0], node->size[1],
		node->texh[0], node->texh[1], node->texl[0], node->texl[1], node->align, node->blend, imageName);
}
