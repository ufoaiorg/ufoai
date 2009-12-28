#ifndef RENDERABLEPICOMODEL_H_
#define RENDERABLEPICOMODEL_H_

#include <string>
#include <vector>
#include "RenderablePicoSurface.h"
#include "imodel.h"
#include "picomodel.h"
#include "math/aabb.h"
#include "igl.h"

namespace model
{
	/* Renderable class containing a model loaded via the picomodel library. A
	 * RenderablePicoModel is made up of one or more RenderablePicoSurface objects,
	 * each of which contains a number of polygons with the same texture. Rendering
	 * a RenderablePicoModel involves rendering all of its surfaces, each of which
	 * binds its texture(s) and submits its geometry via OpenGL calls.
	 */
	class RenderablePicoModel: public IModel
	{
			// Vector of renderable surfaces for this model
			typedef std::vector<RenderablePicoSurface> SurfaceList;
			SurfaceList _surfVec;

			// Local AABB for this model
			AABB _localAABB;

			// The list of skins that are available for this model
			ModelSkinList modelSkinList;

			std::string polyCountStr;
			std::string surfaceCountStr;
			std::string vertexCountStr;

			/** Return the number of surfaces in this model.
			 */
			int getSurfaceCountInt () const
			{
				return _surfVec.size();
			}

			/** Return the number of vertices in this model, by summing the vertex
			 * counts for each surface
			 */
			int getVertexCountInt () const
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
			int getPolyCountInt () const
			{
				int sum = 0;
				for (SurfaceList::const_iterator i = _surfVec.begin(); i != _surfVec.end(); ++i) {
					sum += i->getPolyCount();
				}
				return sum;
			}

		public:

			/** Constructor. Accepts a picoModel_t struct containing the raw model data
			 * loaded from picomodel
			 */
			RenderablePicoModel (picoModel_t* mod);

			/** Virtual render function from OpenGLRenderable.
			 */
			void render (RenderStateFlags flags) const;

			/** Return the enclosing AABB for this model.
			 */
			const AABB& localAABB () const
			{
				return _localAABB;
			}

			/** Return the number of surfaces in this model.
			 */
			const std::string& getSurfaceCount () const
			{
				return surfaceCountStr;
			}

			/** Return the number of vertices in this model, by summing the vertex
			 * counts for each surface
			 */
			const std::string& getVertexCount () const
			{
				return vertexCountStr;
			}

			/** Return the polycount (tricount) of this model by summing the surface
			 * polycounts.
			 */
			const std::string& getPolyCount () const
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
	};
}

#endif /*RENDERABLEPICOMODEL_H_*/
