#pragma once

#include "cullable.h"
#include "renderable.h"
#include "editable.h"

#include "iregistry.h"

#include "selectionlib.h"
#include "instancelib.h"
#include "transformlib.h"
#include "traverselib.h"
#include "entitylib.h"
#include "eclasslib.h"
#include "render.h"
#include "pivot.h"

#include "../targetable.h"
#include "../keys/OriginKey.h"
#include "../keys/AnglesKey.h"
#include "../keys/ScaleKey.h"
#include "../model.h"
#include "../namedentity.h"
#include "../keys/KeyObserverMap.h"
#include "../NameKeys.h"

class MiscModel: public Snappable {
	EntityKeyValues m_entity;
	KeyObserverMap m_keyObservers;
	MatrixTransform m_transform;

	OriginKey m_originKey;
	Vector3 m_origin;
	AnglesKey m_anglesKey;
	Vector3 m_angles;
	ScaleKey m_scaleKey;
	Vector3 m_scale;

	SingletonModel m_model;

	NamedEntity m_named;
	NameKeys m_nameKeys;
	RenderablePivot m_renderOrigin;
	RenderableNamedEntity m_renderName;

	Callback m_transformChanged;
	Callback m_evaluateTransform;

	void construct ();

	void updateTransform ();

	void originChanged ();
	typedef MemberCaller<MiscModel, &MiscModel::originChanged> OriginChangedCaller;

	void anglesChanged ();
	typedef MemberCaller<MiscModel, &MiscModel::anglesChanged> AnglesChangedCaller;

	void scaleChanged ();
	typedef MemberCaller<MiscModel, &MiscModel::scaleChanged> ScaleChangedCaller;

public:

	MiscModel (EntityClass* eclass, scene::Node& node, const Callback& transformChanged,
			const Callback& evaluateTransform);
	MiscModel (const MiscModel& other, scene::Node& node, const Callback& transformChanged,
			const Callback& evaluateTransform);

	InstanceCounter m_instanceCounter;
	void instanceAttach (const scene::Path& path);
	void instanceDetach (const scene::Path& path);

	EntityKeyValues& getEntity ();
	const EntityKeyValues& getEntity () const;

	scene::Traversable& getTraversable ();
	const scene::Traversable& getTraversable () const;

	Namespaced& getNamespaced ();
	NamedEntity& getNameable ();
	const NamedEntity& getNameable () const;
	TransformNode& getTransformNode ();
	const TransformNode& getTransformNode () const;
	void attach (scene::Traversable::Observer* observer);
	void detach (scene::Traversable::Observer* observer);

	void renderSolid (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld, bool selected) const;
	void renderWireframe (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld,
			bool selected) const;

	void translate (const Vector3& translation);
	void rotate (const Quaternion& rotation);
	void scale (const Vector3& scaling);
	void snapto (float snap);
	void revertTransform ();
	void freezeTransform ();
	void transformChanged ();
	typedef MemberCaller<MiscModel, &MiscModel::transformChanged> TransformChangedCaller;
};
