/**
 * @file m_nodes.h
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

#ifndef CLIENT_MENU_M_NODES_H
#define CLIENT_MENU_M_NODES_H

#include "m_menu.h"
#include "m_node_abstractvalue.h"
#include "../cl_renderer.h"
#include "../../common/common.h"
#include "../../renderer/r_mesh.h"
#include "../../renderer/r_draw.h"
#include "../../renderer/r_mesh_anim.h"

/** @brief possible menu node types */
typedef enum mn_s {
	MN_NULL,
	MN_CONFUNC,
	MN_CVARFUNC,
	MN_FUNC,
	MN_ZONE,
	MN_PIC,
	MN_STRING,
	MN_TEXT,
	MN_BAR,
	MN_TBAR,
	MN_MODEL,
	MN_CONTAINER,
	MN_ITEM,	/**< used to display the model of an item or aircraft */
	MN_MAP,
	MN_BASEMAP,
	MN_BASELAYOUT,
	MN_CHECKBOX,
	MN_SELECTBOX,
	MN_LINESTRIP,
	MN_CINEMATIC, /**< every menu can only have one cinematic */
	MN_TEXTLIST,
	MN_RADAR,	/**< tactical mission radar */
	MN_TAB,
	MN_CONTROLS,

	MN_NUM_NODETYPE
} mn_t;

#define MAX_EXLUDERECTS	16

typedef struct excludeRect_s {
	vec2_t pos, size;
} excludeRect_t;

/* all available select box options - for all menunodes */
#define MAX_SELECT_BOX_OPTIONS 128
#define SELECTBOX_DEFAULT_HEIGHT 20.0f
#define SELECTBOX_SIDE_WIDTH 7.0f
#define SELECTBOX_BOTTOM_HEIGHT 4.0f
#define SELECTBOX_SPACER 2.0f
#define SELECTBOX_MAX_VALUE_LENGTH 32

/** @brief MN_SELECTBOX definition */
typedef struct selectBoxOptions_s {
	char id[MAX_VAR];	/**< text for the select box - V_TRANSLATION_MANUAL_STRING */
	char label[SELECTBOX_MAX_VALUE_LENGTH];	/**< text for the select box - V_TRANSLATION_MANUAL_STRING */
	char action[MAX_VAR];	/**< execute this when the value is selected */
	char value[MAX_VAR];	/**< the value the cvar should get */
	struct selectBoxOptions_s *next;	/**< pointer to next option entry for this node
							 * NULL terminated for each node */
	qboolean hovered;		/**< current selected option entry selected? */
} selectBoxOptions_t;

#define MAX_LINESTRIPS 16

typedef struct lineStrips_s {
	int *pointList[MAX_LINESTRIPS];	/**< Pointers to lists of 2d coordiantes MN_LINESTRIP. */
	int numPoints[MAX_LINESTRIPS];	/**< Number of points in each list */
	vec4_t color[MAX_LINESTRIPS];	/**< Color of the point-list. */
	int numStrips;			/**< Number of point-lists. */
} lineStrips_t;

/** @brief extradata node for the model node
 * @todo move it on m_node_model.h
 */
typedef struct {
	char oldRefValue[MAX_VAR];	/**< used for storing old reference values */
	vec3_t angles;
	vec3_t origin;
	vec3_t center;
	struct menuModel_s *menuModel;		/**< pointer to menumodel definition from models.ufo */
	void* tag;	/**< the tag to place the model onto */
	void* animationState;	/**< holds then anim state for the current model */
} modelDataNode_t;

/** @brief menu node
 * @todo delete data* when it's possible
 */
