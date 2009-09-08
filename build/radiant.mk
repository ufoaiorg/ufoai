########################################################################################################
# radiant
########################################################################################################

RADIANT_BASE = tools/radiant

RADIANT_CFLAGS+=-Isrc/$(RADIANT_BASE)/libs -Isrc/$(RADIANT_BASE)/include
RADIANT_LIBS+=-lgthread-2.0 $(OPENAL_LIBS) -lvorbisfile -lvorbis -logg

RADIANT_SRCS_CPP = \
	$(RADIANT_BASE)/radiant/autosave.cpp \
	$(RADIANT_BASE)/radiant/brush.cpp \
	$(RADIANT_BASE)/radiant/brushmanip.cpp \
	$(RADIANT_BASE)/radiant/brushmodule.cpp \
	$(RADIANT_BASE)/radiant/brush_primit.cpp \
	$(RADIANT_BASE)/radiant/camwindow.cpp \
	$(RADIANT_BASE)/radiant/colorscheme.cpp \
	$(RADIANT_BASE)/radiant/commands.cpp \
	$(RADIANT_BASE)/radiant/console.cpp \
	$(RADIANT_BASE)/radiant/csg.cpp \
	$(RADIANT_BASE)/radiant/dialog.cpp \
	$(RADIANT_BASE)/radiant/eclass.cpp \
	$(RADIANT_BASE)/radiant/eclass_def.cpp \
	$(RADIANT_BASE)/radiant/entity.cpp \
	$(RADIANT_BASE)/radiant/environment.cpp \
	$(RADIANT_BASE)/radiant/error.cpp \
	$(RADIANT_BASE)/radiant/exec.cpp \
	$(RADIANT_BASE)/radiant/filetypes.cpp \
	$(RADIANT_BASE)/radiant/filters.cpp \
	$(RADIANT_BASE)/radiant/grid.cpp \
	$(RADIANT_BASE)/radiant/gtkmisc.cpp \
	$(RADIANT_BASE)/radiant/image.cpp \
	$(RADIANT_BASE)/radiant/lastused.cpp \
	$(RADIANT_BASE)/radiant/material.cpp \
	$(RADIANT_BASE)/radiant/particles.cpp \
	$(RADIANT_BASE)/radiant/main.cpp \
	$(RADIANT_BASE)/radiant/mainframe.cpp \
	$(RADIANT_BASE)/radiant/map.cpp \
	$(RADIANT_BASE)/radiant/nullmodel.cpp \
	$(RADIANT_BASE)/radiant/parse.cpp \
	$(RADIANT_BASE)/radiant/pathfinding.cpp \
	$(RADIANT_BASE)/radiant/plugin.cpp \
	$(RADIANT_BASE)/radiant/pluginmenu.cpp \
	$(RADIANT_BASE)/radiant/plugintoolbar.cpp \
	$(RADIANT_BASE)/radiant/preferences.cpp \
	$(RADIANT_BASE)/radiant/qe3.cpp \
	$(RADIANT_BASE)/radiant/qgl.cpp \
	$(RADIANT_BASE)/radiant/referencecache.cpp \
	$(RADIANT_BASE)/radiant/renderstate.cpp \
	$(RADIANT_BASE)/radiant/scenegraph.cpp \
	$(RADIANT_BASE)/radiant/select.cpp \
	$(RADIANT_BASE)/radiant/selection.cpp \
	$(RADIANT_BASE)/radiant/server.cpp \
	$(RADIANT_BASE)/radiant/stacktrace.cpp \
	$(RADIANT_BASE)/radiant/sound.cpp \
	$(RADIANT_BASE)/radiant/texmanip.cpp \
	$(RADIANT_BASE)/radiant/textures.cpp \
	$(RADIANT_BASE)/radiant/timer.cpp \
	$(RADIANT_BASE)/radiant/toolbars.cpp \
	$(RADIANT_BASE)/radiant/treemodel.cpp \
	$(RADIANT_BASE)/radiant/undo.cpp \
	$(RADIANT_BASE)/radiant/url.cpp \
	$(RADIANT_BASE)/radiant/view.cpp \
	$(RADIANT_BASE)/radiant/winding.cpp \
	$(RADIANT_BASE)/radiant/windowobservers.cpp \
	$(RADIANT_BASE)/radiant/xywindow.cpp \
	$(RADIANT_BASE)/radiant/levelfilters.cpp \
	\
	$(RADIANT_BASE)/radiant/brushexport/BrushExportOBJ.cpp \
	\
	$(RADIANT_BASE)/radiant/selection/BestPoint.cpp \
	$(RADIANT_BASE)/radiant/selection/Intersection.cpp \
	$(RADIANT_BASE)/radiant/selection/Manipulatables.cpp \
	$(RADIANT_BASE)/radiant/selection/Manipulators.cpp \
	\
	$(RADIANT_BASE)/radiant/plugin/PluginManager.cpp \
	$(RADIANT_BASE)/radiant/plugin/PluginSlots.cpp \
	\
	$(RADIANT_BASE)/radiant/sidebar/sidebar.cpp \
	$(RADIANT_BASE)/radiant/sidebar/entitylist.cpp \
	$(RADIANT_BASE)/radiant/sidebar/entityinspector.cpp \
	$(RADIANT_BASE)/radiant/sidebar/surfaceinspector.cpp \
	$(RADIANT_BASE)/radiant/sidebar/prefabs.cpp \
	$(RADIANT_BASE)/radiant/sidebar/mapinfo.cpp \
	$(RADIANT_BASE)/radiant/sidebar/jobinfo.cpp \
	$(RADIANT_BASE)/radiant/sidebar/texturebrowser.cpp \
	$(RADIANT_BASE)/radiant/sidebar/ParticleBrowser.cpp \
	\
	$(RADIANT_BASE)/radiant/dialogs/texteditor.cpp \
	$(RADIANT_BASE)/radiant/dialogs/about.cpp \
	$(RADIANT_BASE)/radiant/dialogs/findbrush.cpp \
	$(RADIANT_BASE)/radiant/dialogs/prism.cpp \
	$(RADIANT_BASE)/radiant/dialogs/light.cpp \
	$(RADIANT_BASE)/radiant/dialogs/maptools.cpp \
	$(RADIANT_BASE)/radiant/dialogs/particle.cpp \
	$(RADIANT_BASE)/radiant/dialogs/findtextures.cpp \
	$(RADIANT_BASE)/radiant/dialogs/ModelSelector.cpp \
	\
	$(RADIANT_BASE)/radiant/ui/common/ModelPreview.cpp \
	$(RADIANT_BASE)/radiant/ui/common/RenderableAABB.cpp \
	$(RADIANT_BASE)/radiant/ui/common/TexturePreviewCombo.cpp \
	\
	$(RADIANT_BASE)/radiant/pathfinding/Routing.cpp \
	$(RADIANT_BASE)/radiant/pathfinding/RoutingLumpLoader.cpp \
	$(RADIANT_BASE)/radiant/pathfinding/RoutingLump.cpp \
	$(RADIANT_BASE)/radiant/pathfinding/RoutingRenderable.cpp \
	\
	$(RADIANT_BASE)/libs/gtkutil/accelerator.cpp \
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
	$(RADIANT_BASE)/libs/gtkutil/paned.cpp \
	$(RADIANT_BASE)/libs/gtkutil/timer.cpp \
	$(RADIANT_BASE)/libs/gtkutil/toolbar.cpp \
	$(RADIANT_BASE)/libs/gtkutil/window.cpp \
	$(RADIANT_BASE)/libs/gtkutil/ModelProgressDialog.cpp \
	\
	$(RADIANT_BASE)/libs/profile/profile.cpp \
	$(RADIANT_BASE)/libs/profile/file.cpp \
	$(RADIANT_BASE)/libs/sound/soundmanager.cpp \
	$(RADIANT_BASE)/libs/sound/soundplayer.cpp

