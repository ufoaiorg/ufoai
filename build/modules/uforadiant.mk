TARGET             := uforadiant
RADIANT_BASE       := tools/radiant

# if the linking should be static
$(TARGET)_STATIC   ?= $(STATIC)
ifeq ($($(TARGET)_STATIC),1)
$(TARGET)_LDFLAGS  += -static
endif

$(TARGET)_LINKER   := $(CXX)
$(TARGET)_FILE     := radiant/$(TARGET)$(EXE_EXT)
$(TARGET)_CFLAGS   += -Isrc/$(RADIANT_BASE)/libs -Isrc/$(RADIANT_BASE)/include $(GTK_CFLAGS) $(GLIB_CFLAGS) $(GTK_SOURCEVIEW_CFLAGS) $(GTK_GLEXT_CFLAGS) $(OPENAL_CFLAGS) $(OPENGL_CFLAGS) $(XML2_CFLAGS) $(GDK_PIXBUF_CFLAGS)
$(TARGET)_LDFLAGS  += -lgthread-2.0 -lvorbisfile -lvorbis -logg $(GTK_LIBS) $(GLIB_LIBS) $(GTK_SOURCEVIEW_LIBS) $(GTK_GLEXT_LIBS) $(OPENAL_LIBS) $(OPENGL_LIBS) $(XML2_LIBS) $(GDK_PIXBUF_LIBS) $(SO_LIBS) -lm -lz

