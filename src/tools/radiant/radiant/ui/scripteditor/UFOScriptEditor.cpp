#include "UFOScriptEditor.h"
#include "radiant_i18n.h"
#include "imaterial.h"
#include "gtkutil/MultiMonitor.h"
#include "gtkutil/dialog.h"

namespace ui
{
	UFOScriptEditor::UFOScriptEditor (const std::string& ufoScriptName, const std::string& append)
	{
		_view.setUFOScriptFile(ufoScriptName);
		if (append.length() > 0)
			_view.append(append);
		_dialog = gtk_dialog_new_with_buttons(_("UFOScript Definition"), GlobalRadiant().getMainWindow(),
				GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, GTK_STOCK_OK,
				GTK_RESPONSE_ACCEPT, NULL);

		gtk_dialog_set_default_response(GTK_DIALOG(_dialog), GTK_RESPONSE_ACCEPT);

		gtk_container_set_border_width(GTK_CONTAINER(_dialog), 12);

		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(_dialog)->vbox), _view.getWidget());

		GdkRectangle rect = gtkutil::MultiMonitor::getMonitorForWindow(GlobalRadiant().getMainWindow());
		gtk_window_set_default_size(GTK_WINDOW(_dialog), gint(rect.width / 2), gint(2 * rect.height / 3));
	}

	UFOScriptEditor::~UFOScriptEditor ()
	{
	}

	void UFOScriptEditor::goToLine (int lineNumber)
	{
		_view.goToLine(lineNumber);
	}

	void UFOScriptEditor::show ()
	{
		gtk_widget_show_all(_dialog);

		// Show and block
		gint result = gtk_dialog_run(GTK_DIALOG(_dialog));

		gtk_widget_destroy(_dialog);

		if (result == GTK_RESPONSE_ACCEPT)
			_view.save();
	}

}
