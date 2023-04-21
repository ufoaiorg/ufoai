# Copyright (C) 2002-2023 UFO: Alien Invasion.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

cmake_minimum_required(VERSION 3.13)

project(uforadiant DESCRIPTION "UFORadiant map editor for UFO:AI.")

find_package(Intl)
find_package(GTK2)
find_package(GTKGL)
find_package(GTKSourceView2)
find_package(LibXml2)
find_package(OGG)
find_package(OpenAL)
find_package(SDL)
find_package(VORBIS)
find_package(ZLIB)

set(UFORADIANT_SOURCES
	../../shared/entitiesdef.cpp
	../../shared/parse.cpp

	libs/Instance.cpp
	libs/gtkutil/MenuItemAccelerator.cpp
	libs/gtkutil/ModalProgressDialog.cpp
	libs/gtkutil/PanedPosition.cpp
	libs/gtkutil/RegistryConnector.cpp
	libs/gtkutil/SourceView.cpp
	libs/gtkutil/TextEntryDialog.cpp
	libs/gtkutil/TextPanel.cpp
	libs/gtkutil/Timer.cpp
	libs/gtkutil/TreeModel.cpp
	libs/gtkutil/VFSTreePopulator.cpp
	libs/gtkutil/WindowPosition.cpp
	libs/gtkutil/button.cpp
	libs/gtkutil/clipboard.cpp
	libs/gtkutil/cursor.cpp
	libs/gtkutil/dialog.cpp
	libs/gtkutil/filechooser.cpp
	libs/gtkutil/frame.cpp
	libs/gtkutil/glfont.cpp
	libs/gtkutil/glwidget.cpp
	libs/gtkutil/image.cpp
	libs/gtkutil/menu.cpp
	libs/gtkutil/menu/PopupMenu.cpp
	libs/gtkutil/messagebox.cpp
	libs/gtkutil/window.cpp
	libs/gtkutil/window/PersistentTransientWindow.cpp
	libs/picomodel/RenderablePicoModel.cpp
	libs/picomodel/RenderablePicoSurface.cpp
	libs/picomodel/model.cpp
	libs/xmlutil/Document.cpp
	libs/xmlutil/Node.cpp
	radiant/brush/Brush.cpp
	radiant/brush/BrushClipPlane.cpp
	radiant/brush/BrushInstance.cpp
	radiant/brush/BrushModule.cpp
	radiant/brush/BrushTokens.cpp
	radiant/brush/ContentsFlagsValue.cpp
	radiant/brush/Face.cpp
	radiant/brush/FaceInstance.cpp
	radiant/brush/FaceShader.cpp
	radiant/brush/FaceTexDef.cpp
	radiant/brush/TexDef.cpp
	radiant/brush/TextureProjection.cpp
	radiant/brush/brushmanip.cpp
	radiant/brush/construct/Cone.cpp
	radiant/brush/construct/Cuboid.cpp
	radiant/brush/construct/Prism.cpp
	radiant/brush/construct/Rock.cpp
	radiant/brush/construct/Sphere.cpp
	radiant/brush/construct/Terrain.cpp
	radiant/brush/csg/csg.cpp
	radiant/brush/winding.cpp
	radiant/brushexport/BrushExportOBJ.cpp
	radiant/camera/CamWnd.cpp
	radiant/camera/Camera.cpp
	radiant/camera/CameraSettings.cpp
	radiant/camera/GlobalCamera.cpp
	radiant/camera/view.cpp
	radiant/clipper/ClipPoint.cpp
	radiant/clipper/Clipper.cpp
	radiant/commands.cpp
	radiant/dialog.cpp
	radiant/entity/EntityCreator.cpp
	radiant/entity/EntitySettings.cpp
	radiant/entity/eclassmodel/EClassModel.cpp
	radiant/entity/eclassmodel/EClassModelInstance.cpp
	radiant/entity/eclassmodel/EClassModelNode.cpp
	radiant/entity/entity.cpp
	radiant/entity/entitydef/EntityClassManager.cpp
	radiant/entity/entitydef/EntityClassMgrModule.cpp
	radiant/entity/entitydef/EntityClassScanner.cpp
	radiant/entity/entitydef/EntityDefinitionModule.cpp
	radiant/entity/entitymodule.cpp
	radiant/entity/generic/GenericEntity.cpp
	radiant/entity/generic/GenericEntityInstance.cpp
	radiant/entity/generic/GenericEntityNode.cpp
	radiant/entity/group/Group.cpp
	radiant/entity/group/GroupInstance.cpp
	radiant/entity/group/GroupNode.cpp
	radiant/entity/light/Light.cpp
	radiant/entity/light/LightInstance.cpp
	radiant/entity/light/LightNode.cpp
	radiant/entity/model/MiscModel.cpp
	radiant/entity/model/MiscModelInstance.cpp
	radiant/entity/model/MiscModelNode.cpp
	radiant/entity/particle/MiscParticle.cpp
	radiant/entity/particle/MiscParticleInstance.cpp
	radiant/entity/particle/MiscParticleNode.cpp
	radiant/entity/sound/MiscSound.cpp
	radiant/entity/sound/MiscSoundInstance.cpp
	radiant/entity/sound/MiscSoundNode.cpp
	radiant/entity/targetable.cpp
	radiant/environment.cpp
	radiant/eventmanager/Accelerator.cpp
	radiant/eventmanager/Command.cpp
	radiant/eventmanager/EventManager.cpp
	radiant/eventmanager/Modifiers.cpp
	radiant/eventmanager/MouseEvents.cpp
	radiant/eventmanager/Toggle.cpp
	radiant/eventmanager/WidgetToggle.cpp
	radiant/exec.cpp
	radiant/filesystem/FileSystem.cpp
	radiant/filesystem/FileSystemModule.cpp
	radiant/filesystem/directory/DirectoryArchive.cpp
	radiant/filesystem/pk3/ZipArchive.cpp
	radiant/filetypes.cpp
	radiant/filters/BasicFilterSystem.cpp
	radiant/filters/LevelFilter.cpp
	radiant/filters/XMLFilter.cpp
	radiant/filters/filters.cpp
	radiant/image.cpp
	radiant/imagemodules.cpp
	radiant/log/console.cpp
	radiant/main.cpp
	radiant/map/AutoSaver.cpp
	radiant/map/MapCompiler.cpp
	radiant/map/MapFileChooserPreview.cpp
	radiant/map/RootNode.cpp
	radiant/map/algorithm/Traverse.cpp
	radiant/map/map.cpp
	radiant/map/mapmodule.cpp
	radiant/map/parse.cpp
	radiant/map/write.cpp
	radiant/material/LicenseParser.cpp
	radiant/material/MaterialModule.cpp
	radiant/material/MaterialShader.cpp
	radiant/material/MaterialShaderModule.cpp
	radiant/material/MaterialShaderSystem.cpp
	radiant/material/MaterialSystem.cpp
	radiant/model.cpp
	radiant/namespace/NameObserver.cpp
	radiant/namespace/Namespace.cpp
	radiant/namespace/NamespaceModule.cpp
	radiant/particles/ParticleParser.cpp
	radiant/particles/ParticleSystem.cpp
	radiant/pathfinding/Pathfinding.cpp
	radiant/pathfinding/PathfindingModule.cpp
	radiant/pathfinding/Routing.cpp
	radiant/pathfinding/RoutingLump.cpp
	radiant/pathfinding/RoutingLumpLoader.cpp
	radiant/pathfinding/RoutingRenderable.cpp
	radiant/plugin.cpp
	radiant/referencecache/nullmodel.cpp
	radiant/referencecache/referencecache.cpp
	radiant/render/OpenGLModule.cpp
	radiant/render/OpenGLRenderSystem.cpp
	radiant/render/backend/OpenGLShader.cpp
	radiant/render/backend/OpenGLShaderPass.cpp
	radiant/render/frontend/SphereRenderable.cpp
	radiant/scenegraph/CompiledGraph.cpp
	radiant/scenegraph/SceneGraphModule.cpp
	radiant/scenegraph/treemodel.cpp
	radiant/script/ScriptLibrary.cpp
	radiant/script/ScriptLibraryModule.cpp
	radiant/script/ScriptTokenWriter.cpp
	radiant/script/ScriptTokeniser.cpp
	radiant/select.cpp
	radiant/selection/BestPoint.cpp
	radiant/selection/Intersection.cpp
	radiant/selection/Manipulatables.cpp
	radiant/selection/Manipulators.cpp
	radiant/selection/Planes.cpp
	radiant/selection/RadiantSelectionSystem.cpp
	radiant/selection/RadiantWindowObserver.cpp
	radiant/selection/SelectionSystemModule.cpp
	radiant/selection/SelectionTest.cpp
	radiant/selection/TransformationVisitors.cpp
	radiant/selection/algorithm/Entity.cpp
	radiant/selection/algorithm/General.cpp
	radiant/selection/algorithm/Group.cpp
	radiant/selection/algorithm/Primitives.cpp
	radiant/selection/algorithm/Shader.cpp
	radiant/selection/algorithm/Transformation.cpp
	radiant/selection/selectionset/SelectionSet.cpp
	radiant/selection/selectionset/SelectionSetManager.cpp
	radiant/selection/selectionset/SelectionSetToolmenu.cpp
	radiant/selection/shaderclipboard/ShaderClipboard.cpp
	radiant/selection/shaderclipboard/Texturable.cpp
	radiant/server.cpp
	radiant/settings/GameDescription.cpp
	radiant/settings/GameManager.cpp
	radiant/settings/GameManagerModule.cpp
	radiant/settings/PreferenceDialog.cpp
	radiant/settings/Win32Registry.cpp
	radiant/sidebar/JobInfo.cpp
	radiant/sidebar/MapInfo.cpp
	radiant/sidebar/PrefabSelector.cpp
	radiant/sidebar/entityinspector/AddPropertyDialog.cpp
	radiant/sidebar/entityinspector/BooleanPropertyEditor.cpp
	radiant/sidebar/entityinspector/ColourPropertyEditor.cpp
	radiant/sidebar/entityinspector/EntityInspector.cpp
	radiant/sidebar/entityinspector/EntityPropertyEditor.cpp
	radiant/sidebar/entityinspector/FloatPropertyEditor.cpp
	radiant/sidebar/entityinspector/ModelPropertyEditor.cpp
	radiant/sidebar/entityinspector/ParticlePropertyEditor.cpp
	radiant/sidebar/entityinspector/PropertyEditor.cpp
	radiant/sidebar/entityinspector/PropertyEditorFactory.cpp
	radiant/sidebar/entityinspector/SkinChooser.cpp
	radiant/sidebar/entityinspector/SkinPropertyEditor.cpp
	radiant/sidebar/entityinspector/SoundPropertyEditor.cpp
	radiant/sidebar/entityinspector/SpawnflagsPropertyEditor.cpp
	radiant/sidebar/entityinspector/Vector3PropertyEditor.cpp
	radiant/sidebar/entitylist/EntityList.cpp
	radiant/sidebar/entitylist/GraphTreeModel.cpp
	radiant/sidebar/sidebar.cpp
	radiant/sidebar/surfaceinspector/surfaceinspector.cpp
	radiant/sidebar/texturebrowser.cpp
	radiant/sound/SoundManager.cpp
	radiant/sound/SoundManagerModule.cpp
	radiant/sound/SoundPlayer.cpp
	radiant/stacktrace.cpp
	radiant/textool/TexTool.cpp
	radiant/textool/TexToolItem.cpp
	radiant/textool/item/BrushItem.cpp
	radiant/textool/item/FaceItem.cpp
	radiant/textool/item/FaceVertexItem.cpp
	radiant/textures/Texture.cpp
	radiant/textures/TextureManipulator.cpp
	radiant/textures/TexturesMap.cpp
	radiant/textures/TexturesModule.cpp
	radiant/timer.cpp
	radiant/ufoscript/UFOScript.cpp
	radiant/ufoscript/common/DataBlock.cpp
	radiant/ufoscript/common/Parser.cpp
	radiant/ufoscript/common/ScriptValues.cpp
	radiant/ufoscript/mapdef/MapDef.cpp
	radiant/ufoscript/terrain/Terrain.cpp
	radiant/ui/about/AboutDialog.cpp
	radiant/ui/brush/QuerySidesDialog.cpp
	radiant/ui/colourscheme/ColourScheme.cpp
	radiant/ui/colourscheme/ColourSchemeEditor.cpp
	radiant/ui/colourscheme/ColourSchemeManager.cpp
	radiant/ui/commandlist/CommandList.cpp
	radiant/ui/commandlist/ShortcutChooser.cpp
	radiant/ui/common/MapPreview.cpp
	radiant/ui/common/MaterialDefinitionView.cpp
	radiant/ui/common/ModelPreview.cpp
	radiant/ui/common/RenderableAABB.cpp
	radiant/ui/common/ShaderChooser.cpp
	radiant/ui/common/ShaderSelector.cpp
	radiant/ui/common/SoundChooser.cpp
	radiant/ui/common/SoundPreview.cpp
	radiant/ui/common/TexturePreviewCombo.cpp
	radiant/ui/common/ToolbarCreator.cpp
	radiant/ui/common/UFOScriptDefinitionView.cpp
	radiant/ui/common/UMPDefinitionView.cpp
	radiant/ui/filterdialog/FilterDialog.cpp
	radiant/ui/filterdialog/FilterEditor.cpp
	radiant/ui/findbrush/findbrush.cpp
	radiant/ui/findshader/FindShader.cpp
	radiant/ui/lightdialog/LightDialog.cpp
	radiant/ui/mainframe/SplitPaneLayout.cpp
	radiant/ui/mainframe/mainframe.cpp
	radiant/ui/maptools/ErrorCheckDialog.cpp
	radiant/ui/materialeditor/MaterialEditor.cpp
	radiant/ui/menu/FiltersMenu.cpp
	radiant/ui/menu/UMPMenu.cpp
	radiant/ui/modelselector/ModelSelector.cpp
	radiant/ui/mru/MRU.cpp
	radiant/ui/mru/MRUMenuItem.cpp
	radiant/ui/ortho/EntityClassChooser.cpp
	radiant/ui/ortho/OrthoContextMenu.cpp
	radiant/ui/overlay/Overlay.cpp
	radiant/ui/particles/ParticleEditor.cpp
	radiant/ui/particles/ParticlePreview.cpp
	radiant/ui/particles/ParticleSelector.cpp
	radiant/ui/scripteditor/UFOScriptEditor.cpp
	radiant/ui/splash/Splash.cpp
	radiant/ui/textureoverview/TextureOverviewDialog.cpp
	radiant/ui/transform/TransformDialog.cpp
	radiant/ui/uimanager/MenuItem.cpp
	radiant/ui/uimanager/MenuManager.cpp
	radiant/ui/uimanager/UIManager.cpp
	radiant/ui/umpeditor/UMPEditor.cpp
	radiant/ump/UMPAssembly.cpp
	radiant/ump/UMPFile.cpp
	radiant/ump/UMPModule.cpp
	radiant/ump/UMPSystem.cpp
	radiant/ump/UMPTile.cpp
	radiant/undo/RadiantUndoSystem.cpp
	radiant/undo/RadiantUndoSystemModule.cpp
	radiant/undo/UndoStateTracker.cpp
	radiant/url.cpp
	radiant/windowobservers.cpp
	radiant/xmlregistry/RegistryTree.cpp
	radiant/xmlregistry/XMLRegistry.cpp
	radiant/xmlregistry/XMLRegistryModule.cpp
	radiant/xyview/GlobalXYWnd.cpp
	radiant/xyview/XYWnd.cpp
	radiant/xyview/grid/Grid.cpp

	../../libs/picomodel/picointernal.c
	../../libs/picomodel/picomodel.c
	../../libs/picomodel/picomodules.c
	../../libs/picomodel/pm_ase.c
	../../libs/picomodel/pm_md2.c
	../../libs/picomodel/pm_md3.c
	../../libs/picomodel/pm_obj.c
)

