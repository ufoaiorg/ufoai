#ifndef ORTHOCONTEXTMENU_H_
#define ORTHOCONTEXTMENU_H_

#include <gtk/gtk.h>

#include "radiant_i18n.h"
#include "math/Vector3.h"

namespace ui
{
	namespace
	{
		/* CONSTANTS */

		const char* LIGHT_CLASSNAME = "light";
		const char* MODEL_CLASSNAME = "misc_model";

		const char* ADD_MODEL_TEXT = _("Create model...");
		const char* ADD_MODEL_ICON = "cmenu_add_model.png";
		const char* ADD_LIGHT_TEXT = _("Create light...");
		const char* ADD_LIGHT_ICON = "cmenu_add_light.png";
		const char* ADD_ENTITY_TEXT = _("Create entity...");
		const char* ADD_ENTITY_ICON = "cmenu_add_entity.png";
		const char* CONNECT_ENTITIES_TEXT = _("Connect entities...");
		const char* CONNECT_ENTITIES_ICON = "cmenu_connect_entities.png";
		const char* FIT_TEXTURE_TEXT = _("Fit textures...");
		const char* FIT_TEXTURE_ICON = "cmenu_fit_texture.png";
	}

	/** Displays a menu when the mouse is right-clicked in the ortho window.
	 * This is a singleton class which remains in existence once constructed,
	 * and is hidden and displayed as appropriate.
	 */
	class OrthoContextMenu
	{
		private:

			// The GtkWidget representing the menu
			GtkWidget* _widget;

			// Last provided 3D point for action
			Vector3 _lastPoint;

			/** Enable or disables the "connect entities" option (two entities
			 * must be selected to connect them)
			 */
			void checkConnectEntities ();

			/** Enable or disables the "fit texture" option
			 */
			void checkFitTexture ();

			// GTK widgets that are context dependent
			GtkWidget* _connectEntities;
			GtkWidget* _fitTexture;

		private:

			// Enable or disable the "convert to static" option based on the number
			// of selected brushes.
			void checkConvertStatic ();

			/* Gtk Callbacks */

			static void callbackAddEntity (GtkMenuItem* item, OrthoContextMenu* self);
			static void callbackAddLight (GtkMenuItem* item, OrthoContextMenu* self);
			static void callbackAddModel (GtkMenuItem* item, OrthoContextMenu* self);
			static void callbackConnectEntities (GtkMenuItem* item, OrthoContextMenu* self);
			static void callbackFitTexture (GtkMenuItem* item, OrthoContextMenu* self);

		public:

			/** Constructor. Create the GTK content here.
			 */
			OrthoContextMenu ();

			/** Display the menu at the current mouse position, and act on the
			 * choice.
			 *
			 * @param point
			 * The point in 3D space at which the chosen operation should take
			 * place.
			 */
			void show (const Vector3& point);

			/** Static instance display function. Obtain the singleton instance and
			 * call its show() function.
			 *
			 * @param point
			 * The point in 3D space at which the chosen operation should take
			 * place.
			 */
			static void displayInstance (const Vector3& point);
	};
}

#endif /*ORTHOCONTEXTMENU_H_*/
