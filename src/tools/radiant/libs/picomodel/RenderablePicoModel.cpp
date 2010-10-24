#include "RenderablePicoModel.h"

#include "texturelib.h"

#include <sstream>

namespace model
{
	// Constructor

	RenderablePicoModel::RenderablePicoModel (picoModel_t* mod)
	{
		// Get the number of surfaces to create
		const int nSurf = PicoGetModelNumSurfaces(mod);

		// Create a RenderablePicoSurface for each surface in the structure
		for (int n = 0; n < nSurf; ++n) {
			// Retrieve the surface, discarding it if it is null or non-triangulated (?)
			picoSurface_t* surf = PicoGetModelSurface(mod, n);
			if (surf == 0 || PicoGetSurfaceType(surf) != PICO_TRIANGLES)
				continue;

			// Fix the normals of the surface (?)
			PicoFixSurfaceNormals(surf);

			// Create the RenderablePicoSurface object and add it to the vector
			RenderablePicoSurface rSurf = RenderablePicoSurface(surf);
			_surfVec.push_back(rSurf);

			// Extend the model AABB to include the surface's AABB
			aabb_extend_by_aabb(_localAABB, rSurf.localAABB());
		}

		const int nShaders = PicoGetModelNumShaders(mod);
		for (int n = 0; n < nShaders; n++) {
			const picoShader_t *shader = PicoGetModelShader(mod, n);
			if (shader) {
				modelSkinList.push_back(shader->name);
			}
		}

		std::stringstream polyCountStream;
		const int polyCount = getPolyCountInt();
		polyCountStream << polyCount;
		polyCountStr = polyCountStream.str();

		std::stringstream surfaceCountStream;
		const int surfaceCount = getSurfaceCountInt();
		surfaceCountStream << surfaceCount;
		surfaceCountStr = surfaceCountStream.str();

		std::stringstream vertexCountStream;
		const int vertexCount = getVertexCountInt();
		vertexCountStream << vertexCount;
		vertexCountStr = vertexCountStream.str();
	}

	// Virtual render function

	void RenderablePicoModel::render (RenderStateFlags flags) const
	{
		glEnable(GL_VERTEX_ARRAY);
		glEnable(GL_NORMAL_ARRAY);
		glEnable(GL_TEXTURE_COORD_ARRAY);
		// Render options
		if (flags & RENDER_TEXTURE_2D)
			glEnable(GL_TEXTURE_2D);
		if (flags & RENDER_SMOOTH)
			glShadeModel(GL_SMOOTH);

		// Iterate over the surfaces, calling the render function on each one
		for (SurfaceList::const_iterator i = _surfVec.begin(); i != _surfVec.end(); ++i) {
			qtexture_t& tex = i->getShader()->getTexture();
			glBindTexture(GL_TEXTURE_2D, tex.texture_number);
			i->render(flags);
		}
	}

	// Apply the given skin to this model
	void RenderablePicoModel::applySkin (const std::string& skin)
	{
		// Apply the skin to each surface
		for (SurfaceList::iterator i = _surfVec.begin(); i != _surfVec.end(); ++i) {
			i->applySkin(skin);
		}
	}

	/**
	 * @brief Return the skins associated with the given model.
	 *
	 * @param
	 * The full pathname of the model, as given by the "model" key in the skin definition.
	 *
	 * @return
	 * A vector of strings, each identifying the name of a skin which is associated with the
	 * given model. The vector may be empty as a model does not require any associated skins.
	 */
	const ModelSkinList& RenderablePicoModel::getSkinsForModel () const
	{
		return modelSkinList;
	}
}
