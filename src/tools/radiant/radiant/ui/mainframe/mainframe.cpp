/**
 * @file mainframe.cpp
 * @brief Main Window for UFORadiant
 * @note Creates the editing windows and dialogs, creates commands and
 * registers preferences as well as handling internal paths
 */

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

#ifdef HAVE_CONFIG_H
#include "../../../../config.h"
#endif
#include "mainframe.h"
#include "radiant_i18n.h"

#include "debugging/debugging.h"
#include "version.h"

#include "ifilesystem.h"
#include "iundo.h"
#include "ifilter.h"
#include "iradiant.h"
#include "iregistry.h"
#include "igamemanager.h"
#include "iuimanager.h"
#include "itextures.h"
#include "igl.h"
#include "iump.h"
#include "ieventmanager.h"
#include "iselectionset.h"
#include "moduleobservers.h"

#include "os/path.h"
#include "os/file.h"

#include "gtkutil/clipboard.h"
#include "gtkutil/frame.h"
#include "gtkutil/glfont.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/image.h"
#include "gtkutil/widget.h"
#include "gtkutil/IconTextMenuToggle.h"
#include "gtkutil/MultiMonitor.h"
#include "gtkutil/menu.h"

#include "SplitPaneLayout.h"
#include "../mru/MRU.h"
#include "../splash/Splash.h"
#include "../Icons.h"
#include "../common/ToolbarCreator.h"
#include "../menu/FiltersMenu.h"

#include "../../map/AutoSaver.h"
#include "../../sidebar/sidebar.h"
#include "../../sidebar/texturebrowser.h"
#include "../../sidebar/surfaceinspector/surfaceinspector.h"
#include "../../settings/PreferenceDialog.h"
#include "../../render/OpenGLRenderSystem.h"
#include "../../camera/CamWnd.h"
#include "../../camera/GlobalCamera.h"
#include "../../xyview/GlobalXYWnd.h"
#include "../../textool/TexTool.h"
#include "../../server.h"
#include "../../plugin.h"
#include "../../windowobservers.h"
#include "../../environment.h"

namespace {
const std::string RKEY_WINDOW_STATE = "user/ui/mainFrame/window";
const std::string RKEY_MULTIMON_START_MONITOR = "user/ui/multiMonitor/startMonitorNum";
}

// Virtual file system
class VFSModuleObserver: public ModuleObserver
{
		std::size_t m_unrealised;
	public:
		VFSModuleObserver () :
			m_unrealised(1)
		{
		}
		void realise (void)
		{
			if (--m_unrealised == 0) {
				GlobalFileSystem().initDirectory(GlobalRadiant().getFullGamePath());
				GlobalFileSystem().initialise();
			}
		}
		void unrealise (void)
		{
			if (++m_unrealised == 1) {
				GlobalFileSystem().shutdown();
			}
		}
};

namespace {
VFSModuleObserver g_VFSModuleObserver;
ModuleObservers g_gameModeObservers;
}

void Radiant_attachGameModeObserver (ModuleObserver& observer)
{
	g_gameModeObservers.attach(observer);
}

void Radiant_detachGameModeObserver (ModuleObserver& observer)
{
	g_gameModeObservers.detach(observer);
}

ModuleObservers g_gameToolsPathObservers;

void Radiant_attachGameToolsPathObserver (ModuleObserver& observer)
{
	g_gameToolsPathObservers.attach(observer);
}

void Radiant_detachGameToolsPathObserver (ModuleObserver& observer)
{
	g_gameToolsPathObservers.detach(observer);
}

// This is called from main() to start up the Radiant stuff.
void Radiant_Initialise (void)
{
	// Load the ColourSchemes from the registry
	ColourSchemes().loadColourSchemes();

	// Load the other modules
	Radiant_Construct(GlobalRadiantModuleServer());

	g_VFSModuleObserver.realise();
	GlobalTextureBrowser().createWidget();

	// Rebuild the map path basing on the userGamePath
	std::string newMapPath = GlobalRadiant().getFullGamePath() + "maps/";
	g_mkdir_with_parents(newMapPath.c_str(), 0755);
	Environment::Instance().setMapsPath(newMapPath);

	g_gameToolsPathObservers.realise();
	g_gameModeObservers.realise();

	GlobalUMPSystem().init();

	// Construct the MRU commands and menu structure
	GlobalMRU().constructMenu();

	// Initialise the most recently used files list
	GlobalMRU().loadRecentFiles();

	gtkutil::MultiMonitor::printMonitorInfo();
}

