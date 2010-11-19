#ifndef POPUPMENU_H_
#define POPUPMENU_H_

#include <gtk/gtkwidget.h>
#include <gtk/gtkmenuitem.h>
#include <list>

namespace gtkutil
{

	/**
	 * A free pop-up menu populated with items and displayed on demand. Useful for
	 * right-click context menus.
	 */
	class PopupMenu
	{
		public:

			typedef bool (*SensitivityTest)(gpointer);

		private:

			// Main menu widget
			GtkWidget* _menu;

			// Data class containing the elements of a menu item
			struct MenuItem
			{
					GtkWidget* widget;
					GFunc callback;
					gpointer userData;
					SensitivityTest test;

					MenuItem (GtkWidget* w, GFunc c, gpointer ud, SensitivityTest t) :
						widget(w), callback(c), userData(ud), test(t)
					{
					}
			};

			// List of menu items
			typedef std::list<MenuItem> MenuItemList;
			MenuItemList _menuItems;

		private:

			/*
			 * Default sensitivity test. Return true in all cases. If a menu item does
			 * not specify its own sensitivity test, this will be used to ensure the
			 * item is always visible.
			 */
			static bool _alwaysVisible (gpointer)
			{
				return true;
			}

			/* GTK CALLBACKS */

			// Main activation callback from GTK
			static void _onActivate (GtkMenuItem* item, MenuItem* menuItem)
			{
				menuItem->callback(menuItem->widget, menuItem->userData);
			}

			// Mouse click callback (if required)
			static gboolean _onClick (GtkWidget* w, GdkEventButton* e, PopupMenu* self);

		public:

			/**
			 * Default constructor.
			 *
			 * @param widget
			 * Optional widget for which this menu should be a right-click popup menu.
			 * If not set to NULL, the PopupMenu will connect to the
			 * button-release-event on this widget and automatically display itself
			 * when a right-click is detected.
			 */
			PopupMenu (GtkWidget* widget = NULL);

			/**
			 * Destructor.
			 */
			~PopupMenu ()
			{
				g_object_unref(_menu);
			}

			/**
			 * Add an item to this menu using a widget and callback function.
			 *
			 * @param widget
			 * The GtkWidget containing the displayed menu elements (i.e. icon and
			 * text).
			 *
			 * @param callback
			 * A callback function to be invoked when this menu item is activated.
			 *
			 * @param userData pointer to user data
			 *
			 * @param test SensitivityTest function object to determine whether this menu item is
			 * currently visible.
			 */
			void addItem (GtkWidget* widget, GFunc callback, gpointer userData, SensitivityTest test = _alwaysVisible);

			/**
			 * Show this menu. Each menu item's SensitivityTest will be invoked to
			 * determine whether it should be enabled or not, then the menu will be
			 * displayed.
			 */
			void show ();

			static bool Callback (gpointer userData) {
				return false;
			}
	};
} // namespace gtkutil

#endif /*POPUPMENU_H_*/
