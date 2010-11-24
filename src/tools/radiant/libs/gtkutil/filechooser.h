/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef INCLUDED_GTKUTIL_FILECHOOSER_H
#define INCLUDED_GTKUTIL_FILECHOOSER_H

#include <string>
#include "radiant_i18n.h"

typedef struct _GtkWidget GtkWidget;

namespace gtkutil
{
	class FileChooser
	{
		public:
			/**
			 * greebo: A Preview class can be attached to a FileChooser (in "open" mode),
			 * to allow for adding and updating a preview widget to the dialog.
			 * The Preview object must provide two methods, one for retrieving
			 * the preview widget for addition to the dialog, and one
			 * update method which gets called as soon as the dialog emits the
			 * selection change signal.
			 */
			class Preview
			{
				public:
					virtual ~Preview() {}

					// Retrieve the preview widget for packing into the dialog
					virtual GtkWidget* getPreviewWidget () = 0;

					/**
					 * Gets called whenever the user changes the file selection.
					 * Note: this method must call the setPreviewActive() method on the
					 * FileChooser class to indicate whether the widget is active or not.
					 */
					virtual void onFileSelectionChanged (const std::string& newFileName, FileChooser& fileChooser) = 0;
			};

		private:
			// Parent widget
			GtkWidget* _parent;

			GtkWidget* _dialog;

			// Window title
			std::string _title;

			std::string _path;
			std::string _file;

			std::string _pattern;

			std::string _defaultExt;

			// Open or save dialog
			bool _open;

			// The optional preview object
			Preview* _preview;

		public:
			/**
			 * Construct a new filechooser with the given parameters.
			 *
			 * @param parent The parent GtkWidget
			 * @param title The dialog title.
			 * @param open if TRUE this is asking for "Open" files, FALSE generates a "Save" dialog.
			 * @param browseFolders if TRUE the dialog is asking the user for directories only.
			 * @param pattern the type "map", "prefab", this determines the file extensions.
			 * @param defaultExt The default extension appended when the user enters
			 *              filenames without extension. (Including the dot as seperator character.)
			 */
			FileChooser (GtkWidget* parent, const std::string& title, bool open, bool browseFolders,
					const std::string& pattern = "", const std::string& defaultExt = "");

			virtual ~FileChooser ();

			// Lets the dialog start at a certain path
			void setCurrentPath (const std::string& path);

			// Pre-fills the currently selected file
			void setCurrentFile (const std::string& file);

			/**
			 * FileChooser in "open" mode (see constructor) can have one
			 * single preview attached to it. The Preview object will
			 * get notified on selection changes to update the widget it provides.
			 */
			void attachPreview (Preview* preview);

			/**
			 * Returns the selected filename (default extension
			 * will be added if appropriate).
			 */
			virtual std::string getSelectedFileName ();

			/**
			 * greebo: Displays the dialog and enters the GTK main loop.
			 * Returns the filename or "" if the user hit cancel.
			 *
			 * The returned file name is normalised using the os::standardPath() method.
			 */
			virtual std::string display ();

			// Public function for Preview objects. These must set the "active" state
			// of the preview when the onFileSelectionChange() signal is emitted.
			void setPreviewActive (bool active);

		private:
			// GTK callback for updating the preview widget
			static void onUpdatePreview (GtkFileChooser* chooser, FileChooser* self);
	};

} // namespace gtkutil


#endif