void Radiant_Shutdown (void)
{
	Environment::Instance().deletePathsFromRegistry();

	GlobalMRU().saveRecentFiles();

	g_VFSModuleObserver.unrealise();
	Environment::Instance().setMapsPath("");
	g_gameModeObservers.unrealise();
	g_gameToolsPathObservers.unrealise();

	Radiant_Destroy();
}

static gint window_realize_remove_decoration (GtkWidget* widget, gpointer data)
{
	gdk_window_set_decorations(widget->window, (GdkWMDecoration) (GDK_DECOR_ALL | GDK_DECOR_MENU | GDK_DECOR_MINIMIZE
			| GDK_DECOR_MAXIMIZE));
	return FALSE;
}

class WaitDialog
{
	public:
		GtkWindow* m_window;
		GtkLabel* m_label;
};

static WaitDialog create_wait_dialog (const std::string& title, const std::string& text)
{
	WaitDialog dialog;

	dialog.m_window = create_floating_window(title, GlobalRadiant().getMainWindow());
	gtk_window_set_resizable(dialog.m_window, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(dialog.m_window), 0);
	gtk_window_set_position(dialog.m_window, GTK_WIN_POS_CENTER_ON_PARENT);

	g_signal_connect(G_OBJECT(dialog.m_window), "realize", G_CALLBACK(window_realize_remove_decoration), 0);

	{
		dialog.m_label = GTK_LABEL(gtk_label_new(text.c_str()));
		gtk_misc_set_alignment(GTK_MISC(dialog.m_label), 0.5, 0.5);
		gtk_label_set_justify(dialog.m_label, GTK_JUSTIFY_CENTER);
		gtk_widget_show(GTK_WIDGET(dialog.m_label));
		gtk_widget_set_size_request(GTK_WIDGET(dialog.m_label), 300, 100);

		gtk_container_add(GTK_CONTAINER(dialog.m_window), GTK_WIDGET(dialog.m_label));
	}
	return dialog;
}

namespace {
clock_t g_lastRedrawTime = 0;
const clock_t c_redrawInterval = clock_t(CLOCKS_PER_SEC / 10);

bool redrawRequired (void)
{
	clock_t currentTime = std::clock();
	if (currentTime - g_lastRedrawTime >= c_redrawInterval) {
		g_lastRedrawTime = currentTime;
		return true;
	}
	return false;
}
}

bool MainFrame_isActiveApp (void)
{
	GList* list = gtk_window_list_toplevels();
	for (GList* i = list; i != 0; i = g_list_next(i)) {
		if (gtk_window_is_active(GTK_WINDOW(i->data))) {
			return true;
		}
	}
	return false;
}

typedef std::list<std::string> StringStack;
static StringStack g_wait_stack;
static WaitDialog g_wait;

bool ScreenUpdates_Enabled (void)
{
	return g_wait_stack.empty();
}

void ScreenUpdates_process (void)
{
	if (redrawRequired() && GTK_WIDGET_VISIBLE(g_wait.m_window)) {
		while (gtk_events_pending()) {
			gtk_main_iteration();
		}
	}
}

void ScreenUpdates_Disable (const std::string& message, const std::string& title)
{
	if (g_wait_stack.empty()) {
		map::AutoSaver().stopTimer();

		while (gtk_events_pending()) {
			gtk_main_iteration();
		}

		const bool isActiveApp = MainFrame_isActiveApp();

		g_wait = create_wait_dialog(title, message);

		if (isActiveApp) {
			gtk_widget_show(GTK_WIDGET(g_wait.m_window));
			gtk_grab_add(GTK_WIDGET(g_wait.m_window));
			ScreenUpdates_process();
		}
	} else if (GTK_WIDGET_VISIBLE(g_wait.m_window)) {
		gtk_label_set_text(g_wait.m_label, message.c_str());
		ScreenUpdates_process();
	}
	g_wait_stack.push_back(message);
}

