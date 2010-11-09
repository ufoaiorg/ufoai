#ifndef RENDERABLEPICOMODEL_H_
#define RENDERABLEPICOMODEL_H_

#include <string>
#include <vector>
#include "RenderablePicoSurface.h"
#include "imodel.h"
#include "picomodel.h"
#include "cullable.h"
#include "math/aabb.h"
#include "igl.h"

namespace model
{
	typedef std::vector<RenderablePicoSurface> SurfaceList;

	/* Renderable class containing a model loaded via the picomodel library. A
	 * RenderablePicoModel is made up of one or more RenderablePicoSurface objects,
	 * each of which contains a number of polygons with the same texture. Rendering
	 * a RenderablePicoModel involves rendering all of its surfaces, each of which
	 * binds its texture(s) and submits its geometry via OpenGL calls.
	 */
	class RenderablePicoModel: public IModel, public Cullable
	{
			// Vector of renderable surfaces for this model
			SurfaceList _surfVec;

			// Local AABB for this model
			AABB _localAABB;

			// The list of skins that are available for this model
			ModelSkinList modelSkinList;

			std::string polyCountStr;
			std::string surfaceCountStr;
			std::string vertexCountStr;

		public:

			/** Constructor. Accepts a picoModel_t struct containing the raw model data
			 * loaded from picomodel
			 */
			RenderablePicoModel (picoModel_t* mod);

			/** Virtual render function from OpenGLRenderable.
			 */
			void render (RenderStateFlags flags) const;

			void render (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld,
					std::vector<Shader*> states) const;

			/** Return the number of surfaces in this model.
			 */
			int getSurfaceCount() const {
				return static_cast<int>(_surfVec.size());
			}

			/** Return the number of vertices in this model, by summing the vertex
			 * counts for each surface
			 */
			int getVertexCount () const
			{
				int sum = 0;
				for (SurfaceList::const_iterator i = _surfVec.begin(); i != _surfVec.end(); ++i) {
					sum += i->getVertexCount();
				}
				return sum;
			}

			/** Return the polycount (tricount) of this model by summing the surface
			 * polycounts.
			 */
			int getPolyCount () const
			{
				int sum = 0;
				for (SurfaceList::const_iterator i = _surfVec.begin(); i != _surfVec.end(); ++i) {
					sum += i->getPolyCount();
				}
				return sum;
			}

			/** Return the enclosing AABB for this model.
			 */
			const AABB& localAABB () const
			{
				return _localAABB;
			}

			/** Return the number of surfaces in this model.
			 */
			const std::string& getSurfaceCountStr () const
			{
				return surfaceCountStr;
			}

			/** Return the number of vertices in this model, by summing the vertex
			 * counts for each surface
			 */
			const std::string& getVertexCountStr () const
			{
				return vertexCountStr;
			}

			/** Return the polycount (tricount) of this model by summing the surface
			 * polycounts.
			 */
			const std::string& getPolyCountStr () const
			{
				return polyCountStr;
			}

			/**
			 * @brief Return the skins associated with the given model.
			 *
			 * @return
			 * A vector of strings, each identifying the name of a skin which is associated with the
			 * given model. The vector may be empty as a model does not require any associated skins.
			 */
			const ModelSkinList& getSkinsForModel () const;

			/** Apply the given skin to this model.
			 */
			void applySkin (const std::string& skin);

			// Cullable interface
			VolumeIntersectionValue intersectVolume (const VolumeTest& test, const Matrix4& localToWorld) const;

			/**
			 * Selection test. Test each surface against the SelectionTest object and
			 * if the surface is selected, add it to the selector.
			 *
			 * @param selector
			 * Selector object which builds a list of selectables.
			 *
			 * @param test
			 * The SelectionTest object defining the 3D properties of the selection.
			 *
			 * @param localToWorld
			 * Object to world space transform.
			 */
			void testSelect(Selector& selector,
							SelectionTest& test,
							const Matrix4& localToWorld);

			/**
			 * Return the list of RenderablePicoSurface objects.
			 */
			const SurfaceList& getSurfaces() const {
				return _surfVec;
			}
	};
}

#endif /*RENDERABLEPICOMODEL_H_*/
