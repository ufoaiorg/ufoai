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

#include "filechooser.h"
#include "radiant_i18n.h"

#include "ifiletypes.h"

#include <list>
#include <vector>

#include "string/string.h"
#include "stream/stringstream.h"
#include "container/array.h"
#include "os/path.h"
#include "os/file.h"

#include "messagebox.h"

struct filetype_pair_t
{
		filetype_pair_t () :
			m_moduleName("")
		{
		}
		filetype_pair_t (const std::string& moduleName, filetype_t type) :
			m_moduleName(moduleName), m_type(type)
		{
		}
		const std::string& m_moduleName;
		filetype_t m_type;
};

class FileTypeList: public IFileTypeList
{
		struct filetype_copy_t
		{
				filetype_copy_t (const filetype_pair_t& other) :
					m_moduleName(other.m_moduleName), m_name(other.m_type.name), m_pattern(other.m_type.pattern)
				{
				}
				std::string m_moduleName;
				std::string m_name;
				std::string m_pattern;
		};

		typedef std::list<filetype_copy_t> Types;
		Types m_types;
	public:

		typedef Types::const_iterator const_iterator;
		const_iterator begin () const
		{
			return m_types.begin();
		}
		const_iterator end () const
		{
			return m_types.end();
		}

		std::size_t size () const
		{
			return m_types.size();
		}

		void addType (const std::string& moduleName, filetype_t type)
		{
			m_types.push_back(filetype_pair_t(moduleName, type));
		}
};

class GTKMasks
{
		const FileTypeList& m_types;
	public:
		std::vector<std::string> m_filters;
		std::vector<std::string> m_masks;

		GTKMasks (const FileTypeList& types) :
			m_types(types)
		{
			m_masks.reserve(m_types.size());
			for (FileTypeList::const_iterator i = m_types.begin(); i != m_types.end(); ++i)
				m_masks.push_back((*i).m_name + " <" + (*i).m_pattern + ">");

			m_filters.reserve(m_types.size());
			for (FileTypeList::const_iterator i = m_types.begin(); i != m_types.end(); ++i)
				m_filters.push_back((*i).m_pattern);
		}

		filetype_pair_t GetTypeForGTKMask (const std::string& mask) const
		{
			std::vector<std::string>::const_iterator j = m_masks.begin();
			for (FileTypeList::const_iterator i = m_types.begin(); i != m_types.end(); ++i, ++j) {
				if (mask == (*j)) {
					return filetype_pair_t((*i).m_moduleName, filetype_t((*i).m_name.c_str(), (*i).m_pattern.c_str()));
				}
			}
			return filetype_pair_t();
		}
};

static void file_dialog_update_preview (GtkFileChooser *file_chooser, gpointer data)
{
	GtkWidget *preview = GTK_WIDGET(data);
	char *filename = gtk_file_chooser_get_preview_filename(file_chooser);

	if (filename != NULL) {
		GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size(filename, 128, 128, NULL);

		gtk_image_set_from_pixbuf(GTK_IMAGE(preview), pixbuf);

		gtk_file_chooser_set_preview_widget_active(file_chooser, pixbuf != NULL);
		if (pixbuf)
			gdk_pixbuf_unref(pixbuf);

		g_free(filename);
	}
}

static char g_file_dialog_file[1024];

