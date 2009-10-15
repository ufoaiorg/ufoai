#ifndef TEXTPANEL_H_
#define TEXTPANEL_H_

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

#endif /*TEXTPANEL_H_*/
