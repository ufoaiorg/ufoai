#include "../../dialog.h"
#include "../../environment.h"

namespace ui {

class PathsDialog: public Dialog
{
	public:

		PathsDialog ()
		{
			Create();
			DoModal();
			Destroy();
		}

		void PostModal (EMessageBoxReturn code)
		{
			if (code == eIDOK) {
				_registryConnector.exportValues();
			}
		}

		GtkWindow* BuildDialog ()
		{
			GtkFrame* frame = create_dialog_frame(_("Path Settings"), GTK_SHADOW_ETCHED_IN);

			GtkVBox* vbox2 = create_dialog_vbox(0, 4);
			gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(vbox2));

			// FIXME: use GlobalGameManager().getKeyValue("name")
			addPathEntry(GTK_WIDGET(vbox2), _("UFO:AI Installation Path"), RKEY_ENGINE_PATH, true);

			return create_simple_modal_dialog_window(_("Engine Path Not Found"), m_modal, GTK_WIDGET(frame));
		}
};

} // namespace ui
