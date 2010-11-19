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

/*!
 *  \file modulesystem.h
 *
 */

#if !defined(INCLUDED_MODULESYSTEM_H)
#define INCLUDED_MODULESYSTEM_H

#include "generic/static.h"
#include "debugging/debugging.h"

#ifdef _WIN32
#define RADIANT_DLLEXPORT __declspec(dllexport)
#define RADIANT_DLLIMPORT __declspec(dllimport)
#else
#define RADIANT_DLLEXPORT
#define RADIANT_DLLIMPORT
#endif

/*!
 * \class Module
 * \brief Abstract class
 */
class Module
{
	public:
		virtual ~Module ()
		{
		}
		virtual void capture () = 0;
		virtual void release () = 0;
		virtual void* getTable () = 0;
};

/**
 * @brief Shorthand to call the getTable method on module
 */
inline void* Module_getTable (Module& module)
{
	return module.getTable();
}

class TextOutputStream;
class DebugMessageHandler;

/*!
 * \class ModuleServer
 * \brief Abstract class for supporting multiple Module classes.
 */
class ModuleServer
{
	public:
		class Visitor
		{
			public:
				virtual ~Visitor ()
				{
				}
				virtual void visit (const std::string& name, Module& module) const = 0;
		};

		virtual ~ModuleServer ()
		{
		}

		virtual void setError (bool error) = 0;
		virtual bool getError () const = 0;  /*! Tells if the Module has an error  */

		virtual TextOutputStream& getOutputStream () = 0;
		virtual TextOutputStream& getErrorStream () = 0;
		virtual TextOutputStream& getWarningStream () = 0;
		virtual DebugMessageHandler& getDebugMessageHandler () = 0;

		virtual void registerModule (const std::string& type, int version, const std::string& name, Module& module) = 0;
		virtual Module* findModule (const std::string& type, int version, const std::string& name) const = 0;  /*! Seeks for a Module and returns it */
		virtual void foreachModule (const std::string& type, int version, const Visitor& visitor) = 0;
};

/*!
 * \class ModuleServerHolder
 * Wrapper of a ModuleServer
 */
class ModuleServerHolder
{
		ModuleServer* m_server;
	public:
		ModuleServerHolder () :
			m_server(0)
		{
		}
		void set (ModuleServer& server)
		{
			m_server = &server;
		}
		ModuleServer& get ()
		{
			return *m_server;
		}
};

/*!
 * \typedef GlobalModuleServer
 * Static ModuleServerHolder
 */
typedef Static<ModuleServerHolder> GlobalModuleServer;

/*!
 * Returns the internal ModuleServer of a static instance
 */
inline ModuleServer& globalModuleServer ()
{
	return GlobalModuleServer::instance().get();
}

/*!
 *
 * Gets a ModuleServer and links it to the GlobalModuleServer, Global Streams and DebugHandler
 */
inline void initialiseModule (ModuleServer& server)
{
	GlobalErrorStream::instance().setOutputStream(server.getErrorStream());
	GlobalOutputStream::instance().setOutputStream(server.getOutputStream());
	GlobalWarningStream::instance().setOutputStream(server.getWarningStream());
	GlobalDebugMessageHandler::instance().setHandler(server.getDebugMessageHandler());
	GlobalModuleServer::instance().set(server);
}

/*!
 * \class Modules
 * Abstract class that declares a visitor pattern for the template Type.
 */
template<typename Type>
class Modules
{
	public:
		virtual ~Modules ()
		{
		}
		class Visitor
		{
			public:
				virtual ~Visitor ()
				{
				}
				virtual void visit (const std::string& name, const Type& table) const = 0;
		};

		virtual Type* findModule (const std::string& name) = 0;
		virtual void foreachModule (const Visitor& visitor) = 0;
};

#include "debugging/debugging.h"

/*!
 * Initializes this reference seeking on GlobalModuleServer when matching with Type::Name, Type::Version and name of constructor.
 * If found, gets the internal pointers or sets an error.
 */
template<typename Type>
class ModuleRef
{
		Module* m_module;
		Type* m_table;
	public:
		ModuleRef (const char* name) :
			m_table(0)
		{
			if (!globalModuleServer().getError()) {
				m_module = globalModuleServer().findModule(typename Type::Name(), typename Type::Version(), name);
				if (m_module == 0) {
					globalModuleServer().setError(true);
					globalErrorStream() << "ModuleRef::initialise: type=" << makeQuoted(typename Type::Name())
							<< " version=" << makeQuoted(typename Type::Version()) << " name=" << makeQuoted(name)
							<< " - not found\n";
				} else {
					m_module->capture();
					if (!globalModuleServer().getError()) {
						m_table = static_cast<Type*> (m_module->getTable());
					}
				}
			}
		}
		~ModuleRef ()
		{
			if (m_module != 0) {
				m_module->release();
			}
		}
		Type* getTable ()
		{
#if defined(DEBUG)
			ASSERT_MESSAGE(m_table != 0, "ModuleRef::getTable: type=" << makeQuoted(typename Type::Name()) << " version=" << makeQuoted(typename Type::Version()) << " - module-reference used without being initialised");
#endif
			return m_table;
		}
};

/*!
 * Initializes this reference seeking on GlobalModuleServer when matching with Type::Name, Type::Version and name of initialise.
 * If found, gets the internal pointers or sets an error.
 */
template<typename Type>
class SingletonModuleRef
{
		Module* m_module;
		Type* m_table;
	public:

		SingletonModuleRef () :
			m_module(0), m_table(0)
		{
		}

		bool initialised () const
		{
			return m_module != 0;
		}

		/*!
		 * Point of entry of Singleton Module. Gets the module from GlobalModuleServer
		 */
		void initialise (const std::string& name)
		{
			m_module = globalModuleServer().findModule(typename Type::Name(), typename Type::Version(), name);
			if (m_module == 0) {
				globalModuleServer().setError(true);
				globalErrorStream() << "SingletonModuleRef::initialise: type=" << makeQuoted(typename Type::Name())
						<< " version=" << makeQuoted(typename Type::Version()) << " name=" << makeQuoted(name)
						<< " - not found\n";
			}
		}

		Type* getTable ()
		{
#if defined(DEBUG)
			ASSERT_MESSAGE(m_table != 0, "SingletonModuleRef::getTable: type=" << makeQuoted(typename Type::Name()) << " version=" << makeQuoted(typename Type::Version()) << " - module-reference used without being initialised");
#endif
			return m_table;
		}

		/*!
		 * Should be called after initialise, does the actual initialization.
		 */
		void capture ()
		{
			if (initialised()) {
				m_module->capture();
				m_table = static_cast<Type*> (m_module->getTable());
			}
		}
		void release ()
		{
			if (initialised()) {
				m_module->release();
			}
		}
};

template<typename Type>
class GlobalModule
{
		static SingletonModuleRef<Type> m_instance;
	public:
		static SingletonModuleRef<Type>& instance ()
		{
			return m_instance;
		}
		static Type& getTable ()
		{
			return *m_instance.getTable();
		}
};

template<class Type>
SingletonModuleRef<Type> GlobalModule<Type>::m_instance;

template<typename Type>
class GlobalModuleRef
{
	public:
		GlobalModuleRef (const std::string& name = "*")
		{
			if (!globalModuleServer().getError()) {
				GlobalModule<Type>::instance().initialise(name);
			}
			GlobalModule<Type>::instance().capture();
		}
		~GlobalModuleRef ()
		{
			GlobalModule<Type>::instance().release();
		}
		Type& getTable ()
		{
			return GlobalModule<Type>::getTable();
		}
};

#endif
