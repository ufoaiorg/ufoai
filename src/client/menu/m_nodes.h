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

#include "../cl_renderer.h"
#include "../../common/common.h"
#include "../renderer/r_mesh.h"
#include "../renderer/r_draw.h"
#include "../renderer/r_mesh_anim.h"

/** @brief possible menu node types */
typedef enum mn_s {
	MN_NULL,
	MN_CONFUNC,
	MN_CVARFUNC,
	MN_FUNC,
	MN_ZONE,
	MN_PIC,
	MN_STRING,
	MN_SPINNER,
	MN_TEXT,
	MN_TEXTENTRY,
	MN_BAR,
	MN_TBAR,
	MN_MODEL,
	MN_CONTAINER,
	MN_ITEM,	/**< used to display the model of an item or aircraft */
	MN_MAP,
	MN_AIRFIGHTMAP,
	MN_BASEMAP,
	MN_BASELAYOUT,
	MN_CHECKBOX,
	MN_SELECTBOX,
	MN_LINESTRIP,
	MN_CINEMATIC, /**< every menu can only have one cinematic */
	MN_RADAR,	/**< tactical mission radar */
	MN_TAB,
	MN_CONTROLS,	/**< menu controls */
	MN_CUSTOMBUTTON,
	MN_WINDOWPANEL,
	MN_BUTTON,
	MN_WINDOW,
	MN_VSCROLLBAR,
	MN_ABSTRACTSCROLLBAR,
	MN_ABSTRACTVALUE,
	MN_ABSTRACTOPTION,
	MN_ABSTRACTNODE,

	MN_NUM_NODETYPE
} mn_t;

#define MAX_EXLUDERECTS	32

typedef struct excludeRect_s {
	vec2_t pos, size;
} excludeRect_t;

struct menuIcon_s;

/* extradata struct */
#include "node/m_node_abstractvalue.h"
#include "node/m_node_abstractscrollbar.h"
#include "node/m_node_linestrip.h"
#include "node/m_node_model.h"
#include "node/m_node_selectbox.h"
#include "node/m_node_text.h"
#include "node/m_node_textentry.h"

/**
 * @brief menu node
 * @todo delete data* when it's possible
 */
typedef struct menuNode_s {
	/* common identification */
	char name[MAX_VAR];			/**< name from the script files */
	struct nodeBehaviour_s *behaviour;

	struct menuNode_s *super; /**< Node inherited, else NULL */

	/* common navigation */
	struct menuNode_s *next;
	struct menu_s *menu;		/**< backlink */

	/* common pos */
	vec2_t pos;
	vec2_t size;

	/* common attributes */
	char key[MAX_VAR];			/**< key bindings - used as tooltip */
	byte state;					/**< e.g. the line number for text nodes to highlight due to cursor hovering */
	byte textalign;
	int border;					/**< border for this node - thickness in pixel - default 0 - also see bgcolor */
	int padding;				/**< padding for this node - default 3 - see bgcolor */
	qboolean invis;				/**< true if the node is invisible */
	qboolean blend;				/**< use the blending mode while rendering - useful to render e.g. transparent images */
	qboolean disabled;			/**< true if the node is inactive */
	int mousefx;
	char* text;
	const char* font;			/**< Font to draw text */
	const char* tooltip;		/**< holds the tooltip */
	struct menuIcon_s *icon;	/**< Link to an icon */

	/* not used a lot */
	excludeRect_t *excludeRect;	/**< exclude this for hover or click functions */
	int excludeRectNum;					/**< how many exclude rects defined? */


	byte align;					/** @todo delete it when its possible */

	/** @todo needs cleanup */
	void* dataImageOrModel;	/**< an image, or a model, this depends on the node type */
	void* dataModelSkinOrCVar; /**< a skin or a cvar, this depends on the node type */

	/* common color */
	vec4_t color;				/**< rgba */
	vec4_t bgcolor;				/**< rgba */
	vec4_t bordercolor;			/**< rgba - see border and padding */
	vec4_t selectedColor;		/**< rgba The color to draw the line specified by textLineSelected in. */

	/* common events */
	struct menuAction_s *onClick;
	struct menuAction_s *onRightClick;
	struct menuAction_s *onMiddleClick;
	struct menuAction_s *onWheel;
	struct menuAction_s *onMouseIn;
	struct menuAction_s *onMouseOut;
	struct menuAction_s *onWheelUp;
	struct menuAction_s *onWheelDown;
	struct menuAction_s *onChange;	/**< called when the widget change from an user action */

	/** @todo needs cleanup */
	vec3_t scale;
	invDef_t *container;		/** The container linked to this node. */
	int timeOut;				/**< ms value until invis is set (see cl.time) */
	int timePushed;				/**< when a menu was pushed this value is set to cl.time */
	qboolean timeOutOnce;		/**< timeOut is decreased if this value is true */
	menuDepends_t depends;
	const value_t *scriptValues;

	/* MN_IMAGE, and more */
	vec2_t texh;				/**< lower right texture coordinates, for text nodes texh[0] is the line height and texh[1] tabs width */
	vec2_t texl;				/**< upper left texture coordinates */
	qboolean preventRatio;

	/* MN_TBAR */
	float pointWidth;			/**< MN_TBAR: texture pixels per one point */
	int gapWidth;				/**< MN_TBAR: tens separator width */

	/* MN_TEXT */
	/** @todo remove it  from 'string node', need to full implement R_FontDrawStringInBox */
	byte longlines;				/**< what to do with long lines */

	/* MN_ZONE */
	qboolean repeat;			/**< repeat action when "click" is holded */
	int clickDelay;				/**< for nodes that have repeat set, this is the delay for the next click */

	/* BaseLayout */
	int baseid;					/**< the baseid - e.g. for baselayout nodes */

	/** union will contain all extradata for a node */
	union {
		abstractValueExtraData_t abstractvalue;
		abstractScrollbarExtraData_t abstractscrollbar;
		lineStripExtraData_t linestrip;	/**< List of lines to draw. (MN_LINESTRIP) */
		modelExtraData_t model;
		optionExtraData_t option;
		textEntryExtraData_t textentry;
		textExtraData_t text;
	} u;

} menuNode_t;