RADIANT_SRCS_C = \
	shared/parse.c \
	shared/entitiesdef.c

RADIANT_CPP_OBJS=$(RADIANT_SRCS_CPP:%.cpp=$(BUILDDIR)/tools/radiant/%.o)
RADIANT_C_OBJS=$(RADIANT_SRCS_C:%.c=$(BUILDDIR)/tools/radiant_c/%.o)
RADIANT_TARGET=radiant/uforadiant$(EXE_EXT)

# model plugin
RADIANT_PLUGIN_MODEL_SRCS_CPP = \
	$(RADIANT_BASE)/plugins/model/plugin.cpp \
	$(RADIANT_BASE)/plugins/model/model.cpp \
	$(RADIANT_BASE)/plugins/model/RenderablePicoSurface.cpp \
	$(RADIANT_BASE)/plugins/model/RenderablePicoModel.cpp
RADIANT_PLUGIN_MODEL_SRCS_C = \
	$(RADIANT_BASE)/libs/picomodel/picointernal.c \
	$(RADIANT_BASE)/libs/picomodel/picomodel.c \
	$(RADIANT_BASE)/libs/picomodel/picomodules.c \
	$(RADIANT_BASE)/libs/picomodel/pm_ase.c \
	$(RADIANT_BASE)/libs/picomodel/pm_md3.c \
	$(RADIANT_BASE)/libs/picomodel/pm_obj.c \
	$(RADIANT_BASE)/libs/picomodel/pm_md2.c

