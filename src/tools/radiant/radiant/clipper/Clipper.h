#ifndef GLOBALCLIPPOINTS_H_
#define GLOBALCLIPPOINTS_H_

#include "ClipPoint.h"
#include "math/aabb.h"

#include "preferencesystem.h"
#include "iregistry.h"

namespace {
const unsigned int NUM_CLIP_POINTS = 3;
}

class BrushClipper: public Clipper, public RegistryKeyObserver, public PreferenceConstructor
{
	public:
		// Radiant Module stuff
		typedef Clipper Type;
		STRING_CONSTANT(Name, "*");

		// Return the static instance
		Clipper* getTable ()
		{
			return this;
		}

	private:
		// Hold the currently active xy view type
		EViewType _viewType;

		// The array holding the three possible clip points
		ClipPoint _clipPoints[NUM_CLIP_POINTS];

		// The pointer to the currently moved clip point
		ClipPoint* _movingClip;

		bool _switch;

		// Whether to use the _caulkShader texture for new brush faces
		bool _useCaulk;

		// The shader name used for new faces when _useCaulk is true
		std::string _caulkShader;

	public:
		// Constructor
		BrushClipper ();

		// The RegistryKeyObserver implementation, gets called when the observed keys change
		void keyChanged (const std::string& changedKey, const std::string& newValue);

		EViewType getViewType () const;
		void setViewType (EViewType viewType);

		ClipPoint* getMovingClip ();
		Vector3& getMovingClipCoords ();
		void setMovingClip (ClipPoint* clipPoint);

		// greebo: Cycles through the three possible clip points and returns the nearest to point (for selectiontest)
		// @returns: NULL, if no suitable point could be found
		ClipPoint* find (const Vector3& point, EViewType viewtype, float scale);

		// Draws the set clip points, the <scale> argument is needed for drawing the number id of the clip point
		void draw (float scale);

		// Returns true if at least two clip points are set
		bool valid () const;

		bool clipMode () const;

		void getPlanePoints (Vector3 planepts[3], const AABB& bounds) const;

		// Resets all clip points to zero and their "set" property to false
		void reset ();

		void update ();

		// Returns the shader name of the texture that gets applied to the surfaces created by the clip operation
		const std::string getShader () const;

		// Switches between front and back clip mode
		void flipClip ();

		// Clips the selected brushes using the current clip point selection
		void clip ();

		void splitClip ();

		// This gets called when the clip mode gets activated/deactivated
		void onClipMode (bool enabled);

		// Adds the given clip point coordinates
		void newClipPoint (const Vector3& point);

	private:

		// The method that is passed to the preference system
		void constructPreferencePage (PreferenceGroup& group);
};

#endif /*GLOBALCLIPPOINTS_H_*/
