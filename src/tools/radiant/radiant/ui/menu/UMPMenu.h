#ifndef UMPMENU_H_
#define UMPMENU_H_

#include <glib.h>

typedef struct _GtkMenuItem GtkMenuItem;

namespace ui {

/** Utility class for generating the UMP menu. This class
 * registers the relevant menuitems on demand.
 */
class UMPMenu
{
	public:
		static void _activate (GtkMenuItem* menuItem, gpointer name);

		/** Adds the menuitems to the UIManager
		 */
		static void addItems ();

};

}

#endif /*UMPMENU_H_*/
