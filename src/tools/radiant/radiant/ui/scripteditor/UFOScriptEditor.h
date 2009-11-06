#ifndef UFOSCRIPTEDITOR_H_
#define UFOSCRIPTEDITOR_H_

#include <string>
#include <gtk/gtk.h>
#include "../common/UFOScriptDefinitionView.h"

namespace ui
{
	class UFOScriptEditor
	{
		private:

			UFOScriptDefinitionView _view;
			// Main dialog widget
			GtkWidget* _dialog;

		public:

			UFOScriptEditor (const std::string& ufoScriptName, const std::string& append = "");

			virtual ~UFOScriptEditor ();

			void goToLine (int lineNumber);

			void show ();
	};
}

#endif /* UFOSCRIPTEDITOR_H_ */