$(TARGET)_SRCS      = \
	$(RADIANT_BASE)/radiant/commands.cpp \
	$(RADIANT_BASE)/radiant/dialog.cpp \
	$(RADIANT_BASE)/radiant/environment.cpp \
	$(RADIANT_BASE)/radiant/exec.cpp \
	$(RADIANT_BASE)/radiant/filetypes.cpp \
	$(RADIANT_BASE)/radiant/image.cpp \
	$(RADIANT_BASE)/radiant/imagemodules.cpp \
	$(RADIANT_BASE)/radiant/main.cpp \
	$(RADIANT_BASE)/radiant/plugin.cpp \
	$(RADIANT_BASE)/radiant/select.cpp \
	$(RADIANT_BASE)/radiant/server.cpp \
	$(RADIANT_BASE)/radiant/stacktrace.cpp \
	$(RADIANT_BASE)/radiant/timer.cpp \
	$(RADIANT_BASE)/radiant/url.cpp \
	$(RADIANT_BASE)/radiant/windowobservers.cpp \
	$(RADIANT_BASE)/radiant/model.cpp \
	\
	$(RADIANT_BASE)/radiant/sound/SoundManager.cpp \
	$(RADIANT_BASE)/radiant/sound/SoundManagerModule.cpp \
	$(RADIANT_BASE)/radiant/sound/SoundPlayer.cpp \
	\
	$(RADIANT_BASE)/radiant/script/ScriptLibrary.cpp \
	$(RADIANT_BASE)/radiant/script/ScriptLibraryModule.cpp \
	$(RADIANT_BASE)/radiant/script/ScriptTokeniser.cpp \
	$(RADIANT_BASE)/radiant/script/ScriptTokenWriter.cpp \
	\
	$(RADIANT_BASE)/radiant/undo/RadiantUndoSystem.cpp \
	$(RADIANT_BASE)/radiant/undo/RadiantUndoSystemModule.cpp \
	$(RADIANT_BASE)/radiant/undo/UndoStateTracker.cpp \
	\
	$(RADIANT_BASE)/radiant/material/LicenseParser.cpp \
	$(RADIANT_BASE)/radiant/material/MaterialModule.cpp \
	$(RADIANT_BASE)/radiant/material/MaterialShader.cpp \
	$(RADIANT_BASE)/radiant/material/MaterialShaderModule.cpp \
	$(RADIANT_BASE)/radiant/material/MaterialShaderSystem.cpp \
	$(RADIANT_BASE)/radiant/material/MaterialSystem.cpp \
	\
	$(RADIANT_BASE)/radiant/filesystem/FileSystem.cpp \
	$(RADIANT_BASE)/radiant/filesystem/FileSystemModule.cpp \
	\
	$(RADIANT_BASE)/radiant/filesystem/directory/DirectoryArchive.cpp \
	\
	$(RADIANT_BASE)/radiant/filesystem/pk3/archivezip.cpp \
	$(RADIANT_BASE)/radiant/filesystem/pk3/ZipArchive.cpp \
	\
	$(RADIANT_BASE)/radiant/log/console.cpp \
	\
	$(RADIANT_BASE)/radiant/scenegraph/CompiledGraph.cpp \
	$(RADIANT_BASE)/radiant/scenegraph/SceneGraphModule.cpp \
	$(RADIANT_BASE)/radiant/scenegraph/treemodel.cpp \
	\
	$(RADIANT_BASE)/radiant/settings/PreferenceDialog.cpp \
	$(RADIANT_BASE)/radiant/settings/GameDescription.cpp \
	$(RADIANT_BASE)/radiant/settings/GameManager.cpp \
	$(RADIANT_BASE)/radiant/settings/GameManagerModule.cpp \
	\
	$(RADIANT_BASE)/radiant/clipper/ClipPoint.cpp \
	$(RADIANT_BASE)/radiant/clipper/Clipper.cpp \
	\
	$(RADIANT_BASE)/radiant/eventmanager/Accelerator.cpp \
	$(RADIANT_BASE)/radiant/eventmanager/Command.cpp \
	$(RADIANT_BASE)/radiant/eventmanager/EventManager.cpp \
	$(RADIANT_BASE)/radiant/eventmanager/Modifiers.cpp \
	$(RADIANT_BASE)/radiant/eventmanager/MouseEvents.cpp \
	$(RADIANT_BASE)/radiant/eventmanager/Toggle.cpp \
	$(RADIANT_BASE)/radiant/eventmanager/WidgetToggle.cpp \
	\
	$(RADIANT_BASE)/radiant/render/OpenGLRenderSystem.cpp \
	$(RADIANT_BASE)/radiant/render/OpenGLModule.cpp \
	$(RADIANT_BASE)/radiant/render/backend/OpenGLShader.cpp \
	$(RADIANT_BASE)/radiant/render/backend/OpenGLShaderPass.cpp \
	\
	$(RADIANT_BASE)/radiant/referencecache/nullmodel.cpp \
	$(RADIANT_BASE)/radiant/referencecache/referencecache.cpp \
	\
	$(RADIANT_BASE)/radiant/ufoscript/UFOScript.cpp \
	$(RADIANT_BASE)/radiant/ufoscript/common/Parser.cpp \
	$(RADIANT_BASE)/radiant/ufoscript/common/ScriptValues.cpp \
	$(RADIANT_BASE)/radiant/ufoscript/common/DataBlock.cpp \
	$(RADIANT_BASE)/radiant/ufoscript/terrain/Terrain.cpp \
	$(RADIANT_BASE)/radiant/ufoscript/mapdef/MapDef.cpp \
	\
	$(RADIANT_BASE)/radiant/ump/UMPModule.cpp \
	$(RADIANT_BASE)/radiant/ump/UMPSystem.cpp \
	$(RADIANT_BASE)/radiant/ump/UMPFile.cpp \
	$(RADIANT_BASE)/radiant/ump/UMPTile.cpp \
	$(RADIANT_BASE)/radiant/ump/UMPAssembly.cpp \
	\
	$(RADIANT_BASE)/radiant/map/algorithm/Traverse.cpp \
	$(RADIANT_BASE)/radiant/map/parse.cpp \
	$(RADIANT_BASE)/radiant/map/write.cpp \
	$(RADIANT_BASE)/radiant/map/AutoSaver.cpp \
	$(RADIANT_BASE)/radiant/map/MapCompiler.cpp \
	$(RADIANT_BASE)/radiant/map/map.cpp \
	$(RADIANT_BASE)/radiant/map/MapFileChooserPreview.cpp \
	$(RADIANT_BASE)/radiant/map/mapmodule.cpp \
	$(RADIANT_BASE)/radiant/map/RootNode.cpp \
	\
	$(RADIANT_BASE)/radiant/namespace/NameObserver.cpp \
	$(RADIANT_BASE)/radiant/namespace/Namespace.cpp \
	$(RADIANT_BASE)/radiant/namespace/NamespaceModule.cpp \
	\
	$(RADIANT_BASE)/radiant/xyview/grid/Grid.cpp \
	$(RADIANT_BASE)/radiant/xyview/GlobalXYWnd.cpp \
	$(RADIANT_BASE)/radiant/xyview/XYWnd.cpp \
	\
	$(RADIANT_BASE)/radiant/camera/Camera.cpp \
	$(RADIANT_BASE)/radiant/camera/CameraSettings.cpp \
	$(RADIANT_BASE)/radiant/camera/CamWnd.cpp \
	$(RADIANT_BASE)/radiant/camera/GlobalCamera.cpp \
	$(RADIANT_BASE)/radiant/camera/view.cpp \
	\
	$(RADIANT_BASE)/radiant/brush/BrushClipPlane.cpp \
	$(RADIANT_BASE)/radiant/brush/brushmanip.cpp \
	$(RADIANT_BASE)/radiant/brush/BrushModule.cpp \
	$(RADIANT_BASE)/radiant/brush/ContentsFlagsValue.cpp \
	$(RADIANT_BASE)/radiant/brush/winding.cpp \
	$(RADIANT_BASE)/radiant/brush/Brush.cpp \
	$(RADIANT_BASE)/radiant/brush/BrushInstance.cpp \
	$(RADIANT_BASE)/radiant/brush/BrushTokens.cpp \
	$(RADIANT_BASE)/radiant/brush/Face.cpp \
	$(RADIANT_BASE)/radiant/brush/FaceShader.cpp \
	$(RADIANT_BASE)/radiant/brush/FaceTexDef.cpp \
	$(RADIANT_BASE)/radiant/brush/FaceInstance.cpp \
	$(RADIANT_BASE)/radiant/brush/TextureProjection.cpp \
	$(RADIANT_BASE)/radiant/brush/TexDef.cpp \
	\
	$(RADIANT_BASE)/radiant/brush/csg/csg.cpp \
	\
	$(RADIANT_BASE)/radiant/brush/construct/Cone.cpp \
	$(RADIANT_BASE)/radiant/brush/construct/Cuboid.cpp \
	$(RADIANT_BASE)/radiant/brush/construct/Prism.cpp \
	$(RADIANT_BASE)/radiant/brush/construct/Rock.cpp \
	$(RADIANT_BASE)/radiant/brush/construct/Sphere.cpp \
	$(RADIANT_BASE)/radiant/brush/construct/Terrain.cpp \
	\
	$(RADIANT_BASE)/radiant/brushexport/BrushExportOBJ.cpp \
	\
	$(RADIANT_BASE)/radiant/selection/BestPoint.cpp \
	$(RADIANT_BASE)/radiant/selection/Intersection.cpp \
	$(RADIANT_BASE)/radiant/selection/Manipulatables.cpp \
	$(RADIANT_BASE)/radiant/selection/Manipulators.cpp \
	$(RADIANT_BASE)/radiant/selection/Planes.cpp \
	$(RADIANT_BASE)/radiant/selection/RadiantSelectionSystem.cpp \
	$(RADIANT_BASE)/radiant/selection/RadiantWindowObserver.cpp \
	$(RADIANT_BASE)/radiant/selection/SelectionSystemModule.cpp \
	$(RADIANT_BASE)/radiant/selection/SelectionTest.cpp \
	$(RADIANT_BASE)/radiant/selection/TransformationVisitors.cpp \
	$(RADIANT_BASE)/radiant/selection/algorithm/Entity.cpp \
	$(RADIANT_BASE)/radiant/selection/algorithm/General.cpp \
	$(RADIANT_BASE)/radiant/selection/algorithm/Group.cpp \
	$(RADIANT_BASE)/radiant/selection/algorithm/Primitives.cpp \
	$(RADIANT_BASE)/radiant/selection/algorithm/Shader.cpp \
	$(RADIANT_BASE)/radiant/selection/algorithm/Transformation.cpp \
	$(RADIANT_BASE)/radiant/selection/selectionset/SelectionSet.cpp \
	$(RADIANT_BASE)/radiant/selection/selectionset/SelectionSetManager.cpp \
	$(RADIANT_BASE)/radiant/selection/selectionset/SelectionSetToolmenu.cpp \
	$(RADIANT_BASE)/radiant/selection/shaderclipboard/ShaderClipboard.cpp \
	$(RADIANT_BASE)/radiant/selection/shaderclipboard/Texturable.cpp \
	\
	$(RADIANT_BASE)/radiant/sidebar/entityinspector/AddPropertyDialog.cpp \
	$(RADIANT_BASE)/radiant/sidebar/entityinspector/BooleanPropertyEditor.cpp \
	$(RADIANT_BASE)/radiant/sidebar/entityinspector/ColourPropertyEditor.cpp \
	$(RADIANT_BASE)/radiant/sidebar/entityinspector/EntityInspector.cpp \
	$(RADIANT_BASE)/radiant/sidebar/entityinspector/EntityPropertyEditor.cpp \
	$(RADIANT_BASE)/radiant/sidebar/entityinspector/FloatPropertyEditor.cpp \
	$(RADIANT_BASE)/radiant/sidebar/entityinspector/ModelPropertyEditor.cpp \
	$(RADIANT_BASE)/radiant/sidebar/entityinspector/ParticlePropertyEditor.cpp \
	$(RADIANT_BASE)/radiant/sidebar/entityinspector/PropertyEditor.cpp \
	$(RADIANT_BASE)/radiant/sidebar/entityinspector/PropertyEditorFactory.cpp \
	$(RADIANT_BASE)/radiant/sidebar/entityinspector/SkinChooser.cpp \
	$(RADIANT_BASE)/radiant/sidebar/entityinspector/SkinPropertyEditor.cpp \
	$(RADIANT_BASE)/radiant/sidebar/entityinspector/SoundPropertyEditor.cpp \
	$(RADIANT_BASE)/radiant/sidebar/entityinspector/SpawnflagsPropertyEditor.cpp \
	$(RADIANT_BASE)/radiant/sidebar/entityinspector/Vector3PropertyEditor.cpp \
	\
	$(RADIANT_BASE)/radiant/sidebar/surfaceinspector/surfaceinspector.cpp \
	\
	$(RADIANT_BASE)/radiant/sidebar/entitylist/EntityList.cpp \
	$(RADIANT_BASE)/radiant/sidebar/entitylist/GraphTreeModel.cpp \
	\
	$(RADIANT_BASE)/radiant/sidebar/sidebar.cpp \
	$(RADIANT_BASE)/radiant/sidebar/PrefabSelector.cpp \
	$(RADIANT_BASE)/radiant/sidebar/MapInfo.cpp \
	$(RADIANT_BASE)/radiant/sidebar/JobInfo.cpp \
	$(RADIANT_BASE)/radiant/sidebar/texturebrowser.cpp \
	\
	$(RADIANT_BASE)/radiant/textures/Texture.cpp \
	$(RADIANT_BASE)/radiant/textures/TextureManipulator.cpp \
	$(RADIANT_BASE)/radiant/textures/TexturesMap.cpp \
	$(RADIANT_BASE)/radiant/textures/TexturesModule.cpp \
	\
	$(RADIANT_BASE)/radiant/particles/ParticleParser.cpp \
	$(RADIANT_BASE)/radiant/particles/ParticleSystem.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/findbrush/findbrush.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/filterdialog/FilterDialog.cpp \
	$(RADIANT_BASE)/radiant/ui/filterdialog/FilterEditor.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/maptools/ErrorCheckDialog.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/about/AboutDialog.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/brush/QuerySidesDialog.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/transform/TransformDialog.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/colourscheme/ColourScheme.cpp \
	$(RADIANT_BASE)/radiant/ui/colourscheme/ColourSchemeManager.cpp \
	$(RADIANT_BASE)/radiant/ui/colourscheme/ColourSchemeEditor.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/common/MapPreview.cpp \
	$(RADIANT_BASE)/radiant/ui/common/ModelPreview.cpp \
	$(RADIANT_BASE)/radiant/ui/common/MaterialDefinitionView.cpp \
	$(RADIANT_BASE)/radiant/ui/common/UMPDefinitionView.cpp \
	$(RADIANT_BASE)/radiant/ui/common/UFOScriptDefinitionView.cpp \
	$(RADIANT_BASE)/radiant/ui/common/RenderableAABB.cpp \
	$(RADIANT_BASE)/radiant/ui/common/ShaderChooser.cpp \
	$(RADIANT_BASE)/radiant/ui/common/ShaderSelector.cpp \
	$(RADIANT_BASE)/radiant/ui/common/SoundChooser.cpp \
	$(RADIANT_BASE)/radiant/ui/common/SoundPreview.cpp \
	$(RADIANT_BASE)/radiant/ui/common/TexturePreviewCombo.cpp \
	$(RADIANT_BASE)/radiant/ui/common/ToolbarCreator.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/mainframe/SplitPaneLayout.cpp \
	$(RADIANT_BASE)/radiant/ui/mainframe/mainframe.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/mru/MRU.cpp \
	$(RADIANT_BASE)/radiant/ui/mru/MRUMenuItem.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/splash/Splash.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/findshader/FindShader.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/commandlist/CommandList.cpp \
	$(RADIANT_BASE)/radiant/ui/commandlist/ShortcutChooser.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/menu/FiltersMenu.cpp \
	$(RADIANT_BASE)/radiant/ui/menu/UMPMenu.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/overlay/Overlay.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/uimanager/MenuManager.cpp \
	$(RADIANT_BASE)/radiant/ui/uimanager/MenuItem.cpp \
	$(RADIANT_BASE)/radiant/ui/uimanager/UIManager.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/modelselector/ModelSelector.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/materialeditor/MaterialEditor.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/scripteditor/UFOScriptEditor.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/textureoverview/TextureOverviewDialog.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/umpeditor/UMPEditor.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/particles/ParticleEditor.cpp \
	$(RADIANT_BASE)/radiant/ui/particles/ParticlePreview.cpp \
	$(RADIANT_BASE)/radiant/ui/particles/ParticleSelector.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/ortho/EntityClassChooser.cpp \
	$(RADIANT_BASE)/radiant/ui/ortho/OrthoContextMenu.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/lightdialog/LightDialog.cpp \
	\
	$(RADIANT_BASE)/radiant/filters/BasicFilterSystem.cpp \
	$(RADIANT_BASE)/radiant/filters/filters.cpp \
	$(RADIANT_BASE)/radiant/filters/LevelFilter.cpp \
	$(RADIANT_BASE)/radiant/filters/XMLFilter.cpp \
	\
	$(RADIANT_BASE)/radiant/textool/TexTool.cpp \
	$(RADIANT_BASE)/radiant/textool/TexToolItem.cpp \
	$(RADIANT_BASE)/radiant/textool/item/BrushItem.cpp \
	$(RADIANT_BASE)/radiant/textool/item/FaceItem.cpp \
	$(RADIANT_BASE)/radiant/textool/item/FaceVertexItem.cpp \
	\
	$(RADIANT_BASE)/radiant/pathfinding/Pathfinding.cpp \
	$(RADIANT_BASE)/radiant/pathfinding/PathfindingModule.cpp \
	$(RADIANT_BASE)/radiant/pathfinding/Routing.cpp \
	$(RADIANT_BASE)/radiant/pathfinding/RoutingLumpLoader.cpp \
	$(RADIANT_BASE)/radiant/pathfinding/RoutingLump.cpp \
	$(RADIANT_BASE)/radiant/pathfinding/RoutingRenderable.cpp \
	\
	$(RADIANT_BASE)/radiant/xmlregistry/RegistryTree.cpp \
	$(RADIANT_BASE)/radiant/xmlregistry/XMLRegistry.cpp \
	$(RADIANT_BASE)/radiant/xmlregistry/XMLRegistryModule.cpp \
	\
	$(RADIANT_BASE)/radiant/entity/entitydef/EntityClassManager.cpp \
	$(RADIANT_BASE)/radiant/entity/entitydef/EntityClassMgrModule.cpp \
	$(RADIANT_BASE)/radiant/entity/entitydef/EntityClassScanner.cpp \
	$(RADIANT_BASE)/radiant/entity/entitydef/EntityDefinitionModule.cpp \
	$(RADIANT_BASE)/radiant/entity/EntitySettings.cpp \
	$(RADIANT_BASE)/radiant/entity/entity.cpp \
	$(RADIANT_BASE)/radiant/entity/entitymodule.cpp \
	$(RADIANT_BASE)/radiant/entity/EntityCreator.cpp \
	$(RADIANT_BASE)/radiant/entity/eclassmodel.cpp \
	$(RADIANT_BASE)/radiant/entity/generic.cpp \
	$(RADIANT_BASE)/radiant/entity/group.cpp \
	$(RADIANT_BASE)/radiant/entity/miscmodel.cpp \
	$(RADIANT_BASE)/radiant/entity/miscparticle.cpp \
	$(RADIANT_BASE)/radiant/entity/miscsound.cpp \
	$(RADIANT_BASE)/radiant/entity/light.cpp \
	$(RADIANT_BASE)/radiant/entity/targetable.cpp \
	\
	$(RADIANT_BASE)/libs/gtkutil/button.cpp \
	$(RADIANT_BASE)/libs/gtkutil/clipboard.cpp \
	$(RADIANT_BASE)/libs/gtkutil/cursor.cpp \
	$(RADIANT_BASE)/libs/gtkutil/dialog.cpp \
	$(RADIANT_BASE)/libs/gtkutil/filechooser.cpp \
	$(RADIANT_BASE)/libs/gtkutil/frame.cpp \
	$(RADIANT_BASE)/libs/gtkutil/glfont.cpp \
	$(RADIANT_BASE)/libs/gtkutil/glwidget.cpp \
	$(RADIANT_BASE)/libs/gtkutil/image.cpp \
	$(RADIANT_BASE)/libs/gtkutil/menu.cpp \
	$(RADIANT_BASE)/libs/gtkutil/messagebox.cpp \
	$(RADIANT_BASE)/libs/gtkutil/window.cpp \
	$(RADIANT_BASE)/libs/gtkutil/MenuItemAccelerator.cpp \
	$(RADIANT_BASE)/libs/gtkutil/ModalProgressDialog.cpp \
	$(RADIANT_BASE)/libs/gtkutil/PanedPosition.cpp \
	$(RADIANT_BASE)/libs/gtkutil/RegistryConnector.cpp \
	$(RADIANT_BASE)/libs/gtkutil/SourceView.cpp \
	$(RADIANT_BASE)/libs/gtkutil/TextPanel.cpp \
	$(RADIANT_BASE)/libs/gtkutil/Timer.cpp \
	$(RADIANT_BASE)/libs/gtkutil/TreeModel.cpp \
	$(RADIANT_BASE)/libs/gtkutil/VFSTreePopulator.cpp \
	$(RADIANT_BASE)/libs/gtkutil/WindowPosition.cpp \
	$(RADIANT_BASE)/libs/gtkutil/menu/PopupMenu.cpp \
	$(RADIANT_BASE)/libs/gtkutil/window/PersistentTransientWindow.cpp \
	\
	$(RADIANT_BASE)/libs/picomodel/model.cpp \
	$(RADIANT_BASE)/libs/picomodel/RenderablePicoSurface.cpp \
	$(RADIANT_BASE)/libs/picomodel/RenderablePicoModel.cpp \
	\
	$(RADIANT_BASE)/libs/xmlutil/Document.cpp \
	$(RADIANT_BASE)/libs/xmlutil/Node.cpp \
	\
	$(RADIANT_BASE)/libs/Instance.cpp \
	\
	shared/parse.c \
	shared/entitiesdef.c \
	$(RADIANT_BASE)/libs/picomodel/picointernal.c \
	$(RADIANT_BASE)/libs/picomodel/picomodel.c \
	$(RADIANT_BASE)/libs/picomodel/picomodules.c \
	$(RADIANT_BASE)/libs/picomodel/pm_ase.c \
	$(RADIANT_BASE)/libs/picomodel/pm_md3.c \
	$(RADIANT_BASE)/libs/picomodel/pm_obj.c \
	$(RADIANT_BASE)/libs/picomodel/pm_md2.c

ifeq ($(TARGET_OS),mingw32)
	$(TARGET)_SRCS += $(RADIANT_BASE)/radiant/radiant.rc
endif

$(TARGET)_OBJS     := $(call ASSEMBLE_OBJECTS,$(TARGET))
$(TARGET)_CXXFLAGS := $($(TARGET)_CFLAGS)
$(TARGET)_CCFLAGS  := $($(TARGET)_CFLAGS)
