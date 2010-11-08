#ifndef MODELSELECTOR_H_
#define MODELSELECTOR_H_

#include <gtk/gtk.h>
#include <string>
#include "math/matrix.h"
#include "igl.h"
#include "imodel.h"
#include "../common/ModelPreview.h"
#include <map>

namespace ui
{

	typedef std::map<std::string, GtkTreeIter*> DirIterMap;

	/** Data structure containing both a model and a skin name, to be returned from
	 * the Model Selector.
	 */
	struct ModelAndSkin
	{
			// Model and skin strings
			std::string model;
			int skin;

			// Constructor
			ModelAndSkin (const std::string& m, const int& s) :
				model(m), skin(s)
			{
			}
	};

	/** Singleton class encapsulating the Model Selector dialog and methods required to display the
	 * dialog and retrieve the selected model.
	 */
	class ModelSelector
	{
		private:

			// Main dialog widget
			GtkWidget* _widget;

			// Model preview widget
			ModelPreview _modelPreview;

			// Tree store containing model names
			GtkTreeStore* _treeStore;

			// Currently-selected row in the tree store
			GtkTreeSelection* _selection;

			// List store to contain attributes and values for the selected model
			GtkListStore* _infoStore;

			// Last selected model, which will be returned by showAndBlock() once the
			// recursive main loop exits.
			std::string _lastModel;
			int _lastSkin;

			// Map between model directory names (e.g. "models/objects") and
			// a GtkTreeIter pointing to the equivalent row in the TreeModel. Subsequent
			// modelpaths with this directory will be added as children of this iter.

			DirIterMap _dirIterMap;

			// frees the DirIterMap
			~ModelSelector ();

		private:

			// Private constructor, creates GTK widgets
			ModelSelector ();

			// Show the dialog, called internally by chooseModel(). Return the selected model path
			ModelAndSkin showAndBlock ();

			// Helper function to construct the TreeView
			GtkWidget* createTreeView ();
			GtkWidget* createButtons ();
			GtkWidget* createPreviewAndInfoPanel ();

			// Initialise the GL widget, to avoid doing this every frame
			void initialisePreview ();

			// Update the info table with information from the currently-selected model
			// update the displayed model.
			void updateSelected ();

			// loads only the given directory
			void loadDirectory(const std::string& path);

			// Return the value from the selected column, or an empty string if nothing selected
			std::string getSelectedString (gint col);
			// Return the value from the selected column, or -1 if nothing was selected
			int getSelectedInteger (gint col);

			/* GTK CALLBACKS */
			static void callbackHide (GtkWidget*, GdkEvent*, ModelSelector*);
			static void callbackSelChanged (GtkWidget*, ModelSelector*);
			static void callbackOK (GtkWidget*, ModelSelector*);
			static void callbackCancel (GtkWidget*, ModelSelector*);

		public:

			/** Display the Model Selector instance, constructing it on first use, and return
			 * the VFS path of the model selected by the user. When the ModelSelector is displayed
			 * it will enter a recursive gtk_main loop, blocking execution of the calling
			 * function until destroyed.
			 */
			static ModelAndSkin chooseModel ();
	};
}

#endif /*MODELSELECTOR_H_*/