void ScreenUpdates_Enable (void)
{
	ASSERT_MESSAGE(!ScreenUpdates_Enabled(), "screen updates already enabled");
	g_wait_stack.pop_back();
	if (g_wait_stack.empty()) {
		map::AutoSaver().startTimer();
		gtk_grab_remove(GTK_WIDGET(g_wait.m_window));
		destroy_floating_window(g_wait.m_window);
		g_wait.m_window = 0;
	} else if (GTK_WIDGET_VISIBLE(g_wait.m_window)) {
		gtk_label_set_text(g_wait.m_label, g_wait_stack.back().c_str());
		ScreenUpdates_process();
	}
}

void GlobalCamera_UpdateWindow (void)
{
	if (g_pParentWnd != 0) {
		g_pParentWnd->GetCamWnd()->update();
	}
}

void XY_UpdateAllWindows (void)
{
	if (g_pParentWnd != 0) {
		GlobalXYWnd().updateAllViews();
	}
}

void UpdateAllWindows (void)
{
	GlobalCamera_UpdateWindow();
	XY_UpdateAllWindows();
}

void ClipperChangeNotify (void)
{
	GlobalCamera_UpdateWindow();
	XY_UpdateAllWindows();
}

static GtkWidget* create_main_statusbar (GtkWidget *pStatusLabel[c_count_status])
{
	GtkTable* table = GTK_TABLE(gtk_table_new(1, c_count_status + 1, FALSE));
	gtk_widget_show(GTK_WIDGET(table));

	{
		GtkLabel* label = GTK_LABEL(gtk_label_new(_("Label")));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_misc_set_padding(GTK_MISC(label), 2, 2);
		gtk_widget_show(GTK_WIDGET(label));
		gtk_table_attach_defaults(table, GTK_WIDGET(label), 0, 1, 0, 1);
		pStatusLabel[c_command_status] = GTK_WIDGET(label);
	}

	for (int i = 1; i < c_count_status; ++i) {
		GtkFrame* frame = GTK_FRAME(gtk_frame_new(0));
		gtk_widget_show(GTK_WIDGET(frame));
		gtk_table_attach_defaults(table, GTK_WIDGET(frame), i, i + 1, 0, 1);
		gtk_frame_set_shadow_type(frame, GTK_SHADOW_IN);

		GtkLabel* label = GTK_LABEL(gtk_label_new(_("Label")));
		gtk_label_set_ellipsize(label, PANGO_ELLIPSIZE_END);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_misc_set_padding(GTK_MISC(label), 2, 2);
		gtk_widget_show(GTK_WIDGET(label));
		gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(label));
		pStatusLabel[i] = GTK_WIDGET(label);
	}

	return GTK_WIDGET(table);
}

class MainWindowActive
{
		static gboolean notify (GtkWindow* window, gpointer dummy, MainWindowActive* self)
		{
			if (g_wait.m_window != 0 && gtk_window_is_active(window) && !GTK_WIDGET_VISIBLE(g_wait.m_window)) {
				gtk_widget_show(GTK_WIDGET(g_wait.m_window));
			}

			return FALSE;
		}
	public:
		void connect (GtkWindow* toplevel_window)
		{
			g_signal_connect(G_OBJECT(toplevel_window), "notify::is-active", G_CALLBACK(notify), this);
		}
};

MainWindowActive g_MainWindowActive;

// =============================================================================
// MainFrame class

MainFrame* g_pParentWnd = 0;

GtkWindow* MainFrame_getWindow (void)
{
	if (g_pParentWnd == 0) {
		return 0;
	}
	return g_pParentWnd->m_window;
}