RADIANT_PLUGIN_MODEL_C_OBJS=$(RADIANT_PLUGIN_MODEL_SRCS_C:%.c=$(BUILDDIR)/tools/radiant/plugins_c/%.o)
RADIANT_PLUGIN_MODEL_CPP_OBJS=$(RADIANT_PLUGIN_MODEL_SRCS_CPP:%.cpp=$(BUILDDIR)/tools/radiant/plugins_cpp/%.o)
RADIANT_PLUGIN_MODEL_TARGET=radiant/modules/model.$(SHARED_EXT)

#entity plugin
RADIANT_PLUGIN_ENTITY_SRCS_CPP = \
	$(RADIANT_BASE)/plugins/entity/plugin.cpp \
	$(RADIANT_BASE)/plugins/entity/entity.cpp \
	$(RADIANT_BASE)/plugins/entity/eclassmodel.cpp \
	$(RADIANT_BASE)/plugins/entity/generic.cpp \
	$(RADIANT_BASE)/plugins/entity/group.cpp \
	$(RADIANT_BASE)/plugins/entity/miscmodel.cpp \
	$(RADIANT_BASE)/plugins/entity/miscparticle.cpp \
	$(RADIANT_BASE)/plugins/entity/miscsound.cpp \
	$(RADIANT_BASE)/plugins/entity/filters.cpp \
	$(RADIANT_BASE)/plugins/entity/light.cpp \
	$(RADIANT_BASE)/plugins/entity/targetable.cpp

RADIANT_PLUGIN_ENTITY_CPP_OBJS=$(RADIANT_PLUGIN_ENTITY_SRCS_CPP:%.cpp=$(BUILDDIR)/tools/radiant/plugins_cpp/%.o)
RADIANT_PLUGIN_ENTITY_TARGET=radiant/modules/entity.$(SHARED_EXT)

#image plugin
RADIANT_PLUGIN_IMAGE_SRCS_CPP = \
	$(RADIANT_BASE)/plugins/image/image.cpp

RADIANT_PLUGIN_IMAGE_CPP_OBJS=$(RADIANT_PLUGIN_IMAGE_SRCS_CPP:%.cpp=$(BUILDDIR)/tools/radiant/plugins_cpp/%.o)
RADIANT_PLUGIN_IMAGE_TARGET=radiant/modules/image.$(SHARED_EXT)

#map plugin
RADIANT_PLUGIN_MAP_SRCS_CPP = \
	$(RADIANT_BASE)/plugins/map/plugin.cpp \
	$(RADIANT_BASE)/plugins/map/parse.cpp \
	$(RADIANT_BASE)/plugins/map/write.cpp

RADIANT_PLUGIN_MAP_CPP_OBJS=$(RADIANT_PLUGIN_MAP_SRCS_CPP:%.cpp=$(BUILDDIR)/tools/radiant/plugins_cpp/%.o)
RADIANT_PLUGIN_MAP_TARGET=radiant/modules/map.$(SHARED_EXT)

#shaders plugin
RADIANT_PLUGIN_SHADERS_SRCS_CPP = \
	$(RADIANT_BASE)/plugins/shaders/shaders.cpp \
	$(RADIANT_BASE)/plugins/shaders/plugin.cpp

RADIANT_PLUGIN_SHADERS_CPP_OBJS=$(RADIANT_PLUGIN_SHADERS_SRCS_CPP:%.cpp=$(BUILDDIR)/tools/radiant/plugins_cpp/%.o)
RADIANT_PLUGIN_SHADERS_TARGET=radiant/modules/shaders.$(SHARED_EXT)

#vfspk3 plugin
RADIANT_PLUGIN_VFSPK3_SRCS_CPP = \
	$(RADIANT_BASE)/plugins/vfspk3/vfspk3.cpp \
	$(RADIANT_BASE)/plugins/vfspk3/vfs.cpp \
	$(RADIANT_BASE)/plugins/vfspk3/archive.cpp

