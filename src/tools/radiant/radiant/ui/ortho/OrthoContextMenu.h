#ifndef ORTHOCONTEXTMENU_H_
#define ORTHOCONTEXTMENU_H_

#include <gtk/gtk.h>

#include "math/Vector3.h"

namespace ui
{
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

			/** Enables or disables the "connect entities" option (two entities
			 * must be selected to connect them)
			 */
			void checkConnectEntities ();

			/** Enables or disables the "fit texture" option
			 */
			void checkFitTexture ();

			/** Enables or disables the "generate material" option
			 */
			void checkGenerateMaterial ();

			/** Enables or disables the "generate terrain" option
			 */
			void checkGenerateTerrain ();

			/** Enables or disables the "flip texture" option
			 */
			void checkFlipTexture ();

			// GTK widgets that are context dependent
			GtkWidget* _connectEntities;
			GtkWidget* _fitTexture;
			GtkWidget* _flipTextureX;
			GtkWidget* _flipTextureY;
			GtkWidget* _generateMaterials;
			GtkWidget* _generateTerrain;

		private:

			// Enable or disable the "convert to static" option based on the number
			// of selected brushes.
			void checkConvertStatic ();

			/* Gtk Callbacks */

			static void callbackAddEntity (GtkMenuItem* item, OrthoContextMenu* self);
			static void callbackAddLight (GtkMenuItem* item, OrthoContextMenu* self);
			static void callbackAddModel (GtkMenuItem* item, OrthoContextMenu* self);
			static void callbackGenerateMaterials (GtkMenuItem* item, OrthoContextMenu* self);
			static void callbackGenerateTerrain (GtkMenuItem* item, OrthoContextMenu* self);
			static void callbackConnectEntities (GtkMenuItem* item, OrthoContextMenu* self);
			static void callbackFitTexture (GtkMenuItem* item, OrthoContextMenu* self);
			static void callbackAddSound (GtkMenuItem* item, OrthoContextMenu* self);
			static void callbackFlipXTexture (GtkMenuItem* item, OrthoContextMenu* self);
			static void callbackFlipYTexture (GtkMenuItem* item, OrthoContextMenu* self);

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
