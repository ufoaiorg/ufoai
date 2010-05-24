/**
 * @file m_nodes.h
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

#ifndef CLIENT_MENU_M_NODES_H
#define CLIENT_MENU_M_NODES_H

#include "../../shared/ufotypes.h"
#include "../../common/scripts.h"

/* prototype */
struct menuIcon_s;
struct menuCondition_s;
struct value_s;
struct nodeKeyBinding_s;
struct menuCallContext_s;
struct menuNode_s;
struct menuModel_s;

/* exclude rect */
#define MAX_EXLUDERECTS	64

typedef struct excludeRect_s {
	vec2_t pos, size;
} excludeRect_t;

typedef void (*menuNodeMethod_t)(struct menuNode_s*node, const struct menuCallContext_s *context);

/**
 * @brief menu node
 */
typedef struct menuNode_s {
	/* common identification */
	char name[MAX_VAR];			/**< name from the script files */
	struct nodeBehaviour_s *behaviour;
	struct menuNode_s *super;	/**< Node inherited, else NULL */
	qboolean dynamic;			/** True if it use dynamic memory */

	/* common navigation */
	struct menuNode_s *firstChild;	/**< first element of linked list of child */
	struct menuNode_s *lastChild;	/**< last element of linked list of child */
	struct menuNode_s *next;		/**< Next element into linked list */
	struct menuNode_s *parent;		/**< Parent menu, else NULL */
	struct menuNode_s *root;		/**< Shortcut to the root node */

	/* common pos */
	vec2_t pos;
	vec2_t size;

	/* common attributes */
	int padding;				/**< padding for this node - default 3 - see bgcolor */
	int align;					/**< used to identify node position into a parent using a layout manager. Else it do nothing. */
	int num;					/**< used to identify child into a parent; not sure it is need @todo delete it */
	byte textalign;
	qboolean invis;				/**< true if the node is invisible */
	qboolean disabled;			/**< true if the node is inactive */
	qboolean invalidated;		/**< true if we need to update the layout */
	qboolean ghost;				/**< true if the node is not tangible */
	char* text;
	const char* font;			/**< Font to draw text */
	const char* tooltip;		/**< holds the tooltip */
	struct menuKeyBinding_s *key;	/**< key bindings - used as tooltip */
	struct menuIcon_s *icon;	/**< Link to an icon */

	/** @todo use a linked list of excluderect? */
	excludeRect_t *excludeRect;	/**< exclude this for hover or click functions */
	int excludeRectNum;			/**< how many consecutive exclude rects defined? */

	struct menuAction_s* visibilityCondition;	/**< cvar condition to display/hide the node */

	/** @todo needs cleanup */
	void* image;
	void* cvar;
	qboolean state;				/**< is node hovered */
	int border;					/**< border for this node - thickness in pixel - default 0 - also see bgcolor */
	vec4_t bgcolor;				/**< rgba */
	vec4_t bordercolor;			/**< rgba - see border and padding */

	/** @todo move it into window node, or think about a node way (i.e. Doom3 onTime) */
	int timeOut;				/**< ms value until calling onTimeOut (see cl.time) */
	int lastTime;				/**< when a menu was pushed this value is set to cl.time */

	/* common color */
	vec4_t color;				/**< rgba */
	vec4_t selectedColor;		/**< rgba The color to draw the line specified by textLineSelected in. */

	/* common events */
	struct menuAction_s *onClick;
	struct menuAction_s *onRightClick;
	struct menuAction_s *onMiddleClick;
	struct menuAction_s *onWheel;
	struct menuAction_s *onMouseEnter;
	struct menuAction_s *onMouseLeave;
	struct menuAction_s *onWheelUp;
	struct menuAction_s *onWheelDown;
	struct menuAction_s *onChange;	/**< called when the widget change from an user action */

	/* temporary, and/or for testing */
	float extraData1;			/**< allow behaviour to use it, how it need (before creating a real extradata structure) */

	/* text */
	/** @todo remove it  from 'string node', need to full implement MN_DrawStringInBox */
	byte longlines;				/**< what to do with long lines */
} menuNode_t;


/**
 * @brief Return extradata structure from a node
 * @param TYPE Extradata type
 * @param NODE Pointer to the node
 */
#define MN_EXTRADATA_POINTER(NODE, TYPE) ((TYPE*)((char*)NODE + sizeof(menuNode_t)))
#define MN_EXTRADATA(NODE, TYPE) (*MN_EXTRADATA_POINTER(NODE, TYPE))
#define MN_EXTRADATACONST_POINTER(NODE, TYPE) ((TYPE*)((const char*)NODE + sizeof(menuNode_t)))
#define MN_EXTRADATACONST(NODE, TYPE) (*MN_EXTRADATACONST_POINTER(NODE, const TYPE))

/**
 * @brief Return the offset of an extradata node attribute
 * @param TYPE Extradata type
 * @param MEMBER Attribute name
 * @sa offsetof
 */
#define MN_EXTRADATA_OFFSETOF(TYPE, MEMBER) ((size_t) &((TYPE *)(MN_EXTRADATA_POINTER(0, TYPE)))->MEMBER)

/**
 * @brief node behaviour, how a node work
 * @sa virtualFunctions
 */