RADIANT_PLUGIN_VFSPK3_CPP_OBJS=$(RADIANT_PLUGIN_VFSPK3_SRCS_CPP:%.cpp=$(BUILDDIR)/tools/radiant/plugins_cpp/%.o)
RADIANT_PLUGIN_VFSPK3_TARGET=radiant/modules/vfspk3.$(SHARED_EXT)

#archivezip plugin
RADIANT_PLUGIN_ARCHIVEZIP_SRCS_CPP = \
	$(RADIANT_BASE)/plugins/archivezip/plugin.cpp \
	$(RADIANT_BASE)/plugins/archivezip/archive.cpp

RADIANT_PLUGIN_ARCHIVEZIP_CPP_OBJS=$(RADIANT_PLUGIN_ARCHIVEZIP_SRCS_CPP:%.cpp=$(BUILDDIR)/tools/radiant/plugins_cpp/%.o)
RADIANT_PLUGIN_ARCHIVEZIP_TARGET=radiant/modules/archivezip.$(SHARED_EXT)

# PLUGINS

RADIANT_PLUGIN_BRUSHEXPORT_SRCS_CPP = \
	$(RADIANT_BASE)/plugins/brushexport/callbacks.cpp \
	$(RADIANT_BASE)/plugins/brushexport/export.cpp \
	$(RADIANT_BASE)/plugins/brushexport/interface.cpp \
	$(RADIANT_BASE)/plugins/brushexport/plugin.cpp \
	$(RADIANT_BASE)/plugins/brushexport/support.cpp

RADIANT_PLUGIN_BRUSHEXPORT_CPP_OBJS=$(RADIANT_PLUGIN_BRUSHEXPORT_SRCS_CPP:%.cpp=$(BUILDDIR)/tools/radiant/plugins_cpp/%.o)
RADIANT_PLUGIN_BRUSHEXPORT_TARGET=radiant/plugins/brushexport.$(SHARED_EXT)

ifeq ($(BUILD_UFORADIANT),1)

ALL_RADIANT_OBJS+=$(RADIANT_CPP_OBJS) $(RADIANT_C_OBJS) $(RADIANT_PLUGIN_MODEL_C_OBJS) $(RADIANT_PLUGIN_MODEL_CPP_OBJS) \
	$(RADIANT_PLUGIN_IMAGE_CPP_OBJS) $(RADIANT_PLUGIN_MAP_CPP_OBJS) $(RADIANT_PLUGIN_ENTITY_CPP_OBJS) \
	$(RADIANT_PLUGIN_SHADERS_CPP_OBJS) $(RADIANT_PLUGIN_VFSPK3_CPP_OBJS) $(RADIANT_PLUGIN_ARCHIVEZIP_CPP_OBJS) \
	$(RADIANT_PLUGIN_BRUSHEXPORT_CPP_OBJS)

RADIANT_DEPS = $(patsubst %.o, %.d, $(ALL_RADIANT_OBJS))

uforadiant: $(BUILDDIR)/.dirs $(RADIANT_PLUGIN_MODEL_TARGET) $(RADIANT_PLUGIN_ENTITY_TARGET) $(RADIANT_PLUGIN_IMAGE_TARGET) \
	 $(RADIANT_PLUGIN_MAP_TARGET) $(RADIANT_PLUGIN_SHADERS_TARGET) $(RADIANT_PLUGIN_VFSPK3_TARGET) \
	$(RADIANT_PLUGIN_ARCHIVEZIP_TARGET) $(RADIANT_PLUGIN_BRUSHEXPORT_TARGET) \
	$(RADIANT_TARGET)
#$(RADIANT_PLUGIN_SOUND_TARGET)
else

uforadiant:
	@echo "Radiant is not enabled - use './configure --enable-uforadiant'"

clean-uforadiant:
	@echo "Radiant is not enabled - use './configure --enable-uforadiant'"

ALL_RADIANT_OBJS=""

endif

# Say how to build .o files from .cpp files for this module
$(BUILDDIR)/tools/radiant_c/%.o: $(SRCDIR)/%.c
	@echo " * [RAD] $<"; \
		$(CC) $(CFLAGS) $(RADIANT_CFLAGS) -o $@ -c $< $(CFLAGS_M_OPTS)
$(BUILDDIR)/tools/radiant/%.o: $(SRCDIR)/%.cpp
	@echo " * [RAD] $<"; \
		$(CPP) $(CPPFLAGS) $(RADIANT_CFLAGS) -o $@ -c $< $(CFLAGS_M_OPTS)

