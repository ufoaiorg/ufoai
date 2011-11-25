#include "RenderablePicoSurface.h"
#include "renderable.h"

namespace model
{
	// Constructor. Copy the provided picoSurface_t structure into this object
	RenderablePicoSurface::RenderablePicoSurface (picoSurface_t* surf) :
		_originalShaderName(""), _mappedShaderName("")
	{
		// Get the shader from the picomodel struct.
		picoShader_t* shader = PicoGetSurfaceShader(surf);
		if (shader != 0) {
			_originalShaderName = PicoGetShaderName(shader);
		}

		// Capture the shader
		_shader = GlobalShaderCache().capture(_originalShaderName);
		if (_shader)
			_mappedShaderName = _originalShaderName; // no skin at this time

		// Get the number of vertices and indices, and reserve capacity in our vectors in advance
		// by populating them with empty structs.
		const int nVerts = PicoGetSurfaceNumVertexes(surf);
		_nIndices = PicoGetSurfaceNumIndexes(surf);
		_vertices.resize(nVerts);
		_indices.resize(_nIndices);

		// Stream in the vertex data from the raw struct, expanding the local AABB to include
		// each vertex.
		for (int vNum = 0; vNum < nVerts; ++vNum) {
			Vertex3f vertex(PicoGetSurfaceXYZ(surf, vNum));
			_localAABB.includePoint(vertex);
			_vertices[vNum].vertex = vertex;
			_vertices[vNum].normal = Normal3f(PicoGetSurfaceNormal(surf, vNum));
			_vertices[vNum].texcoord = TexCoord2f(PicoGetSurfaceST(surf, 0, vNum));
		}

		// Stream in the index data
		picoIndex_t* ind = PicoGetSurfaceIndexes(surf, 0);
		for (unsigned int i = 0; i < _nIndices; i++)
			_indices[i] = ind[i];
	}

	//copy constructor, won't release shader
	RenderablePicoSurface::RenderablePicoSurface (RenderablePicoSurface const& other) :
		OpenGLRenderable(other), Cullable(other), _originalShaderName(other._originalShaderName), _mappedShaderName(
				other._mappedShaderName), _vertices(other._vertices), _indices(other._indices), _localAABB(
				other._localAABB)
	{
		_nIndices = other._nIndices;
		_shader = GlobalShaderCache().capture(_mappedShaderName);
	}

	RenderablePicoSurface::~RenderablePicoSurface ()
	{
		GlobalShaderCache().release(_mappedShaderName);
	}

	// Render function
	void RenderablePicoSurface::render (RenderStateFlags flags) const
	{
		// Use Vertex Arrays to submit data to the GL. We will assume that it is
		// acceptable to perform pointer arithmetic over the elements of a std::vector,
		// starting from the address of element 0.
		glNormalPointer(GL_FLOAT, sizeof(ArbitraryMeshVertex), &_vertices[0].normal);
		glVertexPointer(3, GL_FLOAT, sizeof(ArbitraryMeshVertex), &_vertices[0].vertex);
		glTexCoordPointer(2, GL_FLOAT, sizeof(ArbitraryMeshVertex), &_vertices[0].texcoord);
		glDrawElements(GL_TRIANGLES, _nIndices, GL_UNSIGNED_INT, &_indices[0]);
	}

	void RenderablePicoSurface::render (Renderer& renderer, const Matrix4& localToWorld, Shader* state) const
	{
		ASSERT_NOTNULL(state);
		renderer.SetState(state, Renderer::eFullMaterials);
		renderer.addRenderable(*this, localToWorld);
	}

	void RenderablePicoSurface::render (Renderer& renderer, const Matrix4& localToWorld) const
	{
		render(renderer, localToWorld, _shader);
	}

	// Apply a skin to this surface
	void RenderablePicoSurface::applySkin (const std::string& remap)
	{
		if (!remap.empty() && remap != _mappedShaderName) { // change to a new shader
			// Switch shader objects
			GlobalShaderCache().release(_mappedShaderName);
			_shader = GlobalShaderCache().capture(remap);

			// Save the remapped shader name
			_mappedShaderName = remap;
		} else if (remap.empty() && _mappedShaderName != _originalShaderName) {
			// No remap, so reset our shader to the original unskinned shader
			GlobalShaderCache().release(_mappedShaderName);
			_shader = GlobalShaderCache().capture(_originalShaderName);

			// Reset the remapped shader name
			_mappedShaderName = _originalShaderName;
		}
	}
} // namespace model