# platform specific sources
if(WIN32)
	list(APPEND UFORADIANT_SOURCES
		"radiant/radiant.rc"
	)
endif()

# uforadiant is executable
add_executable(${PROJECT_NAME} EXCLUDE_FROM_ALL ${UFORADIANT_SOURCES})

# For generic no-config case (e.g. with mingw, gcc, ...)
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/radiant)
# For multi-config builds like msvc
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
	set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/radiant)
endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

target_compile_definitions(${PROJECT_NAME} PRIVATE -DCOMPILE_MAP)
target_compile_definitions(${PROJECT_NAME} PRIVATE -DPATH_UFORADIANTICON_FILE="${CMAKE_SOURCE_DIR}/build/projects/radiant.ico")

target_include_directories(${PROJECT_NAME}
	PRIVATE include
	PRIVATE libs
	PRIVATE ../../libs/picomodel
	PRIVATE ${SDL_INCLUDE_DIR}
	PRIVATE ${GTK2_INCLUDE_DIRS}
	PRIVATE ${GTKSourceView2_INCLUDE_DIR}
	PRIVATE ${GTKGL_INCLUDE_DIRS}
	PRIVATE ${LIBXML2_INCLUDE_DIR}
	PRIVATE ${OPENAL_INCLUDE_DIR}
	PRIVATE ${VORBIS_INCLUDE_DIR}
	PRIVATE ${OGG_INCLUDE_DIR}
	PRIVATE ${Intl_INCLUDE_DIRS}
	PRIVATE ${ZLIB_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
	${GTK2_LIBRARIES}
	${GTKSourceView2_LIBRARY}
	${GTKGL_LIBRARIES}
	${LIBXML2_LIBRARY}
	${VORBIS_LIBRARY_FILE}
	${VORBIS_LIBRARY}
	${OGG_LIBRARIES}
	${OPENAL_LIBRARY}
	${JPEG_LIBRARIES}
	${PNG_LIBRARIES}
	${ZLIB_LIBRARIES}
	${CMAKE_DL_LIBS}
)

if(WIN32)
	target_link_libraries(${PROJECT_NAME}
		opengl32
		imm32
		${Intl_LIBRARIES}
		version
		winmm
	)
elseif(APPLE)
	target_link_libraries(${PROJECT_NAME}
		${Intl_LIBRARIES}
	)
elseif(UNIX)
	target_link_libraries(${PROJECT_NAME}
		GL
		X11
	)
	if(NOT CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
		target_link_libraries(${PROJECT_NAME}
			rt
		)
	endif()
endif()
