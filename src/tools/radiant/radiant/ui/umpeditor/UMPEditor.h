#ifndef UMPEDITOR_H_
#define UMPEDITOR_H_

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

#endif /* UMPEDITOR_H_ */