MainFrame::MainFrame () :
	m_window(0), g_currentToolMode(0), g_defaultToolMode(0), m_idleRedrawStatusText(RedrawStatusTextCaller(*this))
{
	m_pCamWnd = 0;

	for (int n = 0; n < c_count_status; n++) {
		m_pStatusLabel[n] = 0;
	}

	// Register this class in the preference system so that the constructPreferencePage() gets called.
	GlobalPreferenceSystem().addConstructor(this);

	Create();
}

MainFrame::~MainFrame (void)
{
	SaveWindowInfo();

	gtk_widget_hide(GTK_WIDGET(m_window));

	Shutdown();

	gtk_widget_destroy(GTK_WIDGET(m_window));
}

static gint mainframe_delete (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	if (GlobalMap().askForSave("Exit Radiant")) {
		gtk_main_quit();
	}

	return TRUE;
}

void MainFrame::constructPreferencePage (PreferenceGroup& group)
{
	// Add another page for Multi-Monitor stuff
	PreferencesPage* page(group.createPage(_("Display"), _("Multi Monitor")));

	// Initialise the registry, if no key is set
	if (GlobalRegistry().get(RKEY_MULTIMON_START_MONITOR).empty()) {
		GlobalRegistry().set(RKEY_MULTIMON_START_MONITOR, "0");
	}

	ComboBoxValueList list;

	for (int i = 0; i < gtkutil::MultiMonitor::getNumMonitors(); ++i) {
		GdkRectangle rect = gtkutil::MultiMonitor::getMonitor(i);

		list.push_back(string::format("Monitor %d (%dx%d)", i, rect.width, rect.height));
	}

	page->appendCombo(_("Start UFORadiant on monitor"), RKEY_MULTIMON_START_MONITOR, list);
}

/**
 * @brief Create the user settable window layout
 */
void MainFrame::Create (void)
{
	GtkWindow* window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));

	// do this here, because the commands are needed
	_sidebar = new ui::Sidebar();
	GtkWidget *sidebar = _sidebar->getWidget();

	// Tell the XYManager which window the xyviews should be transient for
	GlobalXYWnd().setGlobalParentWindow(window);

	GlobalWindowObservers_connectTopLevel(window);

	gtk_window_set_transient_for(ui::Splash::Instance().getWindow(), window);

#ifndef _WIN32
	{
		GdkPixbuf* pixbuf = gtkutil::getLocalPixbuf(ui::icons::ICON);
		if (pixbuf != 0) {
			gtk_window_set_icon(window, pixbuf);
			g_object_unref(pixbuf);
		}
	}