# Say how to build .o files from .cpp/.c files for this module
$(BUILDDIR)/tools/radiant/plugins_c/%.o: $(SRCDIR)/%.c
	@echo " * [RAD] $<"; \
		$(CC) $(CFLAGS) $(SHARED_CFLAGS) $(RADIANT_CFLAGS) -o $@ -c $< $(CFLAGS_M_OPTS)
$(BUILDDIR)/tools/radiant/plugins_cpp/%.o: $(SRCDIR)/%.cpp
	@echo " * [RAD] $<"; \
		$(CPP) $(CPPFLAGS) $(SHARED_CFLAGS) $(RADIANT_CFLAGS) -o $@ -c $< $(CFLAGS_M_OPTS)

# Say how about to build the modules
$(RADIANT_PLUGIN_MODEL_TARGET) : $(RADIANT_PLUGIN_MODEL_CPP_OBJS) $(RADIANT_PLUGIN_MODEL_C_OBJS)
	@echo " * [MDL] ... linking $(LNKFLAGS) ($(RADIANT_LIBS))"; \
		$(CPP) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(RADIANT_PLUGIN_MODEL_C_OBJS) $(RADIANT_PLUGIN_MODEL_CPP_OBJS) $(RADIANT_LIBS) $(LNKFLAGS)
$(RADIANT_PLUGIN_ENTITY_TARGET) : $(RADIANT_PLUGIN_ENTITY_CPP_OBJS)
	@echo " * [ENT] ... linking $(LNKFLAGS) ($(RADIANT_LIBS))"; \
		$(CPP) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(RADIANT_PLUGIN_ENTITY_CPP_OBJS) $(RADIANT_LIBS) $(LNKFLAGS)
$(RADIANT_PLUGIN_IMAGE_TARGET) : $(RADIANT_PLUGIN_IMAGE_CPP_OBJS)
	@echo " * [IMG] ... linking $(LNKFLAGS) ($(RADIANT_LIBS))"; \
		$(CPP) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(RADIANT_PLUGIN_IMAGE_CPP_OBJS) $(RADIANT_LIBS) $(LNKFLAGS)
$(RADIANT_PLUGIN_MAP_TARGET) : $(RADIANT_PLUGIN_MAP_CPP_OBJS)
	@echo " * [MAP] ... linking $(LNKFLAGS) ($(RADIANT_LIBS))"; \
		$(CPP) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(RADIANT_PLUGIN_MAP_CPP_OBJS) $(LNKFLAGS)
$(RADIANT_PLUGIN_SHADERS_TARGET) : $(RADIANT_PLUGIN_SHADERS_CPP_OBJS)
	@echo " * [SHD] ... linking $(LNKFLAGS) ($(RADIANT_LIBS))"; \
		$(CPP) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(RADIANT_PLUGIN_SHADERS_CPP_OBJS) $(RADIANT_LIBS) $(LNKFLAGS)
$(RADIANT_PLUGIN_VFSPK3_TARGET) : $(RADIANT_PLUGIN_VFSPK3_CPP_OBJS)
	@echo " * [VFS] ... linking $(LNKFLAGS) ($(RADIANT_LIBS))"; \
		$(CPP) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(RADIANT_PLUGIN_VFSPK3_CPP_OBJS) $(RADIANT_LIBS) $(LNKFLAGS)
$(RADIANT_PLUGIN_ARCHIVEZIP_TARGET) : $(RADIANT_PLUGIN_ARCHIVEZIP_CPP_OBJS)
	@echo " * [ZIP] ... linking $(LNKFLAGS) ($(RADIANT_LIBS))"; \
		$(CPP) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(RADIANT_PLUGIN_ARCHIVEZIP_CPP_OBJS) $(RADIANT_LIBS) $(LNKFLAGS) -lz

# and now link the plugins
$(RADIANT_PLUGIN_BRUSHEXPORT_TARGET) : $(RADIANT_PLUGIN_BRUSHEXPORT_CPP_OBJS)
	@echo " * [BRS] ... linking $(LNKFLAGS) ($(RADIANT_LIBS))"; \
		$(CPP) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(RADIANT_PLUGIN_BRUSHEXPORT_CPP_OBJS) $(RADIANT_LIBS) $(LNKFLAGS)

# Say how to link the exe
$(RADIANT_TARGET): $(RADIANT_CPP_OBJS) $(RADIANT_C_OBJS)
	@echo " * [RAD] ... linking $(LNKFLAGS) ($(RADIANT_LIBS))"; \
		$(CPP) $(LDFLAGS) -o $@ $(RADIANT_CPP_OBJS) $(RADIANT_C_OBJS) $(RADIANT_LIBS) $(LNKFLAGS)

