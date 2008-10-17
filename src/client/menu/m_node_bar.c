#include "m_nodes.h"
#include "m_parse.h"
#include "m_input.h"

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

/**
 * @brief Handles the bar cvar values
 * @sa Key_Message
 */
static void MN_BarClick (menuNode_t * node, int x, int y)
{
	char var[MAX_VAR];
	vec2_t pos;
	menu_t * menu = node->menu;

	if (!node->mousefx)
		return;

	MN_GetNodeAbsPos(node, pos);
	Q_strncpyz(var, node->data[2], sizeof(var));
	/* no cvar? */
	if (!Q_strncmp(var, "*cvar", 5)) {
		/* normalize it */
		const float frac = (float) (x - pos[0]) / node->size[0];
		const float min = MN_GetReferenceFloat(menu, node->data[1]);
		const float value = min + frac * (MN_GetReferenceFloat(menu, node->data[0]) - min);
		/* in the case of MN_BAR the first three data array values are float values - see menuDataValues_t */
		MN_SetCvar(&var[6], NULL, value);
	}
}

void MN_RegisterNodeBar(nodeBehaviour_t *behaviour) {
	behaviour->name = "bar";
	behaviour->draw = MN_DrawBarNode;
	behaviour->leftClick = MN_BarClick;
}