#endif

	gtk_widget_add_events(GTK_WIDGET(window), GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_FOCUS_CHANGE_MASK);
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(mainframe_delete), this);

	m_position_tracker.connect(window);

	g_MainWindowActive.connect(window);

	GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_show(vbox);

	GlobalEventManager().connect(GTK_OBJECT(window));
	GlobalEventManager().connectAccelGroup(GTK_WINDOW(window));

	m_nCurrentStyle = eSplit;

	// Create the Filter menu entries
	ui::FiltersMenu::addItemsToMainMenu();

	// Retrieve the "main" menubar from the UIManager
	GtkMenuBar* mainMenu = GTK_MENU_BAR(GlobalUIManager().getMenuManager()->get("main"));
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(mainMenu), false, false, 0);

	// Instantiate the ToolbarCreator and retrieve the standard toolbar widget
	ui::ToolbarCreator toolbarCreator;

	GtkToolbar* generalToolbar = toolbarCreator.getToolbar("view");
	gtk_widget_show(GTK_WIDGET(generalToolbar));

	GlobalSelectionSetManager().init(generalToolbar);

	// Pack it into the main window
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(generalToolbar), FALSE, FALSE, 0);

	GtkWidget* main_statusbar = create_main_statusbar(m_pStatusLabel);
	gtk_box_pack_end(GTK_BOX(vbox), main_statusbar, FALSE, TRUE, 2);

	GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), TRUE, TRUE, 0);
	gtk_widget_show(hbox);

	GtkToolbar* main_toolbar_v = toolbarCreator.getToolbar("edit");
	gtk_widget_show(GTK_WIDGET(main_toolbar_v));

	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(main_toolbar_v), FALSE, FALSE, 0);


	// Connect the window position tracker
	_windowPosition.loadFromPath(RKEY_WINDOW_STATE);

	// Yes, connect the position tracker, this overrides the existing setting.
	_windowPosition.connect(window);

	int startMonitor = GlobalRegistry().getInt(RKEY_MULTIMON_START_MONITOR);
	if (startMonitor < gtkutil::MultiMonitor::getNumMonitors()) {
		// Load the correct coordinates into the position tracker
		_windowPosition.fitToScreen(gtkutil::MultiMonitor::getMonitor(startMonitor), 0.8f, 0.8f);
	}

	// Apply the position
	_windowPosition.applyPosition();

	int windowState = string::toInt(GlobalRegistry().getAttribute(RKEY_WINDOW_STATE, "state"), GDK_WINDOW_STATE_MAXIMIZED);
	if (windowState & GDK_WINDOW_STATE_MAXIMIZED)
		gtk_window_maximize(window);

	m_window = window;

	gtk_widget_show(GTK_WIDGET(window));

	// The default XYView pointer
	XYWnd* xyWnd;

	GtkWidget* mainHBox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), mainHBox, TRUE, TRUE, 0);
	gtk_widget_show(mainHBox);

	int w, h;
	gtk_window_get_size(window, &w, &h);

	// camera
	m_pCamWnd = GlobalCamera().newCamWnd();
	GlobalCamera().setCamWnd(m_pCamWnd);
	GlobalCamera().setParent(m_pCamWnd, window);
	GtkWidget* camera = m_pCamWnd->getWidget();

	// Allocate the three ortho views
	xyWnd = GlobalXYWnd().createXY();
	xyWnd->setViewType(XY);
	GtkWidget* xy = xyWnd->getWidget();

	XYWnd* yzWnd = GlobalXYWnd().createXY();
	yzWnd->setViewType(YZ);
	GtkWidget* yz = yzWnd->getWidget();

	XYWnd* xzWnd = GlobalXYWnd().createXY();
	xzWnd->setViewType(XZ);
	GtkWidget* xz = xzWnd->getWidget();

	// split view (4 views)
	GtkHPaned* split = create_split_views(camera, yz, xy, xz);
	gtk_box_pack_start(GTK_BOX(mainHBox), GTK_WIDGET(split), TRUE, TRUE, 0);

	// greebo: In any layout, there is at least the XY view present, make it active
	GlobalXYWnd().setActiveXY(xyWnd);

	PreferencesDialog_constructWindow(window);

	GlobalGrid().addGridChangeCallback(FreeCaller<XY_UpdateAllWindows> ());

	/* enable button state tracker, set default states for begin */
	GlobalUndoSystem().trackerAttach(m_saveStateTracker);

	gtk_box_pack_start(GTK_BOX(mainHBox), GTK_WIDGET(sidebar), FALSE, FALSE, 0);

	// Start the autosave timer so that it can periodically check the map for changes
	map::AutoSaver().startTimer();
}

void MainFrame::SaveWindowInfo (void)
{
	// Delete all the current window states from the registry
	GlobalRegistry().deleteXPath(RKEY_WINDOW_STATE);

	// Tell the position tracker to save the information
	_windowPosition.saveToPath(RKEY_WINDOW_STATE);

	GdkWindow* window = GTK_WIDGET(m_window)->window;
	if (window != NULL)
		GlobalRegistry().setAttribute(RKEY_WINDOW_STATE, "state", string::toString(gdk_window_get_state(window)));
}

void MainFrame::Shutdown (void)
{
	map::AutoSaver().stopTimer();
	ui::TexTool::Instance().shutdown();

	GlobalUndoSystem().trackerDetach(m_saveStateTracker);

	GlobalXYWnd().destroyViews();

	GlobalCamera().deleteCamWnd(m_pCamWnd);
	m_pCamWnd = 0;

	PreferencesDialog_destroyWindow();

	delete _sidebar;

	// Stop the AutoSaver class from being called
	map::AutoSaver().stopTimer();
}

