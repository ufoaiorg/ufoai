#ifndef BRUSHGETCLOSESTFACEVISIBLEWALKER_H_
#define BRUSHGETCLOSESTFACEVISIBLEWALKER_H_

#include "../../brush/BrushInstance.h"

class OccludeSelector: public Selector
{
		SelectionIntersection& m_bestIntersection;
		bool& m_occluded;
	public:
		OccludeSelector (SelectionIntersection& bestIntersection, bool& occluded) :
			m_bestIntersection(bestIntersection), m_occluded(occluded)
		{
			m_occluded = false;
		}
		void pushSelectable (Selectable& selectable)
		{
		}
		void popSelectable (void)
		{
		}
		void addIntersection (const SelectionIntersection& intersection)
		{
			if (SelectionIntersection_closer(intersection, m_bestIntersection)) {
				m_bestIntersection = intersection;
				m_occluded = true;
			}
		}
};

class BrushGetClosestFaceVisibleWalker: public scene::Graph::Walker
{
		SelectionTest& m_test;
		Texturable& m_texturable;
		mutable SelectionIntersection m_bestIntersection;

		static void Face_getTexture (Face& face, std::string& shader, TextureProjection& projection,
				ContentsFlagsValue& flags)
		{
			shader = face.GetShader();
			face.GetTexdef(projection);
			flags = face.getShader().m_flags;
		}
		typedef Function4<Face&, std::string&, TextureProjection&, ContentsFlagsValue&, void, Face_getTexture>
				FaceGetTexture;

		static void Face_setTexture (Face& face, const std::string& shader, const TextureProjection& projection,
				const ContentsFlagsValue& flags)
		{
			face.SetShader(shader);
			face.SetTexdef(projection);
			face.SetFlags(flags);
		}
		typedef Function4<Face&, const std::string&, const TextureProjection&, const ContentsFlagsValue&, void,
				Face_setTexture> FaceSetTexture;

		static void Face_getClosest (Face& face, SelectionTest& test, SelectionIntersection& bestIntersection,
				Texturable& texturable)
		{
			SelectionIntersection intersection;
			face.testSelect(test, intersection);
			if (intersection.valid() && SelectionIntersection_closer(intersection, bestIntersection)) {
				bestIntersection = intersection;
				texturable.setTexture = makeCallback3(FaceSetTexture(), face);
				texturable.getTexture = makeCallback3(FaceGetTexture(), face);
			}
		}

	public:
		BrushGetClosestFaceVisibleWalker (SelectionTest& test, Texturable& texturable) :
			m_test(test), m_texturable(texturable)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (path.top().get().visible()) {
				BrushInstance* brush = Instance_getBrush(instance);
				if (brush != 0) {
					m_test.BeginMesh(brush->localToWorld());

					for (Brush::const_iterator i = brush->getBrush().begin(); i != brush->getBrush().end(); ++i) {
						Face_getClosest(*(*i), m_test, m_bestIntersection, m_texturable);
					}
				} else {
					SelectionTestable* selectionTestable = Instance_getSelectionTestable(instance);
					if (selectionTestable) {
						bool occluded;
						OccludeSelector selector(m_bestIntersection, occluded);
						selectionTestable->testSelect(selector, m_test);
						if (occluded)
							m_texturable = Texturable();
					}
				}
			}
			return true;
		}
};

#endif
