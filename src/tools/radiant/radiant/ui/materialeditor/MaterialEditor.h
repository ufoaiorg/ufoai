#ifndef MATERIALEDITOR_H_
#define MATERIALEDITOR_H_

#include <string>
#include <gtk/gtk.h>
#include "../common/MaterialDefinitionView.h"

namespace ui
{
	class MaterialEditor
	{
		private:

			MaterialDefinitionView _view;
			GtkWidget* _dialog;

		public:
			/**
			 * Shows the existing material definition and append new content to it.
			 * @param append The material definition string to append to the existing one
			 */
			MaterialEditor (const std::string& append);

			virtual ~MaterialEditor ();

			void show ();
	};
}

#endif /* MATERIALEDITOR_H_ */
