#pragma once

#include "iregistry.h"

#include "cullable.h"
#include "renderable.h"
#include "editable.h"

#include "selectionlib.h"
#include "instancelib.h"
#include "transformlib.h"
#include "traverselib.h"
#include "entitylib.h"
#include "render.h"
#include "eclasslib.h"
#include "pivot.h"

#include "../targetable.h"
#include "../keys/OriginKey.h"
#include "../keys/AngleKey.h"
#include "../model.h"
#include "../namedentity.h"
#include "../keys/KeyObserverMap.h"
#include "../NameKeys.h"

class EclassModel: public Snappable {
	MatrixTransform m_transform;
	EntityKeyValues m_entity;
	KeyObserverMap m_keyObservers;

	OriginKey m_originKey;
	Vector3 m_origin;
	AngleKey m_angleKey;
	float m_angle;
	SingletonModel m_model;

	NamedEntity m_named;
	NameKeys m_nameKeys;
	RenderablePivot m_renderOrigin;
	RenderableNamedEntity m_renderName;

	Callback m_transformChanged;
	Callback m_evaluateTransform;

	void construct ();

	void updateTransform ();
	typedef MemberCaller<EclassModel, &EclassModel::updateTransform> UpdateTransformCaller;

	void originChanged ();
	typedef MemberCaller<EclassModel, &EclassModel::originChanged> OriginChangedCaller;

	void angleChanged ();
	typedef MemberCaller<EclassModel, &EclassModel::angleChanged> AngleChangedCaller;

	InstanceCounter m_instanceCounter;

public:

	EclassModel (EntityClass* eclass, scene::Node& node, const Callback& transformChanged,
			const Callback& evaluateTransform);
	EclassModel (const EclassModel& other, scene::Node& node, const Callback& transformChanged,
			const Callback& evaluateTransform);

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
	void snapto (float snap);
	void revertTransform ();
	void freezeTransform ();
	void transformChanged ();
	typedef MemberCaller<EclassModel, &EclassModel::transformChanged> TransformChangedCaller;
};
