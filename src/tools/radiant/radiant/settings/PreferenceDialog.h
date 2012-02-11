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

#if !defined(INCLUDED_PREFERENCES_H)
#define INCLUDED_PREFERENCES_H

#include "../dialog.h"
#include "preferencesystem.h"

class PrefPage : public PreferencesPage
{
		Dialog& m_dialog;
		GtkWidget* m_vbox;
	public:
		PrefPage (Dialog& dialog, GtkWidget* vbox) :
			m_dialog(dialog), m_vbox(vbox)
		{
		}
		/* greebo: This adds a checkbox and connects it to an XMLRegistry key.
		 * @returns: the pointer to the created GtkWidget */
		GtkWidget* appendCheckBox (const std::string& name, const std::string& flag, const std::string& registryKey)
		{
			return m_dialog.addCheckBox(m_vbox, name, flag, registryKey);
		}

		/* greebo: Appends an entry field with <name> as caption which is connected to the given registryKey
		 */
		GtkWidget* appendEntry(const std::string& name, const std::string& registryKey) {
			return m_dialog.addEntry(m_vbox, name, registryKey);
		}

		GtkWidget* appendTextureEntry(const std::string& name, const std::string& registryKey) {
			return m_dialog.addTextureEntry(m_vbox, name, registryKey);
		}

		/* greebo: greebo: This adds a horizontal slider to the internally referenced VBox and connects
		 * it to the given registryKey. */
		void appendSlider (const std::string& name, const std::string& registryKey, bool draw_value, double value,
				double lower, double upper, double step_increment, double page_increment, double page_size)
		{
			m_dialog.addSlider(m_vbox, name, registryKey, draw_value, value, lower, upper, step_increment,
					page_increment, page_size);
		}

		/* greebo: Use this to add a dropdown selection box with the given list of strings as captions. The value
		 * stored in the registryKey is used to determine the currently selected combobox item */
		void appendCombo(const std::string& name, const std::string& registryKey, const ComboBoxValueList& valueList)
		{
			m_dialog.addCombo(m_vbox, name, registryKey, valueList);
		}

		// greebo: Adds a PathEntry to choose files or directories (depending on the given boolean)
		GtkWidget* appendPathEntry(const std::string& name, const std::string& registryKey, bool browseDirectories) {
			return m_dialog.addPathEntry(m_vbox, name, registryKey, browseDirectories);
		}

		/* greebo: Appends an entry field with spinner buttons which retrieves its value from the given
		 * RegistryKey. The lower and upper values have to be passed as well.
		 */
		GtkWidget* appendSpinner(const std::string& name, const std::string& registryKey,
								 double lower, double upper, int fraction) {
			return m_dialog.addSpinner(m_vbox, name, registryKey, lower, upper, fraction);
		}
};

class PreferenceTreeGroup;

class PrefsDlg: public Dialog
{
		typedef std::list<PreferenceConstructor*> PreferenceConstructorList;
		// The list of all the constructors that have to be called on dialog construction
		PreferenceConstructorList _constructors;

	public:

		GtkWidget *m_notebook;

		virtual ~PrefsDlg ()
		{
		}

		/*! Utility function for swapping notebook pages for tree list selections */
		void showPrefPage (GtkWidget* prefpage);

		// Add the given preference constructor to the internal list
		void addConstructor(PreferenceConstructor* constructor);

	protected:

		/*! Dialog API */
		GtkWindow* BuildDialog ();
		void PostModal (EMessageBoxReturn code);

	private:
		// greebo: calls the constructors to add the preference elements
		void callConstructors(PreferenceTreeGroup& preferenceGroup);
};

extern PrefsDlg g_Preferences;

typedef struct _GtkWindow GtkWindow;
void PreferencesDialog_constructWindow (GtkWindow* main_window);
void PreferencesDialog_destroyWindow ();

void PreferencesDialog_showDialog ();

#endif
