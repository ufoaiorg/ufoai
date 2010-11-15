#include "SceneGraphModule.h"
#include "treemodel.h"

namespace
{
	CompiledGraph* g_sceneGraph;
	GraphTreeModel* g_tree_model;
}

GraphTreeModel* scene_graph_get_tree_model ()
{
	return g_tree_model;
}

class SceneGraphObserver: public scene::Instantiable::Observer
{
	public:
		void insert (scene::Instance* instance)
		{
			g_sceneGraph->sceneChanged();
			graph_tree_model_insert(g_tree_model, *instance);
		}
		void erase (scene::Instance* instance)
		{
			g_sceneGraph->notifyErase(instance);
			g_sceneGraph->sceneChanged();
			graph_tree_model_erase(g_tree_model, *instance);
		}
};

SceneGraphObserver g_SceneGraphObserver;

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

SceneGraphAPI::SceneGraphAPI ()
{
	g_tree_model = graph_tree_model_new();

	g_sceneGraph = new CompiledGraph(&g_SceneGraphObserver);

	m_scenegraph = g_sceneGraph;
}
SceneGraphAPI::~SceneGraphAPI ()
{
	delete g_sceneGraph;

	graph_tree_model_delete(g_tree_model);
}
scene::Graph* SceneGraphAPI::getTable ()
{
	return m_scenegraph;
}

typedef SingletonModule<SceneGraphAPI> SceneGraphModule;
typedef Static<SceneGraphModule> StaticSceneGraphModule;
StaticRegisterModule staticRegisterSceneGraph(StaticSceneGraphModule::instance());
