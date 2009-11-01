#ifndef MATERIAL_DEFINITION_VIEW_H_
#define MATERIAL_DEFINITION_VIEW_H_

#include <string>
#include "gtkutil/SourceView.h"
typedef struct _GtkWidget GtkWidget;

namespace ui
{
	class UMPDefinitionView
	{
			// The ump file that should be shown
			std::string _umpFile;

			// The top-level widget
			GtkWidget* _vbox;

			GtkWidget* _umpFileName;

			// The actual code view
			gtkutil::SourceView _view;

		public:
			UMPDefinitionView ();

			// Returns the topmost widget for packing this view into a parent container
			GtkWidget* getWidget ();

			void setUMPFile (const std::string& umpFile);

			void update ();

			void append (const std::string& append);

			void save ();
	};

} // namespace ui

#endif /* MATERIAL_DEFINITION_VIEW_H_ */
