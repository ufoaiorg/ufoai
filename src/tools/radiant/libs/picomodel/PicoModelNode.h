#include <string>

#include "model.h"

#include "picomodel.h"
#include "RenderablePicoModel.h"

#include "iarchive.h"
#include "idatastream.h"
#include "imodel.h"

#include "cullable.h"
#include "renderable.h"
#include "selectable.h"

#include "math/frustum.h"
#include "string/string.h"
#include "generic/static.h"
#include "entitylib.h"
#include "shaderlib.h"
#include "scenelib.h"
#include "instancelib.h"
#include "transformlib.h"
#include "traverselib.h"
#include "render.h"
#include "os/path.h"
#include "RenderablePicoSurface.h"
#include "RenderablePicoModel.h"
#include "SkinnedModel.h"

class PicoModelInstance: public scene::Instance, public Renderable, public SelectionTestable, public SkinnedModel
{
		class TypeCasts
		{
				InstanceTypeCastTable m_casts;
			public:
				TypeCasts ()
				{
					InstanceContainedCast<PicoModelInstance, Bounded>::install(m_casts);
					InstanceContainedCast<PicoModelInstance, Cullable>::install(m_casts);
					InstanceStaticCast<PicoModelInstance, Renderable>::install(m_casts);
					InstanceStaticCast<PicoModelInstance, SelectionTestable>::install(m_casts);
					InstanceStaticCast<PicoModelInstance, SkinnedModel>::install(m_casts);
				}
				InstanceTypeCastTable& get ()
				{
					return m_casts;
				}
		};

		model::RenderablePicoModel& m_picomodel;

		class Remap
		{
			public:
				std::string first;
				Shader* second;
				Remap () :
					second(0)
				{
				}
		};
		typedef Array<Remap> SurfaceRemaps;
		SurfaceRemaps m_skins;

		std::string _skin;

		PicoModelInstance (const PicoModelInstance&);
		PicoModelInstance operator= (const PicoModelInstance&);
	public:
		typedef LazyStatic<TypeCasts> StaticTypeCasts;

		Bounded& get (NullType<Bounded>);
		Cullable& get (NullType<Cullable>);

		PicoModelInstance (const scene::Path& path, scene::Instance* parent, model::RenderablePicoModel& picomodel);
		~PicoModelInstance ();

		void render (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const;

		// greebo: Updates the model's surface remaps. Pass the new skin name (can be empty).
		void skinChanged (const std::string& newSkinName);
		// Returns the name of the currently active skin
		std::string getSkin () const;

		void renderSolid (Renderer& renderer, const VolumeTest& volume) const;
		void renderWireframe (Renderer& renderer, const VolumeTest& volume) const;

		void testSelect (Selector& selector, SelectionTest& test);
};

class PicoModelNode: public scene::Node, public scene::Instantiable
{
		class TypeCasts
		{
				NodeTypeCastTable m_casts;
			public:
				TypeCasts ()
				{
				}
				NodeTypeCastTable& get ()
				{
					return m_casts;
				}
		};

		InstanceSet m_instances;
		model::RenderablePicoModel m_picomodel;

	public:
		typedef LazyStatic<TypeCasts> StaticTypeCasts;

		/** Construct a PicoModelNode with the parsed picoModel_t struct and the
		 * provided file extension.
		 */
		PicoModelNode (picoModel_t* model, const std::string& ext);

		scene::Node& node ();

		void skinChanged (const std::string& newSkinName);
		scene::Instance* create (const scene::Path& path, scene::Instance* parent);
		void forEachInstance (const scene::Instantiable::Visitor& visitor);
		void insert (scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance);
		scene::Instance* erase (scene::Instantiable::Observer* observer, const scene::Path& path);
};
