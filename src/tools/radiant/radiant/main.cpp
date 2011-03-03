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

/*! \mainpage GtkRadiant Documentation Index

 \section intro_sec Introduction

 This documentation is generated from comments in the source code.

 \section links_sec Useful Links

 \link include/itextstream.h include/itextstream.h \endlink - Global output and error message streams, similar to std::cout and std::cerr. \n

 FileInputStream - similar to std::ifstream (binary mode) \n
 FileOutputStream - similar to std::ofstream (binary mode) \n
 TextFileInputStream - similar to std::ifstream (text mode) \n
 TextFileOutputStream - similar to std::ofstream (text mode) \n
 StringOutputStream - similar to std::stringstream \n

 \link string/string.h string/string.h \endlink - C-style string comparison and memory management. \n
 \link os/path.h os/path.h \endlink - Path manipulation for radiant's standard path format \n
 \link os/file.h os/file.h \endlink - OS file-system access. \n

 Array - automatic array memory management \n
 HashTable - generic hashtable, similar to std::hash_map \n

 \link math/Vector2.h math/Vector3.h math/Vector4.h \endlink - Vectors \n
 \link math/matrix.h math/matrix.h \endlink - Matrices \n
 \link math/quaternion.h math/quaternion.h \endlink - Quaternions \n
 \link math/Plane3.h math/Plane3.h \endlink - Planes \n
 \link math/aabb.h math/aabb.h \endlink - AABBs \n

 Callback MemberCaller FunctionCaller - callbacks similar to using boost::function with boost::bind \n
 SmartPointer SmartReference - smart-pointer and smart-reference similar to Loki's SmartPtr \n

 \link generic/bitfield.h generic/bitfield.h \endlink - Type-safe bitfield \n
 \link generic/enumeration.h generic/enumeration.h \endlink - Type-safe enumeration \n

 DefaultAllocator - Memory allocation using new/delete, compliant with std::allocator interface \n

 \link debugging/debugging.h debugging/debugging.h \endlink - Debugging macros \n

 */

#include "version.h"
#include "radiant_i18n.h"

#include "debugging/debugging.h"

#include "iundo.h"

#include "os/file.h"
#include "os/path.h"
#include "stream/stringstream.h"
#include "stream/textfilestream.h"
#include "modulesystem/moduleregistry.h"
#include "stacktrace.h"
#include "gtkutil/messagebox.h"

#include "log/console.h"
#include "sidebar/texturebrowser.h"
#include "sidebar/surfaceinspector/surfaceinspector.h"
#include "map/map.h"
#include "settings/PreferenceDialog.h"
#include "settings/GameManager.h"
#include "referencecache/referencecache.h"
#include "ui/mru/MRU.h"
#include "ui/splash/Splash.h"
#include "ui/mainframe/mainframe.h"
#include "server.h"
#include "environment.h"

#include <gtk/gtk.h>
#include <gtk/gtkgl.h>

#ifdef WIN32
#include <windows.h>
#endif

#include <locale.h>

static void gtk_error_redirect (const gchar *domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
	gboolean in_recursion;
	StringOutputStream buf(256);

	in_recursion = (log_level & G_LOG_FLAG_RECURSION) != 0;
	const gboolean is_fatal = (log_level & G_LOG_FLAG_FATAL) != 0;
	log_level = (GLogLevelFlags) (log_level & G_LOG_LEVEL_MASK);

#ifndef DEBUG
	if (log_level == G_LOG_LEVEL_DEBUG)
		return;
#endif

	if (!message)
		message = "(0) message";

	if (domain)
		buf << domain;
	else
		buf << "**";
	buf << "-";

	switch (log_level) {
	case G_LOG_LEVEL_ERROR:
		if (in_recursion)
			buf << "ERROR (recursed) **: ";
		else
			buf << "ERROR **: ";
		break;
	case G_LOG_LEVEL_CRITICAL:
		if (in_recursion)
			buf << "CRITICAL (recursed) **: ";
		else
			buf << "CRITICAL **: ";
		break;
	case G_LOG_LEVEL_WARNING:
		if (in_recursion)
			buf << "WARNING (recursed) **: ";
		else
			buf << "WARNING **: ";
		break;
	case G_LOG_LEVEL_MESSAGE:
		if (in_recursion)
			buf << "Message (recursed): ";
		else
			buf << "Message: ";
		break;
	case G_LOG_LEVEL_INFO:
		if (in_recursion)
			buf << "INFO (recursed): ";
		else
			buf << "INFO: ";
		break;
	case G_LOG_LEVEL_DEBUG:
		if (in_recursion)
			buf << "DEBUG (recursed): ";
		else
			buf << "DEBUG: ";
		break;
	default:
		/* we are used for a log level that is not defined by GLib itself,
		 * try to make the best out of it. */
		if (in_recursion)
			buf << "LOG (recursed:";
		else
			buf << "LOG (";
		if (log_level) {
			gchar string[] = "0x00): ";
			gchar *p = string + 2;
			const guint i = g_bit_nth_msf(log_level, -1);
			*p++ = i >> 4;
			*p = '0' + (i & 0xf);
			if (*p > '9')
				*p += 'A' - '9' - 1;

			buf << string;
		} else
			buf << "): ";
	}

	buf << message;
	if (is_fatal)
		buf << "aborting...\n";

	// spam it...
	globalOutputStream() << buf.toString();

	if (is_fatal)
		ERROR_MESSAGE("GTK+ error: " << buf.toString());
}

