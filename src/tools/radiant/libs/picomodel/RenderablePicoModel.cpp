#include "RenderablePicoModel.h"

#include "texturelib.h"
#include "ifilter.h"

#include "string/string.h"
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
		const int polyCount = getPolyCount();
		polyCountStream << polyCount;
		polyCountStr = polyCountStream.str();

		std::stringstream surfaceCountStream;
		const int surfaceCount = getSurfaceCount();
		surfaceCountStream << surfaceCount;
		surfaceCountStr = surfaceCountStream.str();

		std::stringstream vertexCountStream;
		const int vertexCount = getVertexCount();
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
			// Get the IShader to test the shader name against the filter system
			GLTexture& tex = i->getShader()->getTexture();
			if (GlobalFilterSystem().isVisible(FilterRule::TYPE_TEXTURE, tex.getName())) {
				// Bind the OpenGL texture and render the surface geometry
				glBindTexture(GL_TEXTURE_2D, tex.texture_number);
				i->render(flags);
			}
		}
	}

	// Apply the given skin to this model
	void RenderablePicoModel::applySkin (const std::string& skin)
	{
		std::string name = "";

		for (ModelSkinList::const_iterator i = modelSkinList.begin(); i != modelSkinList.end(); ++i) {
			if (*i == skin) {
				name = *i;
				break;
			}
		}

		if (name.empty()) {
			int skinId = string::toInt(skin);
			if (skinId < 0 || skinId >= modelSkinList.size())
				return;
			name = modelSkinList[skinId];
		}

		// Apply the skin to each surface
		for (SurfaceList::iterator i = _surfVec.begin(); i != _surfVec.end(); ++i) {
			i->applySkin(name);
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

	void RenderablePicoModel::render (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld,
			std::vector<Shader*> states) const
	{
		for (SurfaceList::const_iterator i = _surfVec.begin(); i != _surfVec.end(); ++i) {
			if (i->intersectVolume(volume, localToWorld) != VOLUME_OUTSIDE) {
				i->render(renderer, localToWorld, states[i - _surfVec.begin()]);
			}
		}
	}

	// Perform selection test
	void RenderablePicoModel::testSelect (Selector& selector, SelectionTest& test, const Matrix4& localToWorld)
	{
		// Perform a volume intersection (AABB) check on each surface. For those
		// that intersect, call the surface's own testSelection method to perform
		// a proper selection test.
		for (SurfaceList::iterator i = _surfVec.begin(); i != _surfVec.end(); ++i) {
			// Check volume intersection
			if (test.getVolume().TestAABB(i->localAABB(), localToWorld) != VOLUME_OUTSIDE) {
				// Volume intersection passed, delegate the selection test
				i->testSelect(selector, test, localToWorld);
			}
		}
	}

	VolumeIntersectionValue RenderablePicoModel::intersectVolume (const VolumeTest& test, const Matrix4& localToWorld) const
	{
		return test.TestAABB(_localAABB, localToWorld);
	}
}