typedef struct nodeBehaviour_s {
	/* behaviour attributes */
	const char* name;				/**< name of the behaviour: string type of a node */
	const char* extends;			/**< name of the extends behaviour */
	qboolean isVirtual;				/**< true, if the node doesn't have any position on the screen */
	qboolean isFunction;			/**< true, if the node is a function */
	qboolean isAbstract;			/**< true, if we can't instantiate the behaviour */
	qboolean isInitialized;			/**< cache if we already have initialized the node behaviour */
	qboolean focusEnabled;			/**< true if the node can win the focus (should be use when type TAB) */
	qboolean drawItselfChild;		/**< if true, the node draw function must draw child, the core code will not do it */
	const value_t* properties;		/**< list of properties of the node */
	int propertyCount;				/**< number of the properties into the propertiesList. Cache value to speedup search */
	intptr_t extraDataSize;			/**< Size of the extra data used (it come from "u" attribute) @note use intptr_t because we use the virtual inheritance function (see virtualFunctions) */
	struct nodeBehaviour_s *super;	/**< link to the extended node */
#ifdef DEBUG
	int count;						/**< number of node allocated */
#endif

	/* draw callback */
	void (*draw)(menuNode_t *node);							/**< how to draw a node */
	void (*drawTooltip)(menuNode_t *node, int x, int y);	/**< allow to draw a custom tooltip */
	void (*drawOverMenu)(menuNode_t *node);					/**< callback to draw content over the menu @sa MN_CaptureDrawOver */

	/* mouse events */
	void (*leftClick)(menuNode_t *node, int x, int y);		/**< left mouse click event in the node */
	void (*rightClick)(menuNode_t *node, int x, int y);		/**< right mouse button click event in the node */
	void (*middleClick)(menuNode_t *node, int x, int y);	/**< middle mouse button click event in the node */
	void (*mouseWheel)(menuNode_t *node, qboolean down, int x, int y);	/**< mouse wheel event in the node */
	void (*mouseMove)(menuNode_t *node, int x, int y);
	void (*mouseDown)(menuNode_t *node, int x, int y, int button);	/**< mouse button down event in the node */
	void (*mouseUp)(menuNode_t *node, int x, int y, int button);	/**< mouse button up event in the node */
	void (*capturedMouseMove)(menuNode_t *node, int x, int y);
	void (*capturedMouseLost)(menuNode_t *node);

	/* system allocation */
	void (*loading)(menuNode_t *node);		/**< called before script initialization, initialized default values */
	void (*loaded)(menuNode_t *node);		/**< only called one time, when node parsing was finished */
	void (*clone)(const menuNode_t *source, menuNode_t *clone);			/**< call to initialize a cloned node */
	void (*new)(menuNode_t *node);			/**< call to initialize a dynamic node */
	void (*delete)(menuNode_t *node);		/**< call to delete a dynamic node */

	/* system callback */
	void (*init)(menuNode_t *node);			/**< call every time we push the parent window */
	void (*close)(menuNode_t *node);		/**< call every time we pop the parent window */
	void (*doLayout)(menuNode_t *node);		/**< call to update node layout */
	void (*activate)(menuNode_t *node);		/**< Activate the node. Can be used without the mouse (ie. a button will execute onClick) */
	void (*propertyChanged)(menuNode_t *node, const value_t *property);		/**< Called when a property change */
	void (*sizeChanged)(menuNode_t *node);		/**< Called when the node size change */
	void (*getClientPosition)(menuNode_t *node, vec2_t position);	/**< Return the position of the client zone into the node */

	/* drag and drop callback */
	qboolean (*dndEnter)(menuNode_t *node);							/**< Send to the target when we enter first, return true if we can drop the DND somewhere on the node */
	qboolean (*dndMove)(menuNode_t *node, int x, int y);			/**< Send to the target when we enter first, return true if we can drop the DND here */
	void (*dndLeave)(menuNode_t *node);								/**< Send to the target when the DND is canceled */
	qboolean (*dndDrop)(menuNode_t *node, int x, int y);			/**< Send to the target to finalize the drop */
	qboolean (*dndFinished)(menuNode_t *node, qboolean isDroped);	/**< Sent to the source to finalize the drop */

	/* focus and keyboard events */
	void (*focusGained)(menuNode_t *node);
	void (*focusLost)(menuNode_t *node);
	qboolean (*keyPressed)(menuNode_t *node, unsigned int key, unsigned short unicode);

	/* Planned */
#if 0
	/* mouse move event */
	void (*mouseEnter)(menuNode_t *node);
	void (*mouseLeave)(menuNode_t *node);
#endif
} nodeBehaviour_t;

/* module initialization */
void MN_InitNodes(void);

/* nodes */
menuNode_t* MN_AllocNode(const char* name, const char* type, qboolean isDynamic);
menuNode_t* MN_GetNodeByPath(const char* path) __attribute__ ((warn_unused_result));
void MN_ReadNodePath(const char* path, const menuNode_t *relativeNode, menuNode_t** resultNode, const value_t **resultProperty);
struct menuNode_s *MN_GetNodeAtPosition(int x, int y) __attribute__ ((warn_unused_result));
const char* MN_GetPath(const menuNode_t* node) __attribute__ ((warn_unused_result));
struct menuNode_s *MN_CloneNode(const struct menuNode_s * node, struct menuNode_s *newMenu, qboolean recursive, const char *newName, qboolean isDynamic) __attribute__ ((warn_unused_result));
qboolean MN_CheckVisibility(menuNode_t *node);
void MN_DeleteAllChild(menuNode_t* node);
void MN_DeleteNode(menuNode_t* node);

/* behaviours */
nodeBehaviour_t* MN_GetNodeBehaviour(const char* name) __attribute__ ((warn_unused_result));
nodeBehaviour_t* MN_GetNodeBehaviourByIndex(int index) __attribute__ ((warn_unused_result));
int MN_GetNodeBehaviourCount(void) __attribute__ ((warn_unused_result));
const struct value_s *MN_GetPropertyFromBehaviour(const nodeBehaviour_t *behaviour, const char* name) __attribute__ ((warn_unused_result));

#endif