class Lock
{
		bool m_locked;
	public:
		Lock () :
			m_locked(false)
		{
		}
		void lock (void)
		{
			m_locked = true;
		}
		void unlock (void)
		{
			m_locked = false;
		}
		bool locked () const
		{
			return m_locked;
		}
};

class ScopedLock
{
		Lock& m_lock;
	public:
		ScopedLock (Lock& lock) :
			m_lock(lock)
		{
			m_lock.lock();
		}
		~ScopedLock (void)
		{
			m_lock.unlock();
		}
};

class LineLimitedTextOutputStream: public TextOutputStream
{
		TextOutputStream& outputStream;
		std::size_t count;
	public:
		LineLimitedTextOutputStream (TextOutputStream& outputStream, std::size_t count) :
			outputStream(outputStream), count(count)
		{
		}
		std::size_t write (const char* buffer, std::size_t length)
		{
			if (count != 0) {
				const char* p = buffer;
				const char* end = buffer + length;
				for (;;) {
					p = std::find(p, end, '\n');
					if (p == end) {
						break;
					}
					++p;
					if (--count == 0) {
						length = p - buffer;
						break;
					}
				}
				outputStream.write(buffer, length);
			}
			return length;
		}
};

class PopupDebugMessageHandler: public DebugMessageHandler
{
		StringOutputStream m_buffer;
		Lock m_lock;
	public:
		TextOutputStream& getOutputStream (void)
		{
			if (!m_lock.locked()) {
				return m_buffer;
			}
			return globalErrorStream();
		}
		bool handleMessage (void)
		{
			getOutputStream() << "----------------\n";
			LineLimitedTextOutputStream outputStream(getOutputStream(), 24);
			write_stack_trace(outputStream);
			getOutputStream() << "----------------\n";
			globalErrorStream() << m_buffer.toString();
			if (!m_lock.locked()) {
				ScopedLock lock(m_lock);
#ifdef DEBUG
				m_buffer << "Break into the debugger?\n";
				bool handled = gtk_MessageBox(0, m_buffer.toString(), _("Radiant - Runtime Error"), eMB_YESNO, eMB_ICONERROR) == eIDNO;
				m_buffer.clear();
				return handled;
#else
				m_buffer << "Please report this error to the developers\n";
				gtkutil::errorDialog(m_buffer.toString());
				m_buffer.clear();
#endif
			}
			return true;
		}
};

typedef Static<PopupDebugMessageHandler> GlobalPopupDebugMessageHandler;

static void streams_init (void)
{
	GlobalErrorStream::instance().setOutputStream(getSysPrintErrorStream());
	GlobalOutputStream::instance().setOutputStream(getSysPrintOutputStream());
	GlobalWarningStream::instance().setOutputStream(getSysPrintWarningStream());
}

/**
 * @brief now the secondary game dependant .pid file
 */
static void create_local_pid (void)
{
	std::string g_pidGameFile = GlobalRegistry().get(RKEY_SETTINGS_PATH) + "/radiant.pid";

	FILE *pid = fopen(g_pidGameFile.c_str(), "r");
	if (pid != 0) {
		fclose(pid);

		if (!file_remove(g_pidGameFile)) {
			std::string msg = _("WARNING: Could not delete game pid at ") + g_pidGameFile;
			gtkutil::errorDialog(msg);
		}

		// in debug, never prompt to clean registry, turn console logging auto after a failed start
#ifndef DEBUG
		std::string startupFailure = _("UFORadiant failed to start properly the last time it was run.\n"
				"The failure may be caused by current preferences.\n"
				"Do you want to reset all preferences to defaults?");

		if (gtk_MessageBox(0, startupFailure, _("UFORadiant - Startup Failure"), eMB_YESNO, eMB_ICONQUESTION) == eIDYES) {
			GlobalRegistry().reset();
		}

		std::string msg = "Logging console output to " + GlobalRegistry().get(RKEY_SETTINGS_PATH)
				+ "radiant.log\nRefer to the log if Radiant fails to start again.";

		gtkutil::errorDialog(msg);
#endif
	} else {
		// create one, will remove right after entering message loop
		pid = fopen(g_pidGameFile.c_str(), "w");
		if (pid)
			fclose(pid);
	}
}

