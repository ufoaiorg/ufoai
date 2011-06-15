#include "Primitives.h"

#include "../../brush/FaceInstance.h"
#include "../../brush/BrushVisit.h"
#include "string/string.h"
#include "iundo.h"

// greebo: Nasty global that contains all the selected face instances
extern FaceInstanceSet g_SelectedFaceInstances;

namespace selection {
	namespace algorithm {

int selectedFaceCount() {
	return static_cast<int>(g_SelectedFaceInstances.size());
}

class SelectedBrushFinder :
		public SelectionSystem::Visitor
{
	// The target list that gets populated
	BrushPtrVector& _vector;
public:
	SelectedBrushFinder(BrushPtrVector& targetVector) :
		_vector(targetVector)
	{}

	void visit(scene::Instance& instance) const {
		BrushInstance* brushInstance = Instance_getBrush(instance);
		if (brushInstance != NULL) {
			_vector.push_back(&brushInstance->getBrush());
		}
	}
};

BrushPtrVector getSelectedBrushes() {
	BrushPtrVector returnVector;

	GlobalSelectionSystem().foreachSelected(
		SelectedBrushFinder(returnVector)
	);

	return returnVector;
}

Face& getLastSelectedFace() {
	if (selectedFaceCount() == 1) {
		return g_SelectedFaceInstances.last().getFace();
	}
	else {
		throw selection::InvalidSelectionException(string::toString(selectedFaceCount()));
	}
}

class FaceVectorPopulator
{
private:
	// The target list that gets populated
	FacePtrVector& _vector;
public:
	FaceVectorPopulator(FacePtrVector& targetVector) :
		_vector(targetVector)
	{}

	void operator() (FaceInstance& faceInstance) {
		_vector.push_back(faceInstance.getFacePtr());
	}
};

FacePtrVector getSelectedFaces() {
	FacePtrVector vector;

	// Cycle through all selected faces and fill the vector
	g_SelectedFaceInstances.foreach(FaceVectorPopulator(vector));

	return vector;
}

class FaceSetDetail
{
		bool m_detail;
	public:
		FaceSetDetail (bool detail) :
			m_detail(detail)
		{
		}
		void operator() (Face& face) const
		{
			face.setDetail(m_detail);
		}
};

inline void Scene_BrushSetDetail_Selected (scene::Graph& graph, bool detail)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceSetDetail(detail));
	SceneChangeNotify();
}

void makeDetail ()
{
	UndoableCommand undo("brushSetDetail");
	Scene_BrushSetDetail_Selected(GlobalSceneGraph(), true);
}

void makeStructural ()
{
	UndoableCommand undo("brushClearDetail");
	Scene_BrushSetDetail_Selected(GlobalSceneGraph(), false);
}

	} // namespace algorithm
} // namespace selection
