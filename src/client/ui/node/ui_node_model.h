/**
 * @file ui_node_model.h
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef CLIENT_UI_UI_NODE_MODEL_H
#define CLIENT_UI_UI_NODE_MODEL_H

#include "../ui_nodes.h"
#include "../../cl_renderer.h"	/**< include animState_t */

#define UI_MAX_MODELS		128

/** @brief Model that have more than one part (top and down part of an aircraft) */
typedef struct uiModel_s {
	char *id;
	char *anim;	/**< animation to run for this model */
	char *parent;	/**< parent model id */
	char *tag;	/**< the tag the model should placed onto */
	int skin;		/**< skin number to use - default 0 (first skin) */
	char *model;
	animState_t animState;
	vec3_t origin, scale, angles, center;	/**< to cache the calculated values */
	vec4_t color;				/**< rgba */
	struct uiModel_s *next;
} uiModel_t;

/**
 * @brief extradata for the model node
 */
typedef struct modelExtraData_s {
	char *oldRefValue;	/**< used for storing old reference values */
	vec3_t angles;
	vec3_t origin;
	vec3_t omega;
	vec3_t scale;
	const char* skin;
	const char* model;
	const char* tag;					/**< the tag to place the model onto */
	animState_t* animationState;		/**< holds then anim state for the current model */
	const char* animation;			/**< Anim string from the *.anm files */
	qboolean autoscale;			/**< If true autoscale the model when we draw it */
	qboolean rotateWithMouse;	/**< If true the user can rotate the model with the mouse */
	qboolean clipOverflow;		/**< If true (default) model outside the node are clipped */
	qboolean containerLike;		/**< Display an item like an item from the container */
} modelExtraData_t;

uiModel_t *UI_GetUIModel(const char *modelName);
void UI_DrawModelNode(struct uiNode_s *node, const char *source);
void UI_RegisterModelNode(struct uiBehaviour_s *behaviour);

#endif
