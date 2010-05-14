/**
 * @file m_node_model.h
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#ifndef CLIENT_MENU_M_NODE_MODEL_H
#define CLIENT_MENU_M_NODE_MODEL_H

#include "../m_nodes.h"
#include "../../cl_renderer.h"	/**< include animState_t */

#define MAX_MENUMODELS		128

/** @brief Model that have more than one part (head, body) but may only use one menu node */
typedef struct menuModel_s {
	char *id;
	char *need;
	char *anim;	/**< animation to run for this model */
	char *parent;	/**< parent model id */
	char *tag;	/**< the tag the model should placed onto */
	int skin;		/**< skin number to use - default 0 (first skin) */
	char *model;
	animState_t animState;
	vec3_t origin, scale, angles, center;	/**< to cache the calculated values */
	vec4_t color;				/**< rgba */
	struct menuModel_s *next;
} menuModel_t;

/**
 * @brief extradata for the model node
 */
typedef struct modelExtraData_s {
	char *oldRefValue;	/**< used for storing old reference values */
	vec3_t angles;
	vec3_t origin;
	vec3_t scale;
	void* skin;
	void* model;
	void* tag;					/**< the tag to place the model onto */
	void* animationState;		/**< holds then anim state for the current model */
	void* animation;			/**< Anim string from the *.anm files */
	char *viewName;				/**< view name to use, if it exists (item, ufopedia, buy...) @sa base/ufos/models.ufo */
	qboolean autoscale;			/**< If true autoscale the model when we drw it */
	qboolean rotateWithMouse;	/**< If true the user can rotate the model with the mouse */
	qboolean clipOverflow;		/**< If true (default) model outside the node are clipped */
} modelExtraData_t;

menuModel_t *MN_GetMenuModel(const char *menuModel);
void MN_ListMenuModels_f(void);
void MN_DrawModelNode(struct menuNode_s *node, const char *source);
void MN_RegisterModelNode(struct nodeBehaviour_s *behaviour);

#endif
