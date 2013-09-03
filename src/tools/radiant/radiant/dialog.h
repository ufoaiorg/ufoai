/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

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

#if !defined(INCLUDED_DIALOG_H)
#define INCLUDED_DIALOG_H

#include <list>

#include "gtkutil/dialog.h"
#include "gtkutil/RegistryConnector.h"
#include "generic/callback.h"
#include "string/string.h"

template<typename Environment, typename FirstArgument, void(*func) (Environment&, FirstArgument)>
class ReferenceCaller1;

inline void BoolImport (bool& self, bool value)
{
	self = value;
}
typedef ReferenceCaller1<bool, bool, BoolImport> BoolImportCaller;

inline void BoolExport (bool& self, const BoolImportCallback& importCallback)
{
	importCallback(self);
}
typedef ReferenceCaller1<bool, const BoolImportCallback&, BoolExport> BoolExportCaller;

inline void IntImport (int& self, int value)
{
	self = value;
}
typedef ReferenceCaller1<int, int, IntImport> IntImportCaller;

inline void IntExport (int& self, const IntImportCallback& importCallback)
{
	importCallback(self);
}
typedef ReferenceCaller1<int, const IntImportCallback&, IntExport> IntExportCaller;

inline void SizeImport (std::size_t& self, std::size_t value)
{
	self = value;
}
typedef ReferenceCaller1<std::size_t, std::size_t, SizeImport> SizeImportCaller;

inline void SizeExport (std::size_t& self, const SizeImportCallback& importCallback)
{
	importCallback(self);
}
typedef ReferenceCaller1<std::size_t, const SizeImportCallback&, SizeExport> SizeExportCaller;

inline void FloatImport (float& self, float value)
{
	self = value;
}
typedef ReferenceCaller1<float, float, FloatImport> FloatImportCaller;

inline void FloatExport (float& self, const FloatImportCallback& importCallback)
{
	importCallback(self);
}
typedef ReferenceCaller1<float, const FloatImportCallback&, FloatExport> FloatExportCaller;

inline void StringImport (std::string& self, const char* value)
{
	self = value;
}
typedef ReferenceCaller1<std::string, const char*, StringImport> StringImportCaller;
inline void StringExport (std::string& self, const StringImportCallback& importCallback)
{
	importCallback(self.c_str());
}
typedef ReferenceCaller1<std::string, const StringImportCallback&, StringExport> StringExportCaller;

typedef struct _GtkWindow GtkWindow;
typedef struct _GtkToggleButton GtkToggleButton;
typedef struct _GtkRadioButton GtkRadioButton;
typedef struct _GtkSpinButton GtkSpinButton;
typedef struct _GtkComboBox GtkComboBox;
typedef struct _GtkEntry GtkEntry;
typedef struct _GtkAdjustment GtkAdjustment;

typedef std::list<std::string> ComboBoxValueList;

class Dialog
{
	private:
		GtkWindow* m_window;

		static void onTextureSelect (GtkWidget* button, GtkEntry* entry);

	protected:
		gtkutil::RegistryConnector _registryConnector;
	public:
		ModalDialog m_modal;
		GtkWindow* m_parent;

		Dialog ();
		virtual ~Dialog ();

		/*!
		 start modal dialog box
		 you need to use AddModalButton to select eIDOK eIDCANCEL buttons
		 */
		EMessageBoxReturn DoModal ();
		void EndModal (EMessageBoxReturn code);
		virtual GtkWindow* BuildDialog () = 0;
		virtual void PreModal ()
		{
		}
		virtual void PostModal (EMessageBoxReturn code)
		{
		}
		virtual void ShowDlg ();
		virtual void HideDlg ();
		void Create ();
		void Destroy ();
		GtkWindow* GetWidget ()
		{
			return m_window;
		}
		const GtkWindow* GetWidget () const
		{
			return m_window;
		}

		// greebo: Adds a checkbox and connects it to the given registry key
		GtkWidget* addCheckBox(GtkWidget* vbox, const std::string& name, const std::string& flag, const std::string& registryKey);
		// greebo: Adds a slider and connects it to the registryKey
		void addSlider (GtkWidget* vbox, const std::string& name, const std::string& registryKey,
				gboolean draw_value, double value, double lower, double upper,
				double step_increment, double page_increment, double page_size);
		// greebo: Adds a combo box with the given string list and connects it to the registryKey
		void addCombo(GtkWidget* vbox, const std::string& name, const std::string& registryKey, const ComboBoxValueList& valueList);
		// greebo: Adds an entry field with the given caption <name> and connects it to the <registryKey>
		GtkWidget* addEntry(GtkWidget* vbox, const std::string& name, const std::string& registryKey);
		// greebo: Adds an GtkSpinner with the given caption <name>, bounds <lower> and <upper> and connects it to the <registryKey>
		GtkWidget* addSpinner (GtkWidget* vbox, const std::string& name, const std::string& registryKey, double lower,
				double upper, int fraction);
		GtkWidget* addTextureEntry (GtkWidget* vbox, const std::string& name, const std::string& registryKey);
		// greebo: Adds a PathEntry to choose files or directories (depending on the given boolean)
		GtkWidget* addPathEntry (GtkWidget* vbox, const std::string& name, const std::string& registryKey,
				bool browseDirectories);
};

#endif