static const char* file_dialog_show (GtkWidget* parent, bool open, const std::string& title, const std::string& path,
		const std::string& pattern)
{
	FileTypeList typelist;
	GlobalFiletypes().getTypeList(pattern.empty() ? "*" : pattern, &typelist);
	std::string realTitle = title;
	GTKMasks masks(typelist);

	if (realTitle.empty())
		realTitle = open ? _("Open File") : _("Save File");

	GtkWidget* dialog;
	if (open) {
		dialog = gtk_file_chooser_dialog_new(realTitle.c_str(), GTK_WINDOW(parent), GTK_FILE_CHOOSER_ACTION_OPEN,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, (char const*) 0);
	} else {
		dialog = gtk_file_chooser_dialog_new(realTitle.c_str(), GTK_WINDOW(parent), GTK_FILE_CHOOSER_ACTION_SAVE,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, (char const*) 0);
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "unnamed");
	}

	// Set the Enter key to activate the default response
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	// we expect an actual path below, if the path is 0 we might crash
	if (!path.empty()) {
		ASSERT_MESSAGE(g_path_is_absolute(path.c_str()), "file_dialog_show: path not absolute: " << makeQuoted(path));

		if (strstr(g_file_dialog_file, path.c_str())) {
			gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), g_file_dialog_file);
		} else {
			gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), path.c_str());
		}
	}

	for (std::size_t i = 0; i < masks.m_filters.size(); ++i) {
		GtkFileFilter* filter = gtk_file_filter_new();
		gtk_file_filter_add_pattern(filter, masks.m_filters[i].c_str());
		gtk_file_filter_set_name(filter, masks.m_masks[i].c_str());
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	}

	// Add a final mask for All Files (*.*)
	GtkFileFilter* allFilter = gtk_file_filter_new();
	gtk_file_filter_add_pattern(allFilter, _("*.*"));
	gtk_file_filter_set_name(allFilter, _("All Files (*.*)"));
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), allFilter);

	GtkWidget *preview = gtk_image_new();
	gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(dialog), preview);
	g_signal_connect(GTK_FILE_CHOOSER(dialog), "update-preview",
			G_CALLBACK(file_dialog_update_preview), preview);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		strcpy(g_file_dialog_file, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)));

		if (pattern != "*") {
			GtkFileFilter* filter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(dialog));
			if (filter != 0) { // no filter set? some file-chooser implementations may allow the user to set no filter, which we treat as 'all files'
				std::string fileFilter = gtk_file_filter_get_name(filter);
				filetype_t type = masks.GetTypeForGTKMask(fileFilter).m_type;
				// last ext separator
				const std::string extension = os::getExtension(g_file_dialog_file);
				// no extension
				if (extension.empty()) {
					strcat(g_file_dialog_file, type.pattern.c_str() + 1);
				} else {
					strcpy(g_file_dialog_file + (extension.c_str() - g_file_dialog_file), type.pattern.c_str() + 2);
				}
			}
		}
	} else {
		g_file_dialog_file[0] = '\0';
	}

	gtk_widget_destroy(dialog);

	// don't return an empty filename
	if (g_file_dialog_file[0] == '\0')
		return NULL;

	return g_file_dialog_file;
}

/**
 * @brief Show a new file dialog with given parameters and returns the selected path in a clean manner.
 * @note this will pop up new dialogs if an existing file was chosen and it was chosen to not overwrite it in save mode.
 * @param parent parent window
 * @param open flag indicating whether to open a file or to save a file
 * @param title title to be shown in dialog
 * @param path root path to show on start
 * @param pattern filename pattern, actually a name for a registered file type
 * @return a path to a file, all directory separators will be replaced by '/' (unix style) or NULL if no file was chosen.
 */
const char* file_dialog (GtkWidget* parent, bool open, const std::string& title, const std::string& path,
		const std::string& pattern)
{
	for (;;) {
		const char* file = file_dialog_show(parent, open, title, path, pattern);

		if (open || file == 0 || !file_exists(file) || gtk_MessageBox(parent,
				_("The file specified already exists.\nDo you want to replace it?"), title, eMB_NOYES,
				eMB_ICONQUESTION) == eIDYES) {
			if (file == 0)
				return file;
			std::string cleaned = os::standardPath(file);
			strcpy(g_file_dialog_file, cleaned.c_str());
			return g_file_dialog_file;
		}
	}
}

#include "MultiMonitor.h"

namespace gtkutil
{

	FileChooser::FileChooser (GtkWidget* parent, const std::string& title, bool open, bool browseFolders,
			const std::string& pattern, const std::string& defaultExt) :
		_parent(parent), _dialog(NULL), _title(title), _pattern(pattern), _defaultExt(defaultExt), _open(open),
				_preview(NULL)
	{
		if (_pattern.empty()) {
			_pattern = "*";
		}

		if (_title.empty()) {
			_title = _open ? _("Open File") : _("Save File");
		}

		if (_open) {
			_dialog = gtk_file_chooser_dialog_new(_title.c_str(), GTK_WINDOW(_parent),
					browseFolders ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER : GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
		} else {
			_dialog = gtk_file_chooser_dialog_new(title.c_str(), GTK_WINDOW(_parent),
					browseFolders ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER : GTK_FILE_CHOOSER_ACTION_SAVE,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
		}

		// Set the Enter key to activate the default response
		gtk_dialog_set_default_response(GTK_DIALOG(_dialog), GTK_RESPONSE_ACCEPT);

		// Set position and modality of the dialog
		gtk_window_set_modal(GTK_WINDOW(_dialog), TRUE);
		gtk_window_set_position(GTK_WINDOW(_dialog), GTK_WIN_POS_CENTER_ON_PARENT);

		// Set the default size of the window
		GdkRectangle rect = gtkutil::MultiMonitor::getMonitorForWindow(GTK_WINDOW(_parent));

		gtk_window_set_default_size(GTK_WINDOW(_dialog), gint(rect.width / 2), gint(2 * rect.height / 3));

		// Add the filetype masks
		FileTypeList typelist;
		GlobalFiletypes().getTypeList(pattern.empty() ? "*" : pattern.c_str(), &typelist);
		GTKMasks masks(typelist);

		for (std::size_t i = 0; i < masks.m_filters.size(); ++i) {
			// Create a GTK file filter and add it to the chooser dialog
			GtkFileFilter* filter = gtk_file_filter_new();
			gtk_file_filter_add_pattern(filter, masks.m_filters[i].c_str());
			gtk_file_filter_set_name(filter, masks.m_masks[i].c_str());
			gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(_dialog), filter);
		}

		// Add a final mask for All Files (*.*)
		GtkFileFilter* allFilter = gtk_file_filter_new();
		gtk_file_filter_add_pattern(allFilter, _("*.*"));
		gtk_file_filter_set_name(allFilter, _("All Files (*.*)"));
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(_dialog), allFilter);
	}

