#pragma once

#include "ifc/EditorWidget.h"

namespace gtkutil
{

	class TextPanel: public EditorWidget
	{
			// Main widget
			GtkWidget* _widget;

		protected:

			/* gtkutil::Widget implementation */
			GtkWidget* _getWidget () const;

		public:

			/**
			 * Construct a TextPanel.
			 */
			TextPanel ();

			/**
			 * Destroy this TextPanel including all widgets.
			 */
			~TextPanel ();

			/* gtkutil::EditorWidget implementation */
			void setValue (const std::string& value);
			std::string getValue () const;
	};
}
