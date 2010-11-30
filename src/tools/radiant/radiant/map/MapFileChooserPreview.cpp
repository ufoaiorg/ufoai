#include "MapFileChooserPreview.h"

#include "imap.h"
#include "itextstream.h"
#include <gtk/gtkvbox.h>
#include "scenelib.h"
#include <gtk/gtktextview.h>
#include "gtkutil/ScrolledFrame.h"

namespace map
{
	MapFileChooserPreview::MapFileChooserPreview () :
		_mapResource(NULL), _previewContainer(gtk_vbox_new(FALSE, 0))
	{
		_preview.setSize(400);

		GtkWidget* vbox = gtk_vbox_new(FALSE, 0);

		gtk_box_pack_start(GTK_BOX(vbox), _preview, FALSE, FALSE, 0);

		gtk_box_pack_start(GTK_BOX(_previewContainer), vbox, TRUE, TRUE, 0);
	}

	MapFileChooserPreview::~MapFileChooserPreview ()
	{
		if (_mapResource)
			GlobalReferenceCache().release(_mapName);
		_mapResource = NULL;
	}

	GtkWidget* MapFileChooserPreview::getPreviewWidget ()
	{
		gtk_widget_show_all(_previewContainer);
		return _previewContainer;
	}

	/**
	 * Gets called whenever the user changes the file selection.
	 * Note: this method must call the setPreviewActive() method on the
	 * FileChooser class to indicate whether the widget is active or not.
	 */
	void MapFileChooserPreview::onFileSelectionChanged (const std::string& newFileName,
			gtkutil::FileChooser& fileChooser)
	{
		if (_mapResource)
			GlobalReferenceCache().release(_mapName);
		_mapResource = NULL;

		// Attempt to load file
		setMapName(newFileName);

		_preview.initialisePreview();
		gtk_widget_queue_draw(_preview);

		// Always have the preview active
		fileChooser.setPreviewActive(true);
	}

	bool MapFileChooserPreview::setMapName (const std::string& name)
	{
		_mapName = name;
		_mapResource = GlobalReferenceCache().capture(_mapName);

		if (_mapResource == NULL || !_mapResource->load()) {
			// NULLify the preview map root on failure
			_preview.setRootNode(NULL);
			_mapResource = NULL;
			return false;
		}

		// Get the node from the reosource
		scene::Node* root = _mapResource->getNode();

		GlobalSceneGraph().erase_root();
		GlobalSceneGraph().insert_root(*root);
		// Set the new rootnode
		_preview.setRootNode(root);

		return true;
	}

} // namespace map