/**
 * @brief node behaviour, how a node work
 * @sa virtualFunctions
 */
typedef struct nodeBehaviour_s {
	/* behaviour attributes */
	const char* name;				/**< name of the behaviour: string type of a node */
	const char* extends;			/**< name of the extends behaviour */
	int id;							/**< id of the behaviour: @todo will be removed soon */
	qboolean isVirtual;				/**< true, if the node dont have any position on the screen */
	qboolean isAbstract;			/**< true, if we can't instanciate the behaviour */
	qboolean isInitialized;			/**< cache if we already have initialized the node behaviour */
	const value_t* properties;		/**< list of properties of the node */
	int propertyCount;				/**< number of the properties into the propertiesList. Cache value to speedup search */
	struct nodeBehaviour_s *super;	/**< link to the extended node */

	/* node functions */
	void (*draw)(menuNode_t *node);							/**< how to draw a node */
	void (*drawTooltip)(menuNode_t *node, int x, int y);	/**< allow to draw a custom tooltip */
	void (*leftClick)(menuNode_t *node, int x, int y);		/**< left mouse click event in the node */
	void (*rightClick)(menuNode_t *node, int x, int y);		/**< left mouse button click event in the node */
	void (*middleClick)(menuNode_t *node, int x, int y);	/**< right mouse button click event in the node */
	void (*mouseWheel)(menuNode_t *node, qboolean down, int x, int y);	/**< mouse wheel event in the node */
	void (*mouseMove)(menuNode_t *node, int x, int y);
	void (*mouseDown)(menuNode_t *node, int x, int y, int button);	/**< mouse button down event in the node */
	void (*mouseUp)(menuNode_t *node, int x, int y, int button);	/**< mouse button up event in the node */
	void (*capturedMouseMove)(menuNode_t *node, int x, int y);
	void (*loading)(menuNode_t *node);		/**< called before script initialization, inits default values */
	void (*loaded)(menuNode_t *node);		/**< only called one time, when node parsing was finished */

	/* drag and drop callback */
	qboolean (*dndEnter)(menuNode_t *node);							/**< Send to the target when we enter first, return true if we can drop the DND somewhere on the node */
	qboolean (*dndMove)(menuNode_t *node, int x, int y);			/**< Send to the target when we enter first, return true if we can drop the DND here */
	void (*dndLeave)(menuNode_t *node);								/**< Send to the target when the DND is cancelled */
	qboolean (*dndDrop)(menuNode_t *node, int x, int y);			/**< Send to the target to finalize the drop */
	qboolean (*dndFinished)(menuNode_t *node, qboolean isDroped);	/**< Sent to the source to finalize the drop */

	/* Planned */
#if 0
	/* mouse move event */
	void (*mouseEnter)(menuNode_t *node);
	void (*mouseLeave)(menuNode_t *node);
#endif
} nodeBehaviour_t;

qboolean MN_CheckNodeZone(menuNode_t* const node, int x, int y);
menuNode_t* MN_AllocNode(const char* type);
nodeBehaviour_t* MN_GetNodeBehaviour(const char* name);
struct value_s;
const struct value_s *MN_NodeGetPropertyDefinition(const menuNode_t* node, const char* name);
const struct value_s *MN_GetPropertyFromBehaviour (const nodeBehaviour_t *behaviour, const char* name);

void MN_InitNodes(void);
menuNode_t* MN_CloneNode(const menuNode_t* node, struct menu_s *newMenu, qboolean recursive);

#endif
