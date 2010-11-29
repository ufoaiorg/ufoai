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

#if !defined(INCLUDED_RENDERER_H)
#define INCLUDED_RENDERER_H

#include "irender.h"
#include "ientity.h"
#include "ifilter.h"
#include "renderable.h"
#include "iselection.h"
#include "cullable.h"
#include "scenelib.h"
#include "eclasslib.h"
#include "math/frustum.h"
#include <vector>

inline Cullable* Instance_getCullable (scene::Instance& instance)
{
	return dynamic_cast<Cullable*>(&instance);
}

inline VolumeIntersectionValue Cullable_testVisible (scene::Instance& instance, const VolumeTest& volume,
		VolumeIntersectionValue parentVisible)
{
	if (parentVisible == VOLUME_PARTIAL) {
		Cullable* cullable = Instance_getCullable(instance);
		if (cullable != 0) {
			return cullable->intersectVolume(volume, instance.localToWorld());
		}
	}
	return parentVisible;
}

#include "render/frontend/CullingWalker.h"
#include "render/frontend/ForEachVisible.h"
template<typename Functor>
inline void Scene_forEachVisible (scene::Graph& graph, const VolumeTest& volume, const Functor& functor)
{
	graph.traverse(ForEachVisible<CullingWalker<Functor> > (volume, CullingWalker<Functor> (volume, functor)));
}

#include "render/frontend/RenderHighlighted.h"
inline void Scene_Render (Renderer& renderer, const VolumeTest& volume)
{
	GlobalSceneGraph().traverse(ForEachVisible<RenderHighlighted> (volume, RenderHighlighted(renderer, volume)));
	GlobalShaderCache().forEachRenderable(RenderHighlighted::RenderCaller(RenderHighlighted(renderer, volume)));
}

#endif
