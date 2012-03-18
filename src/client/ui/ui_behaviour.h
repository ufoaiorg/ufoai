/**
 * @file ui_behaviour.h
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

#ifndef CLIENT_UI_UI_BEHAVIOUR_H
#define CLIENT_UI_UI_BEHAVIOUR_H

#include "ui_nodes.h"

struct value_s;
struct uiBehaviour_s;
struct uiNode_s;

/**
 * @brief node behaviour, how a node work
 * @sa virtualFunctions
 */
typedef struct uiBehaviour_s {
	/* behaviour attributes */
	const char* name;				/**< name of the behaviour: string type of a node */
	const char* extends;			/**< name of the extends behaviour */
	qboolean registration;			/**< True if we can define the behavior with registration function */
	qboolean isVirtual;				/**< true, if the node doesn't have any position on the screen */
	qboolean isFunction;			/**< true, if the node is a function */
	qboolean isAbstract;			/**< true, if we can't instantiate the behaviour */
	qboolean isInitialized;			/**< cache if we already have initialized the node behaviour */
	qboolean focusEnabled;			/**< true if the node can win the focus (should be use when type TAB) */
	qboolean drawItselfChild;		/**< if true, the node draw function must draw child, the core code will not do it */

	const value_t** localProperties;	/**< list of properties of the node */
	int propertyCount;				/**< number of the properties into the propertiesList. Cache value to speedup search */
	intptr_t extraDataSize;			/**< Size of the extra data used (it come from "u" attribute) @note use intptr_t because we use the virtual inheritance function (see virtualFunctions) */
	struct uiBehaviour_s *super;	/**< link to the extended node */
#ifdef DEBUG
	int count;						/**< number of node allocated */
#endif

	/* draw callback */
	void (*draw)(struct uiNode_s *node);							/**< how to draw a node */
	void (*drawTooltip)(struct uiNode_s *node, int x, int y);	/**< allow to draw a custom tooltip */
	void (*drawOverWindow)(struct uiNode_s *node);					/**< callback to draw content over the window @sa UI_CaptureDrawOver */

	/* mouse events */
	void (*leftClick)(struct uiNode_s *node, int x, int y);		/**< left mouse click event in the node */
	void (*rightClick)(struct uiNode_s *node, int x, int y);		/**< right mouse button click event in the node */
	void (*middleClick)(struct uiNode_s *node, int x, int y);	/**< middle mouse button click event in the node */
	qboolean (*scroll)(struct uiNode_s *node, int deltaX, int deltaY);	/**< mouse wheel event in the node */
	void (*mouseMove)(struct uiNode_s *node, int x, int y);
	void (*mouseDown)(struct uiNode_s *node, int x, int y, int button);	/**< mouse button down event in the node */
	void (*mouseUp)(struct uiNode_s *node, int x, int y, int button);	/**< mouse button up event in the node */
	void (*capturedMouseMove)(struct uiNode_s *node, int x, int y);
	void (*capturedMouseLost)(struct uiNode_s *node);

	/* system allocation */
	void (*loading)(struct uiNode_s *node);		/**< called before script initialization, initialized default values */
	void (*loaded)(struct uiNode_s *node);		/**< only called one time, when node parsing was finished */
	void (*clone)(const struct uiNode_s *source, struct uiNode_s *clone);			/**< call to initialize a cloned node */
	void (*newNode)(struct uiNode_s *node);			/**< call to initialize a dynamic node */
	void (*deleteNode)(struct uiNode_s *node);		/**< call to delete a dynamic node */

	/* system callback */
	void (*windowOpened)(struct uiNode_s *node, linkedList_t *params);			/**< Invoked when the window is added to the rendering stack */
	void (*windowClosed)(struct uiNode_s *node);		/**< Invoked when the window is removed from the rendering stack */
	void (*doLayout)(struct uiNode_s *node);		/**< call to update node layout */
	void (*activate)(struct uiNode_s *node);		/**< Activate the node. Can be used without the mouse (ie. a button will execute onClick) */
	void (*propertyChanged)(struct uiNode_s *node, const value_t *property);		/**< Called when a property change */
	void (*sizeChanged)(struct uiNode_s *node);		/**< Called when the node size change */
	void (*getClientPosition)(const struct uiNode_s *node, vec2_t position);	/**< Return the position of the client zone into the node */

	/* drag and drop callback */
	qboolean (*dndEnter)(struct uiNode_s *node);							/**< Send to the target when we enter first, return true if we can drop the DND somewhere on the node */
	qboolean (*dndMove)(struct uiNode_s *node, int x, int y);			/**< Send to the target when we enter first, return true if we can drop the DND here */
	void (*dndLeave)(struct uiNode_s *node);								/**< Send to the target when the DND is canceled */
	qboolean (*dndDrop)(struct uiNode_s *node, int x, int y);			/**< Send to the target to finalize the drop */
	qboolean (*dndFinished)(struct uiNode_s *node, qboolean isDroped);	/**< Sent to the source to finalize the drop */

	/* focus and keyboard events */
	void (*focusGained)(struct uiNode_s *node);
	void (*focusLost)(struct uiNode_s *node);
	qboolean (*keyPressed)(struct uiNode_s *node, unsigned int key, unsigned short unicode);
	qboolean (*keyReleased)(struct uiNode_s *node, unsigned int key, unsigned short unicode);

	/* cell size */
	int (*getCellWidth)(struct uiNode_s *node);
	int (*getCellHeight)(struct uiNode_s *node);

	/* Planned */
#if 0
	/* mouse move event */
	void (*mouseEnter)(struct uiNode_s *node);
	void (*mouseLeave)(struct uiNode_s *node);
#endif
} uiBehaviour_t;

