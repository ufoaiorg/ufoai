/**
 * @file m_node_model.c
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "../client.h"
#include "m_node_model.h"

/**
 * @brief Add a menu link to menumodel definition for faster access
 * @note Called after all menus are parsed - only once
 */
void MN_LinkMenuModels (void)
{
	int i, j;
	menuModel_t *m;

	for (i = 0; i < mn.numMenuModels; i++) {
		m = &mn.menuModels[i];
		for (j = 0; j < m->menuTransformCnt; j++) {
			m->menuTransform[j].menuPtr = MN_GetMenu(m->menuTransform[j].menuID);
			if (m->menuTransform[j].menuPtr == NULL)
				Com_Printf("Could not find menu '%s' as requested by menumodel '%s'", m->menuTransform[j].menuID, m->id);
			/* we don't need this anymore */
			Mem_Free(m->menuTransform[j].menuID);
			m->menuTransform[j].menuID = NULL;
		}
	}
}

/**
 * @brief Returns pointer to menu model
 * @param[in] menuModel menu model id from script files
 * @return menuModel_t pointer
 */
menuModel_t *MN_GetMenuModel (const char *menuModel)
{
	int i;
	menuModel_t *m;

	for (i = 0; i < mn.numMenuModels; i++) {
		m = &mn.menuModels[i];
		if (!Q_strncmp(m->id, menuModel, MAX_VAR))
			return m;
	}
	return NULL;
}

void MN_ListMenuModels_f (void)
{
	int i;

	/* search for menumodels with same name */
	Com_Printf("menu models: %i\n", mn.numMenuModels);
	for (i = 0; i < mn.numMenuModels; i++)
		Com_Printf("id: %s\n...model: %s\n...need: %s\n\n", mn.menuModels[i].id, mn.menuModels[i].model, mn.menuModels[i].need);
}


#ifdef DEBUG
/**
 * @brief This function allows you inline transforming of mdoels.
 * @note Changes you made are lost on quit
 */
static void MN_SetModelTransform_f (void)
{
	menuNode_t *node;
	const char *command, *nodeID;
	float x, y ,z;
	vec3_t value;

	/* not initialized yet - commandline? */
	if (mn.menuStackPos <= 0)
		return;

	if (Cmd_Argc() < 5) {
		Com_Printf("Usage: %s <node> <x> <y> <z>\n", Cmd_Argv(0));
		return;
	}

	command = Cmd_Argv(0);
	nodeID = Cmd_Argv(1);
	x = atof(Cmd_Argv(2));
	y = atof(Cmd_Argv(3));
	z = atof(Cmd_Argv(4));

	VectorSet(value, x, y, z);

	/* search the node */
	node = MN_GetNode(MN_GetActiveMenu(), nodeID);
	if (!node) {
		/* didn't find node -> "kill" action and print error */
		Com_Printf("MN_SetModelTransform_f: node \"%s\" doesn't exist\n", nodeID);
		return;
	} else if (node->type != MN_MODEL) {
		Com_Printf("MN_SetModelTransform_f: node \"%s\" isn't a model node\n", nodeID);
		return;
	}

	if (!Q_strcmp(command, "debug_mnscale")) {
		VectorCopy(value, node->scale);
	} else if (!Q_strcmp(command, "debug_mnangles")) {
		VectorCopy(value, node->angles);
	}
}

#endif

void MN_NodeModelInit (void)
{
#ifdef DEBUG
	Cmd_AddCommand("debug_mnscale", MN_SetModelTransform_f, "Transform model from command line.");
	Cmd_AddCommand("debug_mnangles", MN_SetModelTransform_f, "Transform model from command line.");
#endif
	Cmd_AddCommand("menumodelslist", MN_ListMenuModels_f, NULL);
}