	FileChooser::~FileChooser ()
	{
		// Destroy the dialog
		gtk_widget_destroy(_dialog);
	}

	void FileChooser::setCurrentPath (const std::string& path)
	{
		_path = os::standardPath(path);

		// Convert path to standard and set the folder in the dialog
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(_dialog), _path.c_str());
	}

	void FileChooser::setCurrentFile (const std::string& file)
	{
		_file = file;

		if (!_open) {
			gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(_dialog), _file.c_str());
		}
	}

	std::string FileChooser::getSelectedFileName ()
	{
		// Load the filename from the dialog
		const gchar* fileNameRaw = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(_dialog));
		if (!fileNameRaw)
			return "";

		std::string fileName = os::standardPath(fileNameRaw);

		// Append the default extension for save operations before checking overwrites
		if (!_open // save operation
				&& !fileName.empty() // valid filename
				&& !_defaultExt.empty() // non-empty default extension
				&& os::getExtension(fileName) != _defaultExt) // no default extension
		{
			fileName.append(_defaultExt);
		}

		gchar* converted = g_filename_to_utf8(fileName.c_str(), -1, 0, 0, 0);
		fileName = converted;
		g_free(converted);
		return fileName;
	}

	void FileChooser::attachPreview (Preview* preview)
	{
		if (_preview != NULL) {
			g_error("Error, preview already attached to FileChooser.\n");
			return;
		}

		_preview = preview;

		gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(_dialog), _preview->getPreviewWidget());

		g_signal_connect(G_OBJECT(_dialog), "update-preview", G_CALLBACK(onUpdatePreview), this);
	}

	std::string FileChooser::display ()
	{
		// Loop until break
		while (1) {
			// Display the dialog and return the selected filename, or ""
			std::string fileName("");

			if (gtk_dialog_run(GTK_DIALOG(_dialog)) == GTK_RESPONSE_ACCEPT) {
				// "OK" pressed, retrieve the filename
				fileName = getSelectedFileName();
			}

			// Always return the fileName for "open" and empty filenames, otherwise check file existence
			if (_open || fileName.empty()) {
				return fileName;
			}

			// If file exists, ask for overwrite
			std::string askTitle = _title;
			askTitle += (!fileName.empty()) ? ": " + os::getFilenameFromPath(fileName) : "";

			if (!file_exists(fileName) || gtk_MessageBox(_parent,
					_("The specified file already exists.\nDo you want to replace it?"), askTitle.c_str(),
					eMB_NOYES, eMB_ICONQUESTION) == eIDYES) {
				return fileName;
			}
		}
	}

	void FileChooser::setPreviewActive (bool active)
	{
		// Pass the call to the dialog
		gtk_file_chooser_set_preview_widget_active(GTK_FILE_CHOOSER(_dialog), active ? TRUE : FALSE);
	}

	void FileChooser::onUpdatePreview (GtkFileChooser* chooser, FileChooser* self)
	{
		// Check if we have a valid preview object attached
		if (self->_preview == NULL)
			return;

		char* sel = gtk_file_chooser_get_preview_filename(GTK_FILE_CHOOSER(self->_dialog));

		std::string previewFileName = (sel != NULL) ? sel : "";

		g_free(sel);

		previewFileName = os::standardPath(previewFileName);

		// Emit the signal
		self->_preview->onFileSelectionChanged(previewFileName, *self);
	}

} // namespace gtkutil
