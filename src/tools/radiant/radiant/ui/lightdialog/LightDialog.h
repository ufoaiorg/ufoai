#ifndef LIGHTDIALOG_H_
#define LIGHTDIALOG_H_

#include <gtk/gtkwidget.h>
#include <string>
#include "radiant_i18n.h"
#include "gtkutil/TextPanel.h"
#include "gtkutil/LeftAlignedLabel.h"
#include "math/Vector3.h"

namespace ui
{

	/**
	 * Modal dialog for selecting the light intensity and color
	 */
	class LightDialog
	{
			// Main dialog widget
			GtkWidget* _widget;

			// Color selection widget
			GtkColorSelection* _colorSelector;

			// Intensity widget
			gtkutil::TextPanel _intensityEntry;

			Vector3 _color;

			std::string _intensity;

			bool _aborted;

		private:

			// Widget construction
			GtkWidget* createColorSelector ();
			GtkWidget* createIntensity ();
			GtkWidget* createButtons ();

			/* GTK CALLBACKS */
			static gboolean _onDelete (GtkWidget* w, GdkEvent* e, LightDialog* self);
			static void _onOK (GtkWidget*, LightDialog*);
			static void _onCancel (GtkWidget*, LightDialog*);

			Vector3 getNormalizedColor ();

		public:

			/**
			 * Constructor creates widgets.
			 */
			LightDialog ();

			/**
			 * Destroys the created widgets.
			 */
			~LightDialog ();

			/**
			 * Show and block
			 */
			void show ();

			/**
			 * @return The selected color as RGB. This is only set if the dialog was closed with
			 * a click to the ok button, and not if you close it or hit the cancel button
			 */
			Vector3 getColor ();

			/**
			 * @return The intensity for the light. This is only set if the dialog was closed with
			 * a click to the ok button, and not if you close it or hit the cancel button
			 */
			std::string getIntensity ();

			/**
			 * @return true if the dialog was aborted by hitting the cancel button
			 */
			bool isAborted ();
	};
}

#endif /*LIGHTDIALOG_H_*/