/**
 * @brief Signature of a function to bind a node method.
 */
typedef void (*uiNodeMethod_t)(struct uiNode_s* node, const struct uiCallContext_s *context);

/**
 * @brief Register a property to a behaviour.
 * It should not be used in the code
 * @param behaviour Target behaviour
 * @param name Name of the property
 * @param type Type of the property
 * @param pos position of the attribute (which store property memory) into the node structure
 * @param size size of the attribute (which store property memory) into the node structure
 * @see UI_RegisterNodeProperty
 * @see UI_RegisterExtradataNodeProperty
 * @return A link to the node property
 */
const struct value_s *UI_RegisterNodePropertyPosSize_(struct uiBehaviour_s *behaviour, const char* name, int type, size_t pos, size_t size);

/**
 * @brief Initialize a property
 * @param BEHAVIOUR behaviour Target behaviour
 * @param NAME Name of the property
 * @param TYPE Type of the property
 * @param OBJECTTYPE Object type containing the property
 * @param ATTRIBUTE Name of the attribute of the object containing data of the property
 */
#define UI_RegisterNodeProperty(BEHAVIOUR, NAME, TYPE, OBJECTTYPE, ATTRIBUTE) UI_RegisterNodePropertyPosSize_(BEHAVIOUR, NAME, TYPE, offsetof(OBJECTTYPE, ATTRIBUTE), MEMBER_SIZEOF(OBJECTTYPE, ATTRIBUTE))

/**
 * @brief Return the offset of an extradata node attribute
 * @param TYPE Extradata type
 * @param MEMBER Attribute name
 * @sa offsetof
 */
#define UI_EXTRADATA_OFFSETOF_(TYPE, MEMBER) ((size_t) &((TYPE *)(UI_EXTRADATA_POINTER(0, TYPE)))->MEMBER)

/**
 * @brief Initialize a property from extradata of node
 * @param BEHAVIOUR behaviour Target behaviour
 * @param NAME Name of the property
 * @param TYPE Type of the property
 * @param EXTRADATATYPE Object type containing the property
 * @param ATTRIBUTE Name of the attribute of the object containing data of the property
 */
#define UI_RegisterExtradataNodeProperty(BEHAVIOUR, NAME, TYPE, EXTRADATATYPE, ATTRIBUTE) UI_RegisterNodePropertyPosSize_(BEHAVIOUR, NAME, TYPE, UI_EXTRADATA_OFFSETOF_(EXTRADATATYPE, ATTRIBUTE), MEMBER_SIZEOF(EXTRADATATYPE, ATTRIBUTE))

/**
 * @brief Initialize a property which override an inherited property.
 * It is yet only used for the documentation.
 * @param BEHAVIOUR behaviour Target behaviour
 * @param NAME Name of the property
 */
#define UI_RegisterOveridedNodeProperty(BEHAVIOUR, NAME) ;

/**
 * @brief Register a node method to a behaviour.
 * @param behaviour Target behaviour
 * @param name Name of the property
 * @param function function to execute the node method
 * @return A link to the node property
 */
const struct value_s *UI_RegisterNodeMethod(struct uiBehaviour_s *behaviour, const char* name, uiNodeMethod_t function);

/**
 * @brief Return a property from a node behaviour
 * @return A property, else NULL if not found.
 */
const struct value_s *UI_GetPropertyFromBehaviour(const uiBehaviour_t *behaviour, const char* name) __attribute__ ((warn_unused_result));

/**
 * @brief Initialize a node behaviour memory, after registration, and before unsing it.
 * @param behaviour Behaviour to initialize
 */
void UI_InitializeNodeBehaviour(uiBehaviour_t* behaviour);

#endif
