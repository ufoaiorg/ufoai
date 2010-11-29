/**
 * @file server.cpp
 */

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

#include "server.h"

#include "debugging/debugging.h"

#include <vector>
#include <map>
#include <iostream>
#include "os/path.h"

#include "modulesystem.h"

class RadiantModuleServer: public ModuleServer
{
		typedef std::pair<std::string, int> ModuleType;
		typedef std::pair<ModuleType, std::string> ModuleKey;
		typedef std::map<ModuleKey, Module*> Modules_;
		Modules_ m_modules;
		bool m_error;

	public:
		RadiantModuleServer () :
			m_error(false)
		{
		}

		void setError (bool error)
		{
			m_error = error;
		}
		bool getError () const
		{
			return m_error;
		}

		TextOutputStream& getOutputStream ()
		{
			return globalOutputStream();
		}
		TextOutputStream& getErrorStream ()
		{
			return globalErrorStream();
		}
		TextOutputStream& getWarningStream ()
		{
			return globalWarningStream();
		}
		DebugMessageHandler& getDebugMessageHandler ()
		{
			return globalDebugMessageHandler();
		}

		void registerModule (const std::string& type, int version, const std::string& name, Module& module)
		{
			ASSERT_NOTNULL(&module);
			if (!m_modules.insert(Modules_::value_type(ModuleKey(ModuleType(type, version), name), &module)).second) {
				g_warning("module already registered: type='%s' name='%s'\n", type.c_str(), name.c_str());
			} else {
				g_message("Module Registered: type='%s' version='%i' name='%s'\n", type.c_str(), version, name.c_str());
			}
		}

		Module* findModule (const std::string& type, int version, const std::string& name) const
		{
			Modules_::const_iterator i = m_modules.find(ModuleKey(ModuleType(type, version), name));
			if (i != m_modules.end()) {
				return (*i).second;
			}
			return 0;
		}

		void foreachModule (const std::string& type, int version, const Visitor& visitor)
		{
			for (Modules_::const_iterator i = m_modules.begin(); i != m_modules.end(); ++i) {
				if ((*i).first.first.first == type) {
					visitor.visit((*i).first.second, *(*i).second);
				}
			}
		}
};

ModuleServer& GlobalRadiantModuleServer ()
{
	static RadiantModuleServer _server;
	return _server;
}
