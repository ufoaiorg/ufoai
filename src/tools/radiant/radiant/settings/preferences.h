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

#include <libxml/parser.h>
#include "../dialog.h"
#include <list>
#include <map>
#include "iregistry.h"

void Widget_connectToggleDependency (GtkWidget* self, GtkWidget* toggleButton);

class PreferencesPage
{
		Dialog& m_dialog;
		GtkWidget* m_vbox;
	public:
		PreferencesPage (Dialog& dialog, GtkWidget* vbox) :
			m_dialog(dialog), m_vbox(vbox)
		{
		}
		GtkWidget* appendCheckBox (const char* name, const char* flag, bool& data)
		{
			return m_dialog.addCheckBox(m_vbox, name, flag, data);
		}
		GtkWidget* appendCheckBox (const char* name, const char* flag, const BoolImportCallback& importCallback,
				const BoolExportCallback& exportCallback)
		{
			return m_dialog.addCheckBox(m_vbox, name, flag, importCallback, exportCallback);
		}

		/* greebo: This adds a checkbox and connects it to an XMLRegistry key.
		 * @returns: the pointer to the created GtkWidget */
		GtkWidget* appendCheckBox(const std::string& name, const std::string& flag,
				const std::string& registryKey, RegistryKeyObserver* keyObserver) {
			return m_dialog.addCheckBox(m_vbox, name, flag, registryKey,
					keyObserver);
		}

		void appendCombo (const char* name, StringArrayRange values, const IntImportCallback& importCallback,
				const IntExportCallback& exportCallback)
		{
			m_dialog.addCombo(m_vbox, name, values, importCallback, exportCallback);
		}
		void appendCombo (const char* name, int& data, StringArrayRange values)
		{
			m_dialog.addCombo(m_vbox, name, data, values);
		}
		void appendSlider (const char* name, int& data, gboolean draw_value, const char* low, const char* high,
				double value, double lower, double upper, double step_increment, double page_increment,
				double page_size)
		{
			m_dialog.addSlider(m_vbox, name, data, draw_value, low, high, value, lower, upper, step_increment,
					page_increment, page_size);
		}
		void appendRadio (const char* name, StringArrayRange names, const IntImportCallback& importCallback,
				const IntExportCallback& exportCallback)
		{
			m_dialog.addRadio(m_vbox, name, names, importCallback, exportCallback);
		}
		void appendRadio (const char* name, int& data, StringArrayRange names)
		{
			m_dialog.addRadio(m_vbox, name, data, names);
		}
		void appendRadioIcons (const char* name, StringArrayRange icons, const IntImportCallback& importCallback,
				const IntExportCallback& exportCallback)
		{
			m_dialog.addRadioIcons(m_vbox, name, icons, importCallback, exportCallback);
		}
		void appendRadioIcons (const char* name, int& data, StringArrayRange icons)
		{
			m_dialog.addRadioIcons(m_vbox, name, data, icons);
		}
		GtkWidget* appendEntry (const char* name, const IntImportCallback& importCallback,
				const IntExportCallback& exportCallback)
		{
			return m_dialog.addIntEntry(m_vbox, name, importCallback, exportCallback);
		}
		GtkWidget* appendEntry (const char* name, int& data)
		{
			return m_dialog.addEntry(m_vbox, name, data);
		}
		GtkWidget* appendEntry (const char* name, const SizeImportCallback& importCallback,
				const SizeExportCallback& exportCallback)
		{
			return m_dialog.addSizeEntry(m_vbox, name, importCallback, exportCallback);
		}
		GtkWidget* appendEntry (const char* name, std::size_t& data)
		{
			return m_dialog.addEntry(m_vbox, name, data);
		}
		GtkWidget* appendEntry (const char* name, const FloatImportCallback& importCallback,
				const FloatExportCallback& exportCallback)
		{
			return m_dialog.addFloatEntry(m_vbox, name, importCallback, exportCallback);
		}
		GtkWidget* appendEntry (const char* name, float& data)
		{
			return m_dialog.addEntry(m_vbox, name, data);
		}
		GtkWidget* appendPathEntry (const char* name, bool browse_directory,
				const StringImportCallback& importCallback, const StringExportCallback& exportCallback)
		{
			return m_dialog.addPathEntry(m_vbox, name, browse_directory, importCallback, exportCallback);
		}
		GtkWidget* appendPathEntry (const char* name, std::string& data, bool directory)
		{
			return m_dialog.addPathEntry(m_vbox, name, data, directory);
		}
		GtkWidget* appendSpinner (const char* name, int& data, double value, double lower, double upper)
		{
			return m_dialog.addSpinner(m_vbox, name, data, value, lower, upper);
		}
		GtkWidget* appendSpinner (const char* name, double value, double lower, double upper,
				const IntImportCallback& importCallback, const IntExportCallback& exportCallback)
		{
			return m_dialog.addSpinner(m_vbox, name, value, lower, upper, importCallback, exportCallback);
		}
		GtkWidget* appendSpinner (const char* name, double value, double lower, double upper,
				const FloatImportCallback& importCallback, const FloatExportCallback& exportCallback)
		{
			return m_dialog.addSpinner(m_vbox, name, value, lower, upper, importCallback, exportCallback);
		}
};

typedef Callback1<PreferencesPage&> PreferencesPageCallback;

class PreferenceGroup
{
	public:
		virtual ~PreferenceGroup ()
		{
		}
		virtual PreferencesPage createPage (const char* treeName, const char* frameName) = 0;
};

