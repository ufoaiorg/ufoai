#include "BrushExportOBJ.h"
#include "radiant_i18n.h"
#include "gtkutil/dialog.h"
#include "gtkutil/filechooser.h"

/*	This exports the current selection into a WaveFrontOBJ file
 */
void export_selected (GtkWindow* mainWindow)
{
	// Obtain the path from a File Dialog Window
	gtkutil::FileChooser fileChooser(GTK_WIDGET(mainWindow), _("Save as Obj"), false, false, "obj", ".obj");
	const std::string path = fileChooser.display();
	if (path.empty())
		return;

	// Open the file
	TextFileOutputStream file(path);

	if (!file.failed()) {
		// Instantiate the Exporter of type CExportFormatWavefront
		CExporter<CExportFormatWavefront> exporter(file);
		// Traverse through the selection and export it to the file
		GlobalSelectionSystem().foreachSelected(exporter);
	} else {
		gtkutil::errorDialog(_("Unable to write to file"));
	}
}
