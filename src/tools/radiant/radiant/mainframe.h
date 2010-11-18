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

#if !defined(INCLUDED_MAINFRAME_H)
#define INCLUDED_MAINFRAME_H

#include "gtkutil/window.h"
#include "gtkutil/idledraw.h"
#include "gtkutil/widget.h"
#include "gtkutil/button.h"
#include "gtkutil/WindowPosition.h"
#include "string/string.h"
#include "iundo.h"

#include "iradiant.h"

class IPlugin;
class IToolbarButton;

class XYWnd;
class CamWnd;
namespace ui { class Sidebar; }

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;

const int c_command_status = 0;
const int c_position_status = 1;
const int c_brushcount_status = 2;
const int c_texture_status = 3;
const int c_count_status = 4;

class UndoSaveStateTracker: public UndoTracker
{
		unsigned int m_undoSteps;
		unsigned int m_redoSteps;
		int m_savedStep;

		void UpdateSensitiveStates (void);
		void increaseUndo();
		void increaseRedo();
		void checkUndoLevel();
	public:
		UndoSaveStateTracker () :
			m_undoSteps(0), m_redoSteps(0), m_savedStep(0)
		{
		}
		void clear ();
		void clearRedo ();
		void begin ();
		void undo ();
		void redo ();
		void storeState (void);
};

class MainFrame
{
	public:
		/** @brief The style of the window layout */
		enum EViewStyle
		{
			eRegular = 0, /**< one view, console and texture on the right side */
			eSplit = 1
		/**< 4 views */
		};

		MainFrame ();
		~MainFrame ();

		GtkWindow* m_window;

		std::string m_command_status;
		std::string m_position_status;
		std::string m_brushcount_status;
		std::string m_texture_status;
	private:

		gtkutil::WindowPosition _windowPosition;

		void Create ();
		void SaveWindowInfo ();
		void Shutdown ();

		GtkWidget* m_hSplit;
		GtkWidget* m_vSplit2;

		ui::Sidebar *_sidebar;

		CamWnd* m_pCamWnd;

		GtkWidget *m_pStatusLabel[c_count_status];

		EViewStyle m_nCurrentStyle;
		WindowPositionTracker m_position_tracker;

		IdleDraw m_idleRedrawStatusText;

		GtkButton *m_toolBtnSave;
		GtkButton *m_toolBtnUndo;
		GtkButton *m_toolBtnRedo;

		UndoSaveStateTracker m_saveStateTracker;

	public:
		void SetStatusText (std::string& status_text, const std::string& pText);
		void UpdateStatusText ();
		void RedrawStatusText ();
		typedef MemberCaller<MainFrame, &MainFrame::RedrawStatusText> RedrawStatusTextCaller;

		CamWnd* GetCamWnd ()
		{
			ASSERT_NOTNULL(m_pCamWnd);
			return m_pCamWnd;
		}

		EViewStyle CurrentStyle ()
		{
			return m_nCurrentStyle;
		}

		void SaveComplete (void);
};

extern MainFrame* g_pParentWnd;

GtkWindow* MainFrame_getWindow ();

template<typename Value>
class LatchedValue;
typedef LatchedValue<bool> LatchedBool;

// Set the text to be displayed in the status bar
void Sys_Status (const std::string& status);

void ScreenUpdates_Disable (const std::string& message, const std::string& title);
void ScreenUpdates_Enable ();
bool ScreenUpdates_Enabled ();
void ScreenUpdates_process ();

class ScopeDisableScreenUpdates
{
	public:
		ScopeDisableScreenUpdates (const std::string& message, const std::string& title)
		{
			ScreenUpdates_Disable(message, title);
		}
		~ScopeDisableScreenUpdates ()
		{
			ScreenUpdates_Enable();
		}
};

void EnginePath_Realise ();
void EnginePath_Unrealise ();

class ModuleObserver;

void Radiant_attachEnginePathObserver (ModuleObserver& observer);
void Radiant_detachEnginePathObserver (ModuleObserver& observer);

void Radiant_attachGameToolsPathObserver (ModuleObserver& observer);
void Radiant_detachGameToolsPathObserver (ModuleObserver& observer);

void EnginePath_verify ();
const std::string& EnginePath_get ();

const std::string AppPath_get ();

void Radiant_Initialise ();
void Radiant_Shutdown ();

void GlobalCamera_UpdateWindow();
void XY_UpdateAllWindows ();
void UpdateAllWindows ();

void ClipperChangeNotify ();

const std::string& basegame_get ();
void Radiant_attachGameModeObserver (ModuleObserver& observer);
void Radiant_detachGameModeObserver (ModuleObserver& observer);

void VFS_Construct ();
void VFS_Destroy ();

void HomePaths_Construct ();
void HomePaths_Destroy ();
void Radiant_attachHomePathsObserver (ModuleObserver& observer);
void Radiant_detachHomePathsObserver (ModuleObserver& observer);

void MainFrame_Construct ();
void MainFrame_Destroy ();

#endif
