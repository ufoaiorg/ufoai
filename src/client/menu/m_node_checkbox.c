
#include "m_nodes.h"
#include "m_parse.h"
#include "m_input.h"

void MN_DrawCheckBoxNode (menuNode_t* node)
{
	const char *image;
	const char *ref;
	vec2_t nodepos;
	const menu_t* menu = node->menu;
	const char *checkBoxImage = MN_GetNodeReference(node);

	MN_GetNodeAbsPos(node, nodepos);
	/* image set? */
	if (checkBoxImage && *checkBoxImage)
		image = checkBoxImage;
	else
		image = "menu/checkbox";

	ref = MN_GetReferenceString(menu, node->dataModelSkinOrCVar);
	if (!ref)
		return;

	switch (*ref) {
	case '0':
		R_DrawNormPic(nodepos[0], nodepos[1], node->size[0], node->size[1],
			node->texh[0], node->texh[1], node->texl[0], node->texl[1], node->align, node->blend, va("%s_unchecked", image));
		break;
	case '1':
		R_DrawNormPic(nodepos[0], nodepos[1], node->size[0], node->size[1],
			node->texh[0], node->texh[1], node->texl[0], node->texl[1], node->align, node->blend, va("%s_checked", image));
		break;
	default:
		Com_Printf("Error - invalid value for MN_CHECKBOX node - only 0/1 allowed (%s)\n", ref);
	}
}

/**
 * @brief Handles checkboxes clicks
 */
static void MN_CheckboxClick (menuNode_t * node, int x, int y)
{
	int value;
	const char *cvarName;

	assert(node->dataModelSkinOrCVar);
	/* no cvar? */
	if (Q_strncmp(node->dataModelSkinOrCVar, "*cvar", 5))
		return;

	cvarName = &((const char *)node->dataModelSkinOrCVar)[6];
	value = Cvar_VariableInteger(cvarName) ^ 1;
	MN_SetCvar(cvarName, NULL, value);
}

void MN_RegisterNodeCheckBox (nodeBehaviour_t *behaviour)
{
	behaviour->name = "checkbox";
	behaviour->draw = MN_DrawCheckBoxNode;
	behaviour->leftClick = MN_CheckboxClick;
}