typedef Callback1<PreferenceGroup&> PreferenceGroupCallback;

void PreferencesDialog_addInterfacePreferences (const PreferencesPageCallback& callback);
void PreferencesDialog_addInterfacePage (const PreferenceGroupCallback& callback);
void PreferencesDialog_addSettingsPreferences (const PreferencesPageCallback& callback);
void PreferencesDialog_addSettingsPage (const PreferenceGroupCallback& callback);

void PreferencesDialog_restartRequired (const std::string& staticName);

template<typename Value>
class LatchedValue
{
	public:
		Value m_value;
		Value m_latched;
		const std::string m_description;

		LatchedValue (Value value, const std::string& description) :
			m_latched(value), m_description(description)
		{
		}
		void useLatched ()
		{
			m_value = m_latched;
		}
		void import (Value value)
		{
			m_latched = value;
			if (m_latched != m_value) {
				PreferencesDialog_restartRequired(m_description);
			}
		}
};

typedef LatchedValue<bool> LatchedBool;
typedef MemberCaller1<LatchedBool, bool, &LatchedBool::import> LatchedBoolImportCaller;

typedef LatchedValue<int> LatchedInt;
typedef MemberCaller1<LatchedInt, int, &LatchedInt::import> LatchedIntImportCaller;

/*!
 holds game specific configuration stuff
 such as base names, engine names, some game specific features to activate in the various modules
 it is not strictly a prefs thing since the user is not supposed to edit that (unless he is hacking
 support for a new game)

 what we do now is fully generate the information for this during the setup. We might want to
 generate a piece that just says "the game pack is there", but put the rest of the config somwhere
 else (i.e. not generated, copied over during setup .. for instance in the game tools directory)
 */
class CGameDescription
{
	private:
		typedef std::map<std::string, std::string> GameDescription;

	public:
		std::string mGameFile; ///< the .game file that describes this game
		const std::string emptyString;
		GameDescription m_gameDescription;

		const std::string& getKeyValue (const std::string& key) const
		{
			GameDescription::const_iterator i = m_gameDescription.find(key);
			if (i != m_gameDescription.end()) {
				return (*i).second;
			}
			return CGameDescription::emptyString;
		}
		const std::string& getRequiredKeyValue (const std::string& key) const
		{
			GameDescription::const_iterator i = m_gameDescription.find(key);
			if (i != m_gameDescription.end()) {
				return (*i).second;
			}
			ERROR_MESSAGE("game attribute " << makeQuoted(key) << " not found in " << makeQuoted(mGameFile));
			return CGameDescription::emptyString;
		}

		CGameDescription (xmlDocPtr pDoc, const std::string &GameFile);
};

extern CGameDescription *g_pGameDescription;

typedef struct _GtkWidget GtkWidget;
class PrefsDlg;

class PreferencesPage;

class StringOutputStream;

/*!
 standalone dialog for games selection, and more generally global settings
 */
class CGameDialog: public Dialog
{
	protected:

		mutable int m_nComboSelect; ///< intermediate int value for combo in dialog box

	public:

		/*!
		 those settings are saved in the global prefs file
		 I'm too lazy to wrap behind protected access, not sure this needs to be public
		 NOTE: those are preference settings. if you change them it is likely that you would
		 have to restart the editor for them to take effect
		 */
		/*@{*/
		/*!
		 log console to radiant.log
		 m_bForceLogConsole is an obscure forced latching situation
		 */
		bool m_bForceLogConsole;
		/*@}*/

		CGameDialog () :
			m_bForceLogConsole(false)
		{
		}
		virtual ~CGameDialog ();

		void Init ();

		/*!
		 reset the global settings by removing the file
		 */
		void Reset ();

		/*!
		 Dialog API
		 this is only called when the dialog is built at startup for main engine select
		 */
		GtkWindow* BuildDialog ();

		/*!
		 construction of the dialog frame
		 this is the part to be re-used in prefs dialog
		 for the standalone dialog, we include this in a modal box
		 for prefs, we hook the frame in the main notebook
		 build the frame on-demand (only once)
		 */
		void CreateGlobalFrame (PreferencesPage& page);

	private:

		/*!
		 uses m_nComboItem to find the right mGames
		 */
		CGameDescription *GameDescriptionForComboItem ();
};

/*!
 this holds global level preferences
 */
extern CGameDialog g_GamesDialog;

class texdef_t;

class PrefsDlg: public Dialog
{
	public:

		GtkWidget *m_notebook;

		virtual ~PrefsDlg ()
		{
		}

		// initialize the above paths
		void Init ();

		/*! Utility function for swapping notebook pages for tree list selections */
		void showPrefPage (GtkWidget* prefpage);

	protected:

		/*! Dialog API */
		GtkWindow* BuildDialog ();
		void PostModal (EMessageBoxReturn code);
};

extern PrefsDlg g_Preferences;

struct preferences_globals_t
{
		// disabled all INI / registry read write .. used when shutting down after registry cleanup
		bool disable_ini;
		preferences_globals_t () :
			disable_ini(false)
		{
		}
};
extern preferences_globals_t g_preferences_globals;

typedef struct _GtkWindow GtkWindow;
void PreferencesDialog_constructWindow (GtkWindow* main_window);
void PreferencesDialog_destroyWindow ();

void PreferencesDialog_showDialog ();

void Preferences_Init ();

void Preferences_Load ();
void Preferences_Save ();

void Preferences_Reset ();

#endif
