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
struct uiNode_t {
	/* values that are read only accessible from lua */
	%immutable;
	char name[MAX_VAR];			/**< name from the script files */

	/* values that are read/write accessible from lua */
	%mutable;
};

