#pragma once

#include <string>
#include <gtk/gtk.h>
#include "../common/UMPDefinitionView.h"

namespace ui
{
	class UMPEditor
	{
		private:

			UMPDefinitionView _view;
			// Main dialog widget
			GtkWidget* _dialog;

		public:

			UMPEditor (const std::string& umpName);

			virtual ~UMPEditor ();

			void show ();
	};
}
