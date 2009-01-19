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
#include "string/string.h"

#include "iradiant.h"

class IPlugIn;
class IToolbarButton;

class XYWnd;
class CamWnd;

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;

const int c_command_status = 0;
const int c_position_status = 1;
const int c_brushcount_status = 2;
const int c_texture_status = 3;
const int c_count_status = 4;

class MainFrame {
public:
	/** @brief The style of the window layout */
	enum EViewStyle {
		eRegular = 0,		/**< one view, console and texture on the right side */
		eSplit = 1			/**< 4 views */
	};

	MainFrame();
	~MainFrame();

	GtkWindow* m_window;

	CopiedString m_command_status;
	CopiedString m_position_status;
	CopiedString m_brushcount_status;
	CopiedString m_texture_status;
private:

	void Create();
	void SaveWindowInfo();
	void Shutdown();

	GtkWidget* m_vSplit;
	GtkWidget* m_hSplit;
	GtkWidget* m_vSplit2;

	XYWnd* m_pXYWnd;
	XYWnd* m_pYZWnd;
	XYWnd* m_pXZWnd;
	CamWnd* m_pCamWnd;
	XYWnd* m_pActiveXY;

	GtkWidget *m_pStatusLabel[c_count_status];

	EViewStyle m_nCurrentStyle;
	WindowPositionTracker m_position_tracker;

	IdleDraw m_idleRedrawStatusText;

public:
	void SetStatusText(CopiedString& status_text, const char* pText);
	void UpdateStatusText();
	void RedrawStatusText();
	typedef MemberCaller<MainFrame, &MainFrame::RedrawStatusText> RedrawStatusTextCaller;

	void SetActiveXY(XYWnd* p);
	XYWnd* ActiveXY() {
		return m_pActiveXY;
	};
	XYWnd* GetXYWnd() {
		return m_pXYWnd;
	}
	XYWnd* GetXZWnd() {
		return m_pXZWnd;
	}
	XYWnd* GetYZWnd() {
		return m_pYZWnd;
	}
	CamWnd* GetCamWnd() {
		ASSERT_NOTNULL(m_pCamWnd);
		return m_pCamWnd;
	}

	EViewStyle CurrentStyle() {
		return m_nCurrentStyle;
	};
};

extern MainFrame* g_pParentWnd;

GtkWindow* MainFrame_getWindow();

enum EMouseButtonMode {
	ETwoButton = 0,
	EThreeButton = 1,
};

struct glwindow_globals_t {
	int m_nMouseType;

	glwindow_globals_t() :
			m_nMouseType(EThreeButton) {
	}
};

void GLWindow_Construct();
void GLWindow_Destroy();

extern glwindow_globals_t g_glwindow_globals;
template<typename Value>
class LatchedValue;
typedef LatchedValue<bool> LatchedBool;
extern LatchedBool g_Layout_enableDetachableMenus;

void deleteSelection();


void Sys_Status(const char* status);


void ScreenUpdates_Disable(const char* message, const char* title);
void ScreenUpdates_Enable();
bool ScreenUpdates_Enabled();
void ScreenUpdates_process();

class ScopeDisableScreenUpdates {
public:
	ScopeDisableScreenUpdates(const char* message, const char* title) {
		ScreenUpdates_Disable(message, title);
	}
	~ScopeDisableScreenUpdates() {
		ScreenUpdates_Enable();
	}
};


void EnginePath_Realise();
void EnginePath_Unrealise();

class ModuleObserver;

void Radiant_attachEnginePathObserver(ModuleObserver& observer);
void Radiant_detachEnginePathObserver(ModuleObserver& observer);

void Radiant_attachGameToolsPathObserver(ModuleObserver& observer);
void Radiant_detachGameToolsPathObserver(ModuleObserver& observer);

void EnginePath_verify();
const char* EnginePath_get();
const char* QERApp_GetGamePath();

extern CopiedString g_strCompilerBinaryWithPath;
const char* CompilerBinaryWithPath_get();

extern CopiedString g_strAppPath;
const char* AppPath_get();

const char* SettingsPath_get();

const char* const g_pluginsDir = "plugins/"; ///< name of plugins directory, always sub-directory of toolspath
const char* const g_modulesDir = "modules/"; ///< name of modules directory, always sub-directory of toolspath

void Radiant_Initialise();
void Radiant_Shutdown();

void SaveMapAs();


void XY_UpdateAllWindows();
void UpdateAllWindows();


void ClipperChangeNotify();

void DefaultMode();

const char* basegame_get();
const char* gamename_get();
void gamename_set(const char* gamename);
void Radiant_attachGameNameObserver(ModuleObserver& observer);
void Radiant_detachGameNameObserver(ModuleObserver& observer);
void Radiant_attachGameModeObserver(ModuleObserver& observer);
void Radiant_detachGameModeObserver(ModuleObserver& observer);



void VFS_Construct();
void VFS_Destroy();

void HomePaths_Construct();
void HomePaths_Destroy();
void Radiant_attachHomePathsObserver(ModuleObserver& observer);
void Radiant_detachHomePathsObserver(ModuleObserver& observer);


void MainFrame_Construct();
void MainFrame_Destroy();

SignalHandlerId XYWindowDestroyed_connect(const SignalHandler& handler);
void XYWindowDestroyed_disconnect(SignalHandlerId id);
MouseEventHandlerId XYWindowMouseDown_connect(const MouseEventHandler& handler);
void XYWindowMouseDown_disconnect(MouseEventHandlerId id);

#endif
