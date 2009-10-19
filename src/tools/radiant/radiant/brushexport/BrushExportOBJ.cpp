#include "BrushExportOBJ.h"
#include "radiant_i18n.h"
#include "gtkutil/dialog.h"

/*	This exports the current selection into a WaveFrontOBJ file
 */
void export_selected (GtkWindow* mainWindow)
{
	// Obtain the path from a File Dialog Window
	const char* path = GlobalRadiant().m_pfnFileDialog(GTK_WIDGET(mainWindow), false, _("Save as Obj"), "", "");

	// Open the file
	TextFileOutputStream file(path);

	if (!file.failed()) {
		// Instantiate the Exporter of type CExportFormatWavefront
		CExporter<CExportFormatWavefront> exporter(file);
		// Traverse through the selection and export it to the file
		GlobalSelectionSystem().foreachSelected(exporter);
	} else {
		gtkutil::errorDialog(mainWindow, _("Unable to write to file"));
	}
}
