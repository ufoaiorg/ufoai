#include "SceneGraphModule.h"

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

SceneGraphAPI::SceneGraphAPI ()
{
	_scenegraph = new CompiledGraph();
}
SceneGraphAPI::~SceneGraphAPI ()
{
	delete _scenegraph;
}
scene::Graph* SceneGraphAPI::getTable ()
{
	return _scenegraph;
}

typedef SingletonModule<SceneGraphAPI> SceneGraphModule;
typedef Static<SceneGraphModule> StaticSceneGraphModule;
StaticRegisterModule staticRegisterSceneGraph(StaticSceneGraphModule::instance());

GraphTreeModel* scene_graph_get_tree_model ()
{
	CompiledGraph* sceneGraph = static_cast<CompiledGraph*>(
			StaticSceneGraphModule::instance().getTable()
	);
	return sceneGraph->getTreeModel();
}
