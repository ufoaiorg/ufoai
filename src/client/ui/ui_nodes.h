/**
 * @file ui_nodes.h
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

#ifndef CLIENT_UI_UI_NODES_H
#define CLIENT_UI_UI_NODES_H

#include "../../shared/ufotypes.h"
#include "../../common/scripts.h"

/* prototype */
struct uiIcon_s;
struct value_s;
struct nodeKeyBinding_s;
struct uiCallContext_s;
struct uiNode_s;
struct uiModel_s;

/* exclude rect */
#define UI_MAX_EXLUDERECTS	64

typedef struct uiExcludeRect_s {
	vec2_t pos, size;
} uiExcludeRect_t;

typedef void (*uiNodeMethod_t)(struct uiNode_s*node, const struct uiCallContext_s *context);

/**
 * @brief Atomic structure used to define most of the UI
 */
typedef struct uiNode_s {
	/* common identification */
	char name[MAX_VAR];			/**< name from the script files */
	struct uiBehaviour_s *behaviour;
	struct uiNode_s *super;	/**< Node inherited, else NULL */
	qboolean dynamic;			/** If true, it use dynamic memory */
	qboolean indexed;			/** If true, the node name indexed into his window */

	/* common navigation */
	struct uiNode_s *firstChild;	/**< first element of linked list of child */
	struct uiNode_s *lastChild;		/**< last element of linked list of child */
	struct uiNode_s *next;			/**< Next element into linked list */
	struct uiNode_s *parent;		/**< Parent window, else NULL */
	struct uiNode_s *root;			/**< Shortcut to the root node */

	/* common pos */
	vec2_t pos;
	vec2_t size;

	/* common attributes */
	const char* tooltip;		/**< holds the tooltip */
	struct uiKeyBinding_s *key;	/**< key bindings - used as tooltip */
	qboolean invis;				/**< true if the node is invisible */
	qboolean disabled;			/**< true if the node is inactive */
	qboolean invalidated;		/**< true if we need to update the layout */
	qboolean ghost;				/**< true if the node is not tangible */
	qboolean state;				/**< is node hovered */
	int padding;				/**< padding for this node - default 3 - see bgcolor */
	int align;					/**< used to identify node position into a parent using a layout manager. Else it do nothing. */
	int num;					/**< used to identify child into a parent; not sure it is need @todo delete it */
	struct uiAction_s* visibilityCondition;	/**< cvar condition to display/hide the node */

	/** @todo use a linked list of excluderect? */
	uiExcludeRect_t *excludeRect;	/**< exclude this for hover or click functions */
	int excludeRectNum;			/**< how many consecutive exclude rects defined? */

	/* other attributes */
	/** @todo needs cleanup */
	align_t textalign;				/**< Alignment to draw text */
	char* text;					/**< Text we want to display */
	const char* font;			/**< Font to draw text */
	const char* image;
	int border;					/**< border for this node - thickness in pixel - default 0 - also see bgcolor */
	vec4_t bgcolor;				/**< rgba */
	vec4_t bordercolor;			/**< rgba - see border and padding */
	vec4_t color;				/**< rgba */
	vec4_t selectedColor;		/**< rgba The color to draw the line specified by textLineSelected in. */

	/* common events */
	struct uiAction_s *onClick;
	struct uiAction_s *onRightClick;
	struct uiAction_s *onMiddleClick;
	struct uiAction_s *onWheel;
	struct uiAction_s *onMouseEnter;
	struct uiAction_s *onMouseLeave;
	struct uiAction_s *onWheelUp;
	struct uiAction_s *onWheelDown;
	struct uiAction_s *onChange;	/**< called when the widget change from an user action */
} uiNode_t;


/**
 * @brief Return extradata structure from a node
 * @param TYPE Extradata type
 * @param NODE Pointer to the node
 */
#define UI_EXTRADATA_POINTER(NODE, TYPE) ((TYPE*)((char*)NODE + sizeof(uiNode_t)))
#define UI_EXTRADATA(NODE, TYPE) (*UI_EXTRADATA_POINTER(NODE, TYPE))
#define UI_EXTRADATACONST_POINTER(NODE, TYPE) ((TYPE*)((const char*)NODE + sizeof(uiNode_t)))
#define UI_EXTRADATACONST(NODE, TYPE) (*UI_EXTRADATACONST_POINTER(NODE, const TYPE))

/**
 * @brief Return the offset of an extradata node attribute
 * @param TYPE Extradata type
 * @param MEMBER Attribute name
 * @sa offsetof
 */
#define UI_EXTRADATA_OFFSETOF(TYPE, MEMBER) ((size_t) &((TYPE *)(UI_EXTRADATA_POINTER(0, TYPE)))->MEMBER)

/**
 * @brief node behaviour, how a node work
 * @sa virtualFunctions
 */