/**
 * @brief Updates the statusbar text with command, position, texture and so on
 * @sa TextureBrowser_SetStatus
 */
void MainFrame::RedrawStatusText (void)
{
	gtk_label_set_markup(GTK_LABEL(m_pStatusLabel[c_command_status]), m_command_status.c_str());
	gtk_label_set_markup(GTK_LABEL(m_pStatusLabel[c_position_status]), m_position_status.c_str());
	gtk_label_set_markup(GTK_LABEL(m_pStatusLabel[c_brushcount_status]), m_brushcount_status.c_str());
	gtk_label_set_markup(GTK_LABEL(m_pStatusLabel[c_texture_status]), m_texture_status.c_str());
}

void MainFrame::UpdateStatusText (void)
{
	m_idleRedrawStatusText.queueDraw();
}

void MainFrame::SetStatusText (std::string& status_text, const std::string& pText)
{
	status_text = pText;
	UpdateStatusText();
}

/**
 * @brief Updates the first statusbar column
 * @param[in] status the status to print into the first statusbar column
 * @sa MainFrame::RedrawStatusText
 * @sa MainFrame::SetStatusText
 */
void Sys_Status (const std::string& status)
{
	if (g_pParentWnd != 0) {
		g_pParentWnd->SetStatusText(g_pParentWnd->m_command_status, status);
	}
}

namespace {
GLFont g_font(0, 0);
}

void GlobalGL_sharedContextCreated (void)
{
	// report OpenGL information
	globalOutputStream() << "GL_VENDOR: " << reinterpret_cast<const char*> (glGetString(GL_VENDOR)) << "\n";
	globalOutputStream() << "GL_RENDERER: " << reinterpret_cast<const char*> (glGetString(GL_RENDERER)) << "\n";
	globalOutputStream() << "GL_VERSION: " << reinterpret_cast<const char*> (glGetString(GL_VERSION)) << "\n";
	globalOutputStream() << "GL_EXTENSIONS: " << reinterpret_cast<const char*> (glGetString(GL_EXTENSIONS)) << "\n";

	QGL_sharedContextCreated(GlobalOpenGL());

	GlobalShaderCache().realise();
	GlobalTexturesCache().realise();

	/* use default font here (Sans 10 is gtk default) */
	GtkSettings *settings = gtk_settings_get_default();
	gchar *fontname;
	g_object_get(settings, "gtk-font-name", &fontname, (char*) 0);
	g_font = glfont_create(fontname);
	// Fallbacks
	if (g_font.getPixelHeight() == -1)
		g_font = glfont_create("Sans 10");
	if (g_font.getPixelHeight() == -1)
		g_font = glfont_create("fixed 10");
	if (g_font.getPixelHeight() == -1)
		g_font = glfont_create("courier new 10");

	GlobalOpenGL().m_font = g_font.getDisplayList();
	GlobalOpenGL().m_fontHeight = g_font.getPixelHeight();
}

void GlobalGL_sharedContextDestroyed (void)
{
	GlobalTexturesCache().unrealise();
	GlobalShaderCache().unrealise();

	QGL_sharedContextDestroyed(GlobalOpenGL());
}

#include "preferencesystem.h"
#include "stringio.h"

void Commands_Register ();

void MainFrame_Construct (void)
{
	// Tell the FilterSystem to register its commands
	GlobalFilterSystem().init();

	Commands_Register();

	GLWidget_sharedContextCreated = GlobalGL_sharedContextCreated;
	GLWidget_sharedContextDestroyed = GlobalGL_sharedContextDestroyed;

	// Broadcast the startup event
	GlobalRadiant().broadcastStartupEvent();
}

void MainFrame_Destroy (void)
{
	// Broadcast shutdown event to RadiantListeners
	GlobalRadiant().broadcastShutdownEvent();
}

/**
 * Called whenever save was completed. This causes the UndoSaveTracker to mark this point as saved.
 */
void MainFrame::SaveComplete ()
{
	m_saveStateTracker.storeState();
}
