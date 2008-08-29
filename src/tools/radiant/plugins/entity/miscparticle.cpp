/*
Copyright (C) 2001-2006, William Joseph.
All Rights Reserved.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

///\file
///\brief Represents the misc_particle entity.
///
/// This entity displays the particle specified in its "particle" key.
/// The "origin" key directly controls the entity's local-to-parent transform.

#include "autoptr.h"
#include "cullable.h"
#include "renderable.h"
#include "editable.h"

#include "math/frustum.h"
#include "selectionlib.h"
#include "instancelib.h"
#include "transformlib.h"
#include "entitylib.h"
#include "render.h"
#include "eclasslib.h"
#include "math/line.h"

#include "ifilesystem.h"
#include <glib/gslist.h>

#include "targetable.h"
#include "origin.h"
#include "filters.h"
#include "namedentity.h"
#include "keyobservers.h"
#include "namekeys.h"

#include "model.h"
#include "archivelib.h"

#include "entity.h"

// the list of ufos/*.ufo files we need to work with
GSList *l_ufofiles = 0;

GSList* getUFOFileList() {
	return l_ufofiles;
}

std::list<CopiedString> g_ufoFilenames;


class ParticleDefinition {
public:
	ParticleDefinition(const char* id, const char *model, const char *image)
		: m_id(id), m_model(model), m_image(image) {
		globalOutputStream() << "particle " << id << "with: ";
		if (model)
			globalOutputStream() << "model: " << model << " ";
		if (image)
			globalOutputStream() << "image: " << image << " ";
		globalOutputStream() << "\n";
	}
	const char *m_id;
	const char *m_model;
	const char *m_image;
};

typedef std::map<CopiedString, ParticleDefinition> ParticleDefinitionMap;

ParticleDefinitionMap g_particleDefinitions;


void ParseUFOFile(Tokeniser& tokeniser, const char* filename) {
	g_ufoFilenames.push_back(filename);
	filename = g_ufoFilenames.back().c_str();
	tokeniser.nextLine();
	for (;;) {
		const char* token = tokeniser.getToken();

		if (token == 0) {
			break;
		}

		if (string_equal(token, "particle")) {
			char pID[64];
			token = tokeniser.getToken();
			if (token == 0) {
				Tokeniser_unexpectedError(tokeniser, 0, "#id-name");
				return;
			}

			strncpy(pID, token, sizeof(pID) - 1);
			pID[sizeof(pID) - 1] = '\0';

			if (!Tokeniser_parseToken(tokeniser, "{")) {
				globalOutputStream() << "ERROR: expected {.\n";
				return;
			}

			const char *model = NULL;
			const char *image = NULL;
			for (;;) {
				token = tokeniser.getToken();
				if (string_equal(token, "init")) {
					if (!Tokeniser_parseToken(tokeniser, "{")) {
						globalOutputStream() << "ERROR: expected {.\n";
						return;
					}
					for (;;) {
						const char* option = tokeniser.getToken();
						if (string_equal(option, "}")) {
							break;
						} else if (string_equal(option, "image")) {
							image = tokeniser.getToken();
							token = "}";
							break;
						} else if (string_equal(option, "model")) {
							model = tokeniser.getToken();
							token = "}";
							break;
						}
					}
				}
				if (string_equal(token, "}"))
					break;
			}
			if (model || image) {
				// do we already have this particle?
				if (!g_particleDefinitions.insert(ParticleDefinitionMap::value_type(pID, ParticleDefinition(pID, model, image))).second)
					globalOutputStream() << "WARNING: particle " << pID << " is already in memory, definition in " << filename << " ignored.\n";
				else
					globalOutputStream() << "adding particle " << pID << "\n";
			}
		}
	}
}

void LoadUFOFile(const char* filename) {
	AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(filename));
	if (file) {
		globalOutputStream() << "Parsing ufo script file " << filename << "\n";

		AutoPtr<Tokeniser> tokeniser(GlobalScriptLibrary().m_pfnNewScriptTokeniser(file->getInputStream()));
		ParseUFOFile(*tokeniser, filename);
	} else {
		globalOutputStream() << "Unable to read ufo script file " << filename << "\n";
	}
}

void addUFOFile(const char* dirstring) {
	bool found = false;

	for (GSList* tmp = l_ufofiles; tmp != 0; tmp = tmp->next) {
		if (string_equal_nocase(dirstring, (char*)tmp->data)) {
			found = true;
			break;
		}
	}

	if (!found) {
		l_ufofiles = g_slist_append(l_ufofiles, strdup(dirstring));
		StringOutputStream ufoname(256);
		ufoname << "ufos/" << dirstring;
		LoadUFOFile(ufoname.c_str());
		ufoname.clear();
	}
}

typedef FreeCaller1<const char*, addUFOFile> AddUFOFileCaller;

void MiscParticle_construct() {
	GlobalFileSystem().forEachFile("ufos/", "ufo", AddUFOFileCaller(), 0);
}

void MiscParticle_destroy() {
	globalOutputStream() << "Unload UFO script files\n";
	while (l_ufofiles != 0) {
		free(l_ufofiles->data);
		l_ufofiles = g_slist_remove(l_ufofiles, l_ufofiles->data);
	}
	g_ufoFilenames.clear();
	g_particleDefinitions.clear();
}

// Todo get the particle image/model and render it
class RenderableParticle : public OpenGLRenderable {
	const char* m_particleID;

public:
	RenderableParticle(const char *id) : m_particleID(id) {

	}

	void render(RenderStateFlags state) const {
	}
};

inline void read_aabb(AABB& aabb, const EntityClass& eclass) {
	aabb = aabb_for_minmax(eclass.mins, eclass.maxs);
}


class MiscParticle :
			public Cullable,
			public Bounded,
			public Snappable {
	EntityKeyValues m_entity;
	KeyObserverMap m_keyObservers;
	MatrixTransform m_transform;

	OriginKey m_originKey;
	Vector3 m_origin;

	ClassnameFilter m_filter;
	NamedEntity m_named;
	NameKeys m_nameKeys;

	AABB m_aabb_local;
	char *m_particleID;

	RenderableParticle m_particle;
	RenderableSolidAABB m_aabb_solid;
	RenderableWireframeAABB m_aabb_wire;
	RenderableNamedEntity m_renderName;

	Callback m_transformChanged;
	Callback m_evaluateTransform;

	void construct() {
		read_aabb(m_aabb_local, m_entity.getEntityClass());

		m_keyObservers.insert("classname", ClassnameFilter::ClassnameChangedCaller(m_filter));
		m_keyObservers.insert(Static<KeyIsName>::instance().m_nameKey, NamedEntity::IdentifierChangedCaller(m_named));
		m_keyObservers.insert("origin", OriginKey::OriginChangedCaller(m_originKey));
		// todo: particle watchdog
	}

	void updateTransform() {
		m_transform.localToParent() = g_matrix4_identity;
		matrix4_translate_by_vec3(m_transform.localToParent(), m_origin);
		m_transformChanged();
	}
	typedef MemberCaller<MiscParticle, &MiscParticle::updateTransform> UpdateTransformCaller;
	void originChanged() {
		m_origin = m_originKey.m_origin;
		updateTransform();
	}
	typedef MemberCaller<MiscParticle, &MiscParticle::originChanged> OriginChangedCaller;
public:

	MiscParticle(EntityClass* eclass, scene::Node& node, const Callback& transformChanged, const Callback& evaluateTransform) :
			m_entity(eclass),
			m_originKey(OriginChangedCaller(*this)),
			m_origin(ORIGINKEY_IDENTITY),
			m_filter(m_entity, node),
			m_named(m_entity),
			m_nameKeys(m_entity),
			m_particle(m_particleID),
			m_aabb_solid(m_aabb_local),
			m_aabb_wire(m_aabb_local),
			m_renderName(m_named, g_vector3_identity),
			m_transformChanged(transformChanged),
			m_evaluateTransform(evaluateTransform) {
		construct();
	}
	MiscParticle(const MiscParticle& other, scene::Node& node, const Callback& transformChanged, const Callback& evaluateTransform) :
			m_entity(other.m_entity),
			m_originKey(OriginChangedCaller(*this)),
			m_origin(ORIGINKEY_IDENTITY),
			m_filter(m_entity, node),
			m_named(m_entity),
			m_nameKeys(m_entity),
			m_particle(m_particleID),
			m_aabb_solid(m_aabb_local),
			m_aabb_wire(m_aabb_local),
			m_renderName(m_named, g_vector3_identity),
			m_transformChanged(transformChanged),
			m_evaluateTransform(evaluateTransform) {
		construct();
	}

	InstanceCounter m_instanceCounter;
	void instanceAttach(const scene::Path& path) {
		if (++m_instanceCounter.m_count == 1) {
			m_filter.instanceAttach();
			m_entity.instanceAttach(path_find_mapfile(path.begin(), path.end()));
			m_entity.attach(m_keyObservers);
		}
	}
	void instanceDetach(const scene::Path& path) {
		if (--m_instanceCounter.m_count == 0) {
			m_entity.detach(m_keyObservers);
			m_entity.instanceDetach(path_find_mapfile(path.begin(), path.end()));
			m_filter.instanceDetach();
		}
	}

	EntityKeyValues& getEntity() {
		return m_entity;
	}
	const EntityKeyValues& getEntity() const {
		return m_entity;
	}

	Namespaced& getNamespaced() {
		return m_nameKeys;
	}
	Nameable& getNameable() {
		return m_named;
	}
	TransformNode& getTransformNode() {
		return m_transform;
	}

	const AABB& localAABB() const {
		return m_aabb_local;
	}

	VolumeIntersectionValue intersectVolume(const VolumeTest& volume, const Matrix4& localToWorld) const {
		return volume.TestAABB(localAABB(), localToWorld);
	}

	void renderSolid(Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const {
		renderer.SetState(m_entity.getEntityClass().m_state_fill, Renderer::eFullMaterials);
		renderer.addRenderable(m_aabb_solid, localToWorld);
	}
	void renderWireframe(Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const {
		renderer.SetState(m_entity.getEntityClass().m_state_wire, Renderer::eWireframeOnly);
		renderer.addRenderable(m_aabb_wire, localToWorld);
		if (g_showNames) {
			renderer.addRenderable(m_renderName, localToWorld);
		}
	}


	void testSelect(Selector& selector, SelectionTest& test, const Matrix4& localToWorld) {
		test.BeginMesh(localToWorld);

		SelectionIntersection best;
		aabb_testselect(m_aabb_local, test, best);
		if (best.valid()) {
			selector.addIntersection(best);
		}
	}

	void translate(const Vector3& translation) {
		m_origin = origin_translated(m_origin, translation);
	}
	void rotate(const Quaternion& rotation) {
	}
	void snapto(float snap) {
		m_originKey.m_origin = origin_snapped(m_originKey.m_origin, snap);
		m_originKey.write(&m_entity);
	}
	void revertTransform() {
		m_origin = m_originKey.m_origin;
	}
	void freezeTransform() {
		m_originKey.m_origin = m_origin;
		m_originKey.write(&m_entity);
	}
	void transformChanged() {
		revertTransform();
		m_evaluateTransform();
		updateTransform();
	}
	typedef MemberCaller<MiscParticle, &MiscParticle::transformChanged> TransformChangedCaller;
};

class MiscParticleInstance :
			public TargetableInstance,
			public TransformModifier,
			public Renderable,
			public SelectionTestable {
	class TypeCasts {
		InstanceTypeCastTable m_casts;
	public:
		TypeCasts() {
			m_casts = TargetableInstance::StaticTypeCasts::instance().get();
			InstanceContainedCast<MiscParticleInstance, Bounded>::install(m_casts);
			InstanceContainedCast<MiscParticleInstance, Cullable>::install(m_casts);
			InstanceStaticCast<MiscParticleInstance, Renderable>::install(m_casts);
			InstanceStaticCast<MiscParticleInstance, SelectionTestable>::install(m_casts);
			InstanceStaticCast<MiscParticleInstance, Transformable>::install(m_casts);
			InstanceIdentityCast<MiscParticleInstance>::install(m_casts);
		}
		InstanceTypeCastTable& get() {
			return m_casts;
		}
	};

	MiscParticle& m_contained;
	mutable AABB m_bounds;
public:

	typedef LazyStatic<TypeCasts> StaticTypeCasts;

	Bounded& get(NullType<Bounded>) {
		return m_contained;
	}
	Cullable& get(NullType<Cullable>) {
		return m_contained;
	}

	STRING_CONSTANT(Name, "MiscParticleInstance");

	MiscParticleInstance(const scene::Path& path, scene::Instance* parent, MiscParticle& contained) :
			TargetableInstance(path, parent, this, StaticTypeCasts::instance().get(), contained.getEntity(), *this),
			TransformModifier(MiscParticle::TransformChangedCaller(contained), ApplyTransformCaller(*this)),
			m_contained(contained) {
		m_contained.instanceAttach(Instance::path());

		StaticRenderableConnectionLines::instance().attach(*this);
	}
	~MiscParticleInstance() {
		StaticRenderableConnectionLines::instance().detach(*this);

		m_contained.instanceDetach(Instance::path());
	}

	void renderSolid(Renderer& renderer, const VolumeTest& volume) const {
		m_contained.renderSolid(renderer, volume, Instance::localToWorld());
	}
	void renderWireframe(Renderer& renderer, const VolumeTest& volume) const {
		m_contained.renderWireframe(renderer, volume, Instance::localToWorld());
	}

	void testSelect(Selector& selector, SelectionTest& test) {
		m_contained.testSelect(selector, test, Instance::localToWorld());
	}

	void evaluateTransform() {
		if (getType() == TRANSFORM_PRIMITIVE) {
			m_contained.translate(getTranslation());
			m_contained.rotate(getRotation());
		}
	}
	void applyTransform() {
		m_contained.revertTransform();
		evaluateTransform();
		m_contained.freezeTransform();
	}
	typedef MemberCaller<MiscParticleInstance, &MiscParticleInstance::applyTransform> ApplyTransformCaller;
};

class MiscParticleNode :
			public scene::Node::Symbiot,
			public scene::Instantiable,
			public scene::Cloneable {
	class TypeCasts {
		NodeTypeCastTable m_casts;
	public:
		TypeCasts() {
			NodeStaticCast<MiscParticleNode, scene::Instantiable>::install(m_casts);
			NodeStaticCast<MiscParticleNode, scene::Cloneable>::install(m_casts);
			NodeContainedCast<MiscParticleNode, Snappable>::install(m_casts);
			NodeContainedCast<MiscParticleNode, TransformNode>::install(m_casts);
			NodeContainedCast<MiscParticleNode, Entity>::install(m_casts);
			NodeContainedCast<MiscParticleNode, Nameable>::install(m_casts);
			NodeContainedCast<MiscParticleNode, Namespaced>::install(m_casts);
		}
		NodeTypeCastTable& get() {
			return m_casts;
		}
	};


	InstanceSet m_instances;

	scene::Node m_node;
	MiscParticle m_contained;

public:
	typedef LazyStatic<TypeCasts> StaticTypeCasts;

	Snappable& get(NullType<Snappable>) {
		return m_contained;
	}
	TransformNode& get(NullType<TransformNode>) {
		return m_contained.getTransformNode();
	}
	Entity& get(NullType<Entity>) {
		return m_contained.getEntity();
	}
	Nameable& get(NullType<Nameable>) {
		return m_contained.getNameable();
	}
	Namespaced& get(NullType<Namespaced>) {
		return m_contained.getNamespaced();
	}

	MiscParticleNode(EntityClass* eclass) :
			m_node(this, this, StaticTypeCasts::instance().get()),
			m_contained(eclass, m_node, InstanceSet::TransformChangedCaller(m_instances), InstanceSetEvaluateTransform<MiscParticleInstance>::Caller(m_instances)) {
	}
	MiscParticleNode(const MiscParticleNode& other) :
			scene::Node::Symbiot(other),
			scene::Instantiable(other),
			scene::Cloneable(other),
			m_node(this, this, StaticTypeCasts::instance().get()),
			m_contained(other.m_contained, m_node, InstanceSet::TransformChangedCaller(m_instances), InstanceSetEvaluateTransform<MiscParticleInstance>::Caller(m_instances)) {
	}

	scene::Node& node() {
		return m_node;
	}

	scene::Node& clone() const {
		return (new MiscParticleNode(*this))->node();
	}

	scene::Instance* create(const scene::Path& path, scene::Instance* parent) {
		return new MiscParticleInstance(path, parent, m_contained);
	}
	void forEachInstance(const scene::Instantiable::Visitor& visitor) {
		m_instances.forEachInstance(visitor);
	}
	void insert(scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance) {
		m_instances.insert(observer, path, instance);
	}
	scene::Instance* erase(scene::Instantiable::Observer* observer, const scene::Path& path) {
		return m_instances.erase(observer, path);
	}
};

scene::Node& New_MiscParticle(EntityClass* eclass) {
	MiscParticle_construct();
	return (new MiscParticleNode(eclass))->node();
}
