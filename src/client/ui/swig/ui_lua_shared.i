/*
	@file Interface file for SWIG to generarte lua ui binding.
*/

/* expose the ui code using a lua table ufoui */
%module ufoui

%{
/* import headers into the interface so they can be used */
#include <typeinfo>

/* import common functions */
#include "../../../shared/shared.h"

/* import ui specific functions */
#include "../ui_main.h"
#include "../ui_behaviour.h"
#include "../ui_nodes.h"
#include "../ui_node.h"
%}

/* expose common print function */
void Com_Printf(const char* fmt, ...);

/* expose node structure */
typedef uiNode_t ufoUiNode;

