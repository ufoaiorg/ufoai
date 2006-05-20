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

#include "plugin.h"

#include "debugging/debugging.h"

#include "iplugin.h"
#include "ifilter.h"

#include "filters.h"

#include "string/string.h"
#include "modulesystem/singletonmodule.h"

class filter_face_level : public UfoAIFilter
{
  int m_contents;
public:
  filter_face_level(int contents) : m_contents(contents)
  {
  }
  bool filter(const Face& face) const
  {
    return (face.getShader().m_flags.m_contentFlags & m_contents) == 0;
  }
};

namespace UfoAI
{
  void add_ufoai_filter(EntityFilter& filter, int mask, bool invert)
  {
    GlobalFilterSystem().addFilter(g_entityFilters.back(), mask);
  }
  void construct ( void )
  {
    filter_face_level g_filter_face_level1(CONTENTS_LEVEL1);
    filter_brush_all_faces g_filter_brush_level1(&g_filter_face_level1);
    add_brush_filter(g_filter_brush_level1, EXCLUDE_LEVEL1);

  }
  const char* init(void* hApp, void* pMainWidget)
  {
    return "UFO:AI Filters";
  }
  const char* getName()
  {
    return "UFO:AI Filters";
  }
  const char* getCommandList()
  {
    return "StepOn,-,Level8,Level7,Level6,Level5,Level4,Level3,Level2,Level1";
  }
  const char* getCommandTitleList()
  {
    return "";
  }
  void dispatch(const char* command, float* vMin, float* vMax, bool bSingleBrush)
  {
    globalOutputStream() << "Sample Demo Plugin\n";
    if(string_equal(command, "Level1"))
    {
      globalOutputStream() << "Show only level 1\n";
    }
    else if(string_equal(command, "Level2"))
    {
      globalOutputStream() << "Show only level 2\n";
    }
    else if(string_equal(command, "Level3"))
    {
      globalOutputStream() << "Show only level 3\n";
    }
    else if(string_equal(command, "Level4"))
    {
      globalOutputStream() << "Show only level 4\n";
    }
    else if(string_equal(command, "Level5"))
    {
      globalOutputStream() << "Show only level 5\n";
    }
    else if(string_equal(command, "Level6"))
    {
      globalOutputStream() << "Show only level 6\n";
    }
    else if(string_equal(command, "Level7"))
    {
      globalOutputStream() << "Show only level 7\n";
    }
    else if(string_equal(command, "Level8"))
    {
      globalOutputStream() << "Show only level 8\n";
    }
    else if(string_equal(command, "StepOn"))
    {
      globalOutputStream() << "Hide all stepons\n";
    }
  }

} // namespace


class UfoAIPluginModule
{
  _QERPluginTable m_plugin;
public:
  typedef _QERPluginTable Type;
  STRING_CONSTANT(Name, "UFO:AI Filters");

  UfoAIPluginModule()
  {
    Filter
    m_plugin.m_pfnQERPlug_Init = &UfoAI::init;
    m_plugin.m_pfnQERPlug_GetName = &UfoAI::getName;
    m_plugin.m_pfnQERPlug_GetCommandList = &UfoAI::getCommandList;
    m_plugin.m_pfnQERPlug_GetCommandTitleList = &UfoAI::getCommandTitleList;
    m_plugin.m_pfnQERPlug_Dispatch = &UfoAI::dispatch;
    UfoAI::construct();
  }
  _QERPluginTable* getTable()
  {
    return &m_plugin;
  }
};

typedef SingletonModule<UfoAIPluginModule> SingletonUfoAIPluginModule;

SingletonUfoAIPluginModule g_UfoAIPluginModule;


extern "C" void RADIANT_DLLEXPORT Radiant_RegisterModules(ModuleServer& server)
{
  initialiseModule(server);

  g_UfoAIPluginModule.selfRegister();
}