typedef struct uiBehaviour_s {
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
	struct uiBehaviour_s *super;	/**< link to the extended node */
#ifdef DEBUG
	int count;						/**< number of node allocated */
#endif

	/* draw callback */
	void (*draw)(uiNode_t *node);							/**< how to draw a node */
	void (*drawTooltip)(uiNode_t *node, int x, int y);	/**< allow to draw a custom tooltip */
	void (*drawOverWindow)(uiNode_t *node);					/**< callback to draw content over the window @sa UI_CaptureDrawOver */

	/* mouse events */
	void (*leftClick)(uiNode_t *node, int x, int y);		/**< left mouse click event in the node */
	void (*rightClick)(uiNode_t *node, int x, int y);		/**< right mouse button click event in the node */
	void (*middleClick)(uiNode_t *node, int x, int y);	/**< middle mouse button click event in the node */
	void (*mouseWheel)(uiNode_t *node, qboolean down, int x, int y);	/**< mouse wheel event in the node */
	void (*mouseMove)(uiNode_t *node, int x, int y);
	void (*mouseDown)(uiNode_t *node, int x, int y, int button);	/**< mouse button down event in the node */
	void (*mouseUp)(uiNode_t *node, int x, int y, int button);	/**< mouse button up event in the node */
	void (*capturedMouseMove)(uiNode_t *node, int x, int y);
	void (*capturedMouseLost)(uiNode_t *node);

	/* system allocation */
	void (*loading)(uiNode_t *node);		/**< called before script initialization, initialized default values */
	void (*loaded)(uiNode_t *node);		/**< only called one time, when node parsing was finished */
	void (*clone)(const uiNode_t *source, uiNode_t *clone);			/**< call to initialize a cloned node */
	void (*newNode)(uiNode_t *node);			/**< call to initialize a dynamic node */
	void (*deleteNode)(uiNode_t *node);		/**< call to delete a dynamic node */

	/* system callback */
	void (*init)(uiNode_t *node);			/**< call every time we push the parent window */
	void (*close)(uiNode_t *node);		/**< call every time we pop the parent window */
	void (*doLayout)(uiNode_t *node);		/**< call to update node layout */
	void (*activate)(uiNode_t *node);		/**< Activate the node. Can be used without the mouse (ie. a button will execute onClick) */
	void (*propertyChanged)(uiNode_t *node, const value_t *property);		/**< Called when a property change */
	void (*sizeChanged)(uiNode_t *node);		/**< Called when the node size change */
	void (*getClientPosition)(uiNode_t *node, vec2_t position);	/**< Return the position of the client zone into the node */

	/* drag and drop callback */
	qboolean (*dndEnter)(uiNode_t *node);							/**< Send to the target when we enter first, return true if we can drop the DND somewhere on the node */
	qboolean (*dndMove)(uiNode_t *node, int x, int y);			/**< Send to the target when we enter first, return true if we can drop the DND here */
	void (*dndLeave)(uiNode_t *node);								/**< Send to the target when the DND is canceled */
	qboolean (*dndDrop)(uiNode_t *node, int x, int y);			/**< Send to the target to finalize the drop */
	qboolean (*dndFinished)(uiNode_t *node, qboolean isDroped);	/**< Sent to the source to finalize the drop */

	/* focus and keyboard events */
	void (*focusGained)(uiNode_t *node);
	void (*focusLost)(uiNode_t *node);
	qboolean (*keyPressed)(uiNode_t *node, unsigned int key, unsigned short unicode);

	/* Planned */
#if 0
	/* mouse move event */
	void (*mouseEnter)(uiNode_t *node);
	void (*mouseLeave)(uiNode_t *node);
#endif
} uiBehaviour_t;

/* module initialization */
void UI_InitNodes(void);

/* nodes */
uiNode_t* UI_AllocNode(const char* name, const char* type, qboolean isDynamic);
uiNode_t* UI_GetNodeByPath(const char* path) __attribute__ ((warn_unused_result));
void UI_ReadNodePath(const char* path, const uiNode_t *relativeNode, uiNode_t** resultNode, const value_t **resultProperty);
struct uiNode_s *UI_GetNodeAtPosition(int x, int y) __attribute__ ((warn_unused_result));
const char* UI_GetPath(const uiNode_t* node) __attribute__ ((warn_unused_result));
struct uiNode_s *UI_CloneNode(const struct uiNode_s * node, struct uiNode_s *newWindow, qboolean recursive, const char *newName, qboolean isDynamic) __attribute__ ((warn_unused_result));
qboolean UI_CheckVisibility(uiNode_t *node);
void UI_DeleteAllChild(uiNode_t* node);
void UI_DeleteNode(uiNode_t* node);

/* behaviours */
uiBehaviour_t* UI_GetNodeBehaviour(const char* name) __attribute__ ((warn_unused_result));
uiBehaviour_t* UI_GetNodeBehaviourByIndex(int index) __attribute__ ((warn_unused_result));
int UI_GetNodeBehaviourCount(void) __attribute__ ((warn_unused_result));
const struct value_s *UI_GetPropertyFromBehaviour(const uiBehaviour_t *behaviour, const char* name) __attribute__ ((warn_unused_result));

#endif