/**
 * @brief now the secondary game dependant .pid file
 */
static void remove_local_pid (void)
{
	std::string g_pidGameFile = GlobalRegistry().get(RKEY_SETTINGS_PATH) + "/radiant.pid";
	file_remove(g_pidGameFile);
}

void Commands_Init();

int main (int argc, char* argv[])
{
	streams_init();

#ifdef WIN32
	HMODULE lib;
	lib = LoadLibrary("dwmapi.dll");
	if (lib != 0) {
		HRESULT (WINAPI *dwmEnableComposition) (UINT) = (HRESULT (WINAPI *) (UINT)) GetProcAddress(lib, "DwmEnableComposition");
		if (dwmEnableComposition) {
			dwmEnableComposition(FALSE);
		}
		FreeLibrary(lib);
	}
#endif

	/** @todo support system wide locale dirs */
	bindtextdomain(GETTEXT_PACKAGE, "i18n");
	/* set encoding to utf-8 to prevent errors for Windows */
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");

	gtk_init(&argc, &argv);
	gtk_gl_init(&argc, &argv);
	gdk_gl_init(&argc, &argv);

	/* reset some locale settings back to standard c
	 * this is e.g. needed for parsing float values from textfiles */
	setlocale(LC_NUMERIC, "C");
	setlocale(LC_TIME, "C");

	if (!g_thread_supported()) {
		g_thread_init(NULL);
	}

	// redirect Gtk warnings to the console
	g_log_set_handler("Gdk", (GLogLevelFlags) (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING
			| G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION),
			gtk_error_redirect, 0);
	g_log_set_handler("Gtk", (GLogLevelFlags) (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING
			| G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION),
			gtk_error_redirect, 0);
	g_log_set_handler("GtkGLExt", (GLogLevelFlags) (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING
			| G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION),
			gtk_error_redirect, 0);
	g_log_set_handler("GLib", (GLogLevelFlags) (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING
			| G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION),
			gtk_error_redirect, 0);
	g_log_set_handler(0, (GLogLevelFlags) (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING
			| G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION),
			gtk_error_redirect, 0);

	GlobalDebugMessageHandler::instance().setHandler(GlobalPopupDebugMessageHandler::instance());

	// Retrieve the application path and such
	Environment::Instance().init(argc, argv);

	// Initialise and instantiate the registry
	GlobalModuleServer::instance().set(GlobalRadiantModuleServer());
	StaticModuleRegistryList().instance().registerModule("registry");
	GlobalRegistryModuleRef registryRef;

	// Tell the Environment class to store the paths into the Registry
	Environment::Instance().savePathsToRegistry();

	// The settings path is set, start logging now
	Sys_LogFile(true);

	// Try to load all the XML files into the registry
	GlobalRegistry().init();

	ui::Splash::Instance().show();

	create_local_pid();

	Radiant_Initialise();

	g_pParentWnd = new MainFrame();

	Commands_Init();

	ui::SurfaceInspector::Instance().init();

	ui::Splash::Instance().hide();

	if (GlobalMRU().loadLastMap() && GlobalMRU().getLastMapName() != "") {
		GlobalMap().loadFile(GlobalMRU().getLastMapName());
	} else if (Environment::Instance().getArgc() == 2) {
		const std::string mapname = Environment::Instance().getArgv(1);
		if (file_readable(mapname))
			GlobalMap().loadFile(mapname);
		else
			GlobalMap().createNew();
	} else {
		GlobalMap().createNew();
	}

	remove_local_pid();

	// load up shaders now that we have the map loaded
	GlobalTextureBrowser().showStartupShaders();

	// Save the paths *once again* into the registry, to overwrite bogus stuff in there
	Environment::Instance().savePathsToRegistry();

	gtk_main();

	GlobalMap().free();

	delete g_pParentWnd;

	Radiant_Shutdown();

	// close the log file if any
	Sys_LogFile(false);

	return EXIT_SUCCESS;
}
