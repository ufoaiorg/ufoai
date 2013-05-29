#pragma once

#include <string>
#include "gtkutil/SourceView.h"
typedef struct _GtkWidget GtkWidget;

namespace ui
{
	class MaterialDefinitionView
	{
			// The material which should be previewed
			std::string _material;

			// The top-level widget
			GtkWidget* _vbox;

			GtkWidget* _materialName;
			GtkWidget* _filename;

			// The actual code view
			gtkutil::SourceView _view;

		public:
			MaterialDefinitionView ();

			// Returns the topmost widget for packing this view into a parent container
			GtkWidget* getWidget ();

			void setMaterial (const std::string& material);

			void update ();

			void append (const std::string& append);

			void save ();
	};

} // namespace ui
