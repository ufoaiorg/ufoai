#ifndef UFO_SCRIPT_DEFINITION_VIEW_H_
#define UFO_SCRIPT_DEFINITION_VIEW_H_

#include <string>
#include "gtkutil/SourceView.h"
typedef struct _GtkWidget GtkWidget;

namespace ui
{
	class UFOScriptDefinitionView
	{
			// The ump file that should be shown
			std::string _ufoFile;

			// The top-level widget
			GtkWidget* _vbox;

			GtkWidget* _ufoFileName;

			// The actual code view
			gtkutil::SourceView _view;

		public:
			UFOScriptDefinitionView ();

			// Returns the topmost widget for packing this view into a parent container
			GtkWidget* getWidget ();

			void setUFOScriptFile (const std::string& ufoFile);

			void goToLine (int lineNumber);

			void update ();

			void append (const std::string& append);

			void save ();
	};

} // namespace ui

#endif /* UFO_SCRIPT_DEFINITION_VIEW_H_ */