typedef struct menuNode_s {
	char name[MAX_VAR];
	char key[MAX_VAR];
	int type;
	vec3_t scale;

	vec2_t pos;
	vec2_t size;
	vec2_t texh;				/**< lower right texture coordinates, for text nodes texh[0] is the line height and texh[1] tabs width */
	vec2_t texl;				/**< upper left texture coordinates */
	byte state;
	byte align;
	int border;					/**< border for this node - thickness in pixel - default 0 - also see bgcolor */
	int padding;				/**< padding for this node - default 3 - see bgcolor */
	qboolean invis, blend;
	int mousefx;
	invDef_t *container;		/** The container linked to this node. */
	int horizontalScroll;		/**< if text is too long, the text is horizontally scrolled, @todo implement me */
	int textScroll;				/**< textfields - current scroll position */
	int textLines;				/**< How many lines there are (set by MN_DrawMenus)*/
	int textLineSelected;		/**< Which line is currenlty selected? This counts only visible lines). Add textScroll to this value to get total linecount. @sa selectedColor below.*/
	int timeOut;				/**< ms value until invis is set (see cl.time) */
	int timePushed;				/**< when a menu was pushed this value is set to cl.time */
	qboolean timeOutOnce;		/**< timeOut is decreased if this value is true */
	int num;					/**< textfields: menutexts-id - baselayouts: baseID */
	int height;					/**< textfields: max. rows to show
								 * select box: options count */
	int baseid;					/**< the baseid - e.g. for baselayout nodes */
	struct selectBoxOptions_s *options;	/**< pointer to select box options when type is MN_SELECTBOX */
	vec4_t color;				/**< rgba */
	vec4_t bgcolor;				/**< rgba */
	vec4_t bordercolor;			/**< rgba - see border and padding */
	vec4_t selectedColor;		/**< rgba The color to draw the line specified by textLineSelected in. */
	struct menuAction_s *click, *rclick, *mclick, *wheel, *mouseIn, *mouseOut, *wheelUp, *wheelDown;
	qboolean repeat;			/**< repeat action when "click" is holded */
	int clickDelay;				/**< for nodes that have repeat set, this is the delay for the next click */
	qboolean scrollbar;			/**< if you want to add a scrollbar to a text node, set this to true */
	qboolean scrollbarLeft;		/**< true if the scrollbar should be on the left side of the text node */
	excludeRect_t exclude[MAX_EXLUDERECTS];	/**< exclude this for hover or click functions */
	int excludeNum;				/**< how many exclude rects defined? */
	menuDepends_t depends;
	struct menuNode_s *next;
	struct menu_s *menu;		/**< backlink */
	lineStrips_t linestrips;	/**< List of lines to draw. (MN_LINESTRIP) */
	float pointWidth;			/**< MN_TBAR: texture pixels per one point */
	int gapWidth;				/**< MN_TBAR: tens separator width */
	const value_t *scriptValues;
	void* dataStringOrImageOrModel;	/**< First entry in data array can
						* be an image, an model or an string, this depends
						* on the node type
						*/
	void* dataModelSkinOrCVar; /**< the skin of the model */
	void* dataNodeTooltip; /**< holds the tooltip for the menu */
	void* dataAnimOrFont;	/** Anim string from the *.anm files for the model; or font */

	/** union will contain all extradata for a node */
	union {
		abstractValueDataNode_t abstractvalue;
		modelDataNode_t model;
	};

} menuNode_t;

/** @brief node behaviour, how a node work */
typedef struct {
	char* name;							/**< name of the behaviour: string type of a node */
	void (*draw)(menuNode_t *node);		/**< how to draw a node */
	void (*leftClick)(menuNode_t *node, int x, int y); /**< on left mouse click into the node */
	void (*rightClick)(menuNode_t *node, int x, int y); /**< on left mouse button click into the node */
	void (*middleClick)(menuNode_t *node, int x, int y); /**< on right mouse button click into the node */
	void (*mouseWheel)(menuNode_t *node, qboolean down, int x, int y); /**< on use mouse wheel into the node */
	void (*mouseMove)(menuNode_t *node, int x, int y);

	/** Planed */
	/*
	mouse move event
	void (*mouseEnter)(menuNode_t *node);
	void (*mouseLeave)(menuNode_t *node);
	void (*mouseMove)(menuNode_t *node, int relativeX, int relativeY);
	mouse click event
	void (*middleClickDown)(menuNode_t *node);
	void (*middleClickUp)(menuNode_t *node);
	void (*rightClickDown)(menuNode_t *node);
	void (*rightClickUp)(menuNode_t *node);
	void (*leftClickDown)(menuNode_t *node);
	void (*leftClickUp)(menuNode_t *node);
	void (*mouseWheel)(menuNode_t *node);
	captured mouse callback
	void (*capturedMouseMove)(menuNode_t *node, int relativeX, int relativeY);
	drag and drop callback
	int  (*dndEnter)(menuNode_t *node);
	void (*dndLeave)(menuNode_t *node);
	int  (*dndMove)(menuNode_t *node);
	int  (*dndEnd)(menuNode_t *node);
	*/
} nodeBehaviour_t;

extern nodeBehaviour_t nodeBehaviourList[MN_NUM_NODETYPE];

qboolean MN_CheckNodeZone(menuNode_t* const node, int x, int y);
void MN_UnHideNode(menuNode_t* node);
void MN_HideNode(menuNode_t* node);
menuNode_t* MN_GetNodeFromCurrentMenu(const char *name);
void MN_SetNewNodePos(menuNode_t* node, int x, int y);
menuNode_t *MN_GetNode(const menu_t* const menu, const char *name);
void MN_GetNodeAbsPos (const menuNode_t* node, vec2_t pos);
const char *MN_GetNodeReference(const menuNode_t *node);

void MN_InitNodes(void);

#include "m_node_image.h"
#include "m_node_model.h"
#include "m_node_string.h"
#include "m_node_text.h"
#include "m_node_selectbox.h"
#include "m_node_checkbox.h"
#include "m_node_radar.h"
#include "m_node_tab.h"

#endif
