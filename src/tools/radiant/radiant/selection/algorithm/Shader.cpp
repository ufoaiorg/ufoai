#include "Shader.h"

#include "iregistry.h"
#include "iselection.h"
#include "iscenegraph.h"
#include "selectable.h"
#include "selectionlib.h"
#include "../../brush/FaceInstance.h"
#include "../../brush/BrushVisit.h"
#include "../../brush/TextureProjection.h"
#include "Primitives.h"
#include "../shaderclipboard/ShaderClipboard.h"
#include "../../sidebar/surfaceinspector/surfaceinspector.h"

#include "gtkutil/dialog.h"
#include "radiant_i18n.h"

// greebo: Nasty global that contains all the selected face instances
extern FaceInstanceSet g_SelectedFaceInstances;

namespace selection {
namespace algorithm {

class AmbiguousShaderException: public std::runtime_error
{
	public:
		// Constructor
		AmbiguousShaderException (const std::string& what) :
			std::runtime_error(what)
		{
		}
};

/** greebo: Cycles through all the Faces and throws as soon as
 * at least two different non-empty shader names are found.
 *
 * @throws: AmbiguousShaderException
 */
class UniqueFaceShaderFinder
{
		// The string containing the result
		std::string& _shader;

	public:
		UniqueFaceShaderFinder (std::string& shader) :
			_shader(shader)
		{
		}

		void operator() (FaceInstance& face) const
		{

			const std::string& foundShader = face.getFace().GetShader();

			if (foundShader != "$NONE" && _shader != "$NONE" && _shader != foundShader) {
				throw AmbiguousShaderException(foundShader);
			}

			_shader = foundShader;
		}
};

std::string getShaderFromSelection ()
{
	std::string returnValue("");

	const SelectionInfo& selectionInfo = GlobalSelectionSystem().getSelectionInfo();

	if (selectionInfo.totalCount > 0) {
		std::string faceShader("$NONE");

		// BRUSHES
		// If there are no FaceInstances selected, cycle through the brushes
		if (g_SelectedFaceInstances.empty()) {
			// Try to get the unique shader from the selected brushes
			try {
				// Go through all the selected brushes and their faces
				Scene_ForEachSelectedBrush_ForEachFaceInstance(GlobalSceneGraph(), UniqueFaceShaderFinder(faceShader));
			} catch (AmbiguousShaderException &a) {
				faceShader = "";
			}
		} else {
			// Try to get the unique shader from the faces
			try {
				g_SelectedFaceInstances.foreach(UniqueFaceShaderFinder(faceShader));
			} catch (AmbiguousShaderException &a) {
				faceShader = "";
			}
		}

		if (faceShader != "$NONE") {
			// Only a faceShader has been found
			returnValue = faceShader;
		}
	}

	return returnValue;
}

/** greebo: Applies the shader from the clipboard to the given <target> face
 */
inline void applyClipboardShaderToFace (Face& target)
{
	// Get a reference to the source Texturable in the clipboard
	Texturable& source = GlobalShaderClipboard().getSource();

	// Retrieve the textureprojection from the source face
	TextureProjection projection;
	source.face->GetTexdef(projection);

	target.SetShader(source.face->GetShader());
	target.SetTexdef(projection);
	target.SetFlags(source.face->getShader().m_flags);
}

inline void applyClipboardToTexturable (Texturable& target, bool entireBrush)
{
	// Get a reference to the source Texturable in the clipboard
	Texturable& source = GlobalShaderClipboard().getSource();

	// Check the basic conditions
	if (!target.empty() && !source.empty()) {
		// Do we have a Face to copy from?
		if (source.isFace()) {
			if (target.isFace() && entireBrush) {
				// Copy Face >> Whole Brush
				for (Brush::const_iterator i = target.brush->begin(); i != target.brush->end(); ++i) {
					applyClipboardShaderToFace(*(*i));
				}
			} else if (target.isFace() && !entireBrush) {
				// Copy Face >> Face
				applyClipboardShaderToFace(*target.face);
			}
		}
	}
}

void pasteShader (SelectionTest& test, bool entireBrush)
{
	// Construct the command string
	std::string command("pasteShader");
	command += (entireBrush ? "ToBrush" : "");

	UndoableCommand undo(command);

	// Initialise an empty Texturable structure
	Texturable target;

	// Find a suitable target Texturable
	GlobalSceneGraph().traverse(ClosestTexturableFinder(test, target));

	// Pass the call to the algorithm function taking care of all the IFs
	applyClipboardToTexturable(target, entireBrush);

	SceneChangeNotify();
	// Update the Texture Tools
	ui::SurfaceInspector::Instance().queueUpdate();
}

/** greebo: This applies the clipboard to the visited faces.
 */
class ClipboardShaderApplicator
{
	public:
		ClipboardShaderApplicator ()
		{
		}

		void operator() (Face& face) const
		{
			Texturable target;
			target.face = &face;
			// Apply the shader (projected, not to the entire brush)
			applyClipboardToTexturable(target, false);
		}
};

void pasteShaderToSelection ()
{
	if (GlobalShaderClipboard().getSource().empty()) {
		return;
	}

	// Start a new command
	UndoableCommand command("pasteShaderToSelection");

	// Cycle through all selected brushes
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_ForEachSelectedBrush_ForEachFace(GlobalSceneGraph(), ClipboardShaderApplicator());
	}

	// Cycle through all selected components
	Scene_ForEachSelectedBrushFace(ClipboardShaderApplicator());

	SceneChangeNotify();
	// Update the Texture Tools
	ui::SurfaceInspector::Instance().queueUpdate();
}

void pasteShaderNaturalToSelection ()
{
	if (GlobalShaderClipboard().getSource().empty()) {
		return;
	}

	// Start a new command
	UndoableCommand command("pasteShaderNaturalToSelection");

	// Cycle through all selected brushes
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_ForEachSelectedBrush_ForEachFace(GlobalSceneGraph(), ClipboardShaderApplicator());
	}

	// Cycle through all selected components
	Scene_ForEachSelectedBrushFace(ClipboardShaderApplicator());

	SceneChangeNotify();
	// Update the Texture Tools
	ui::SurfaceInspector::Instance().queueUpdate();
}

void pickShaderFromSelection ()
{
	GlobalShaderClipboard().clear();

	if (selectedFaceCount() == 1) {
		try {
			Face& sourceFace = getLastSelectedFace();
			GlobalShaderClipboard().setSource(sourceFace);
		} catch (InvalidSelectionException &e) {
			gtkutil::errorDialog(_("Can't copy Shader. Couldn't retrieve face."));
		}
	} else {
		// Nothing to do, this works for patches only
		gtkutil::errorDialog(_("Can't copy Shader. Please select a single face."));
	}
}

class FaceGetTexdef
{
		TextureProjection& m_projection;
	public:
		FaceGetTexdef (TextureProjection& projection) :
			m_projection(projection)
		{
		}
		void operator() (Face& face) const
		{
			face.GetTexdef(m_projection);
		}
};

TextureProjection getSelectedTextureProjection ()
{
	TextureProjection returnValue;

	if (GlobalSelectionSystem().areFacesSelected()) {
		// Get the last selected face instance from the global
		FaceInstance& faceInstance = g_SelectedFaceInstances.last();
		faceInstance.getFace().GetTexdef(returnValue);
	} else {
		Scene_ForEachSelectedBrush_ForEachFace(GlobalSceneGraph(), FaceGetTexdef(returnValue));
	}

	return returnValue;
}

Vector2 getSelectedFaceShaderSize ()
{
	Vector2 returnValue(0, 0);

	if (selectedFaceCount() == 1) {
		// Get the last selected face instance from the global
		FaceInstance& faceInstance = g_SelectedFaceInstances.last();

		const FaceShader& shader = faceInstance.getFacePtr()->getShader();
		returnValue[0] = shader.width();
		returnValue[1] = shader.height();
	}

	return returnValue;
}

/** greebo: Applies the given texture repeat to the visited face
 */
class FaceTextureFitter
{
		float _repeatS, _repeatT;
	public:
		FaceTextureFitter (float repeatS, float repeatT) :
			_repeatS(repeatS), _repeatT(repeatT)
		{
		}

		void operator() (Face& face) const
		{
			face.FitTexture(_repeatS, _repeatT);
		}
};

void fitTexture (const float& repeatS, const float& repeatT)
{
	UndoableCommand command("fitTexture");

	// Cycle through all selected brushes
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_ForEachSelectedBrush_ForEachFace(GlobalSceneGraph(), FaceTextureFitter(repeatS, repeatT));
	}

	// Cycle through all selected components
	Scene_ForEachSelectedBrushFace(FaceTextureFitter(repeatS, repeatT));

	SceneChangeNotify();
}

/** greebo: Applies the default texture projection to all
 * the visited faces.
 */
class FaceTextureProjectionSetter
{
		TextureProjection& _projection;
	public:
		FaceTextureProjectionSetter (TextureProjection& projection) :
			_projection(projection)
		{
		}

		void operator() (Face& face) const
		{
			face.SetTexdef(_projection);
		}
};

void naturalTexture ()
{
	UndoableCommand undo("naturalTexture");

	TextureProjection projection;
	projection.constructDefault();

	// Faces
	Scene_ForEachSelectedBrushFace(FaceTextureProjectionSetter(projection));

	SceneChangeNotify();
}

void applyTextureProjectionToFaces (TextureProjection& projection)
{
	UndoableCommand undo("textureProjectionSetSelected");

	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_ForEachSelectedBrush_ForEachFace(GlobalSceneGraph(), FaceTextureProjectionSetter(projection));
	}

	// Faces
	Scene_ForEachSelectedBrushFace(FaceTextureProjectionSetter(projection));

	SceneChangeNotify();
}

/** greebo: Translates the texture of the visited faces
 * about the specified <shift> Vector2
 */
class FaceTextureShifter
{
		const Vector2& _shift;
	public:
		FaceTextureShifter (const Vector2& shift) :
			_shift(shift)
		{
		}

		void operator() (Face& face) const
		{
			face.ShiftTexdef(_shift[0], _shift[1]);
		}
};

void shiftTexture (const Vector2& shift)
{
	std::string command("shiftTexture: ");
	command += "s=" + string::toString(shift[0]) + ", t=" + string::toString(shift[1]);

	UndoableCommand undo(command);

	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_ForEachSelectedBrush_ForEachFace(GlobalSceneGraph(), FaceTextureShifter(shift));
	}
	// Translate the face textures
	Scene_ForEachSelectedBrushFace(FaceTextureShifter(shift));

	SceneChangeNotify();
}

/** greebo: Scales the texture of the visited faces
 * about the specified x,y-scale values in the given Vector2
 */
class FaceTextureScaler
{
		const Vector2& _scale;
	public:
		FaceTextureScaler (const Vector2& scale) :
			_scale(scale)
		{
		}

		void operator() (Face& face) const
		{
			face.ScaleTexdef(_scale[0], _scale[1]);
		}
};

/** greebo: Rotates the texture of the visited faces
 * about the specified angle
 */
class FaceTextureRotater
{
		const float& _angle;
	public:
		FaceTextureRotater (const float& angle) :
			_angle(angle)
		{
		}

		void operator() (Face& face) const
		{
			face.RotateTexdef(_angle);
		}
};

inline void scaleTexture (const Vector2& scale)
{
	std::string command("scaleTexture: ");
	command += "sScale=" + string::toString(scale[0]) + ", tScale=" + string::toString(scale[1]);

	UndoableCommand undo(command);

	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_ForEachSelectedBrush_ForEachFace(GlobalSceneGraph(), FaceTextureScaler(scale));
	}
	// Scale the face textures
	Scene_ForEachSelectedBrushFace(FaceTextureScaler(scale));

	SceneChangeNotify();
}

inline void rotateTexture (const float& angle)
{
	std::string command("rotateTexture: ");
	command += "angle=" + string::toString(angle);

	UndoableCommand undo(command);

	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_ForEachSelectedBrush_ForEachFace(GlobalSceneGraph(), FaceTextureRotater(angle));
	}

	// Rotate the face textures
	Scene_ForEachSelectedBrushFace(FaceTextureRotater(angle));

	SceneChangeNotify();
}

void shiftTextureLeft ()
{
	shiftTexture(Vector2(-GlobalRegistry().getFloat("user/ui/textures/surfaceInspector/hShiftStep"), 0.0f));
}

void shiftTextureRight ()
{
	shiftTexture(Vector2(GlobalRegistry().getFloat("user/ui/textures/surfaceInspector/hShiftStep"), 0.0f));
}

void shiftTextureUp ()
{
	shiftTexture(Vector2(0.0f, GlobalRegistry().getFloat("user/ui/textures/surfaceInspector/vShiftStep")));
}

void shiftTextureDown ()
{
	shiftTexture(Vector2(0.0f, -GlobalRegistry().getFloat("user/ui/textures/surfaceInspector/vShiftStep")));
}

void scaleTextureLeft ()
{
	scaleTexture(Vector2(-GlobalRegistry().getFloat("user/ui/textures/surfaceInspector/hScaleStep"), 0.0f));
}

void scaleTextureRight ()
{
	scaleTexture(Vector2(GlobalRegistry().getFloat("user/ui/textures/surfaceInspector/hScaleStep"), 0.0f));
}

void scaleTextureUp ()
{
	scaleTexture(Vector2(0.0f, GlobalRegistry().getFloat("user/ui/textures/surfaceInspector/vScaleStep")));
}

void scaleTextureDown ()
{
	scaleTexture(Vector2(0.0f, -GlobalRegistry().getFloat("user/ui/textures/surfaceInspector/vScaleStep")));
}

void rotateTextureClock ()
{
	rotateTexture(fabs(GlobalRegistry().getFloat("user/ui/textures/surfaceInspector/rotStep")));
}

void rotateTextureCounter ()
{
	rotateTexture(-fabs(GlobalRegistry().getFloat("user/ui/textures/surfaceInspector/rotStep")));
}

/** greebo: This replaces the shader of the visited face/patch with <replace>
 * 			if the face is textured with <find> and increases the given <counter>.
 */
class ShaderReplacer: public BrushInstanceVisitor
{
		const std::string _find;
		const std::string _replace;
		mutable int _counter;
	public:
		ShaderReplacer (const std::string& find, const std::string& replace) :
			_find(find), _replace(replace), _counter(0)
		{
		}

		int getReplacedCount () const
		{
			return _counter;
		}

		void operator() (Face& face) const
		{
			if (face.getShader().getShader() == _find) {
				face.SetShader(_replace);
				_counter++;
			}
		}

		// BrushInstanceVisitor implementation
		virtual void visit (FaceInstance& face) const
		{
			Face& f = face.getFace();
			if (f.getShader().getShader() == _find) {
				f.SetShader(_replace);
				_counter++;
			}
		}
};

int findAndReplaceShader (const std::string& find, const std::string& replace, bool selectedOnly)
{
	if (find.empty() || replace.empty())
		return 0;

	std::string command("textureFindReplace");
	command += "-find " + find + " -replace " + replace;
	UndoableCommand undo(command);

	// Construct a visitor class
	ShaderReplacer replacer(find, replace);

	if (selectedOnly) {
		if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
			// Find & replace all the brush shaders
			Scene_ForEachSelectedBrush_ForEachFace(GlobalSceneGraph(), replacer);
		}

		// Search the single selected faces
		Scene_ForEachSelectedBrushFace(replacer);
	} else {
		Scene_ForEachBrush_ForEachFace(GlobalSceneGraph(), replacer);
	}

	return replacer.getReplacedCount();
}

/** greebo: Flips the visited object about the axis given to the constructor.
 */
class TextureFlipper
{
		unsigned int _flipAxis;
	public:
		TextureFlipper (unsigned int flipAxis) :
			_flipAxis(flipAxis)
		{
		}

		void operator() (Face& face) const
		{
			face.flipTexture(_flipAxis);
		}
};

void flipTexture (unsigned int flipAxis)
{
	UndoableCommand undo("flipTexture");

	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		// Flip the texture of all the brushes (selected as a whole)
		Scene_ForEachSelectedBrush_ForEachFace(GlobalSceneGraph(), TextureFlipper(flipAxis));
	}
	// Now flip all the seperately selected faces
	Scene_ForEachSelectedBrushFace(TextureFlipper(flipAxis));
	SceneChangeNotify();
}

void flipTextureS ()
{
	flipTexture(0);
}

void flipTextureT ()
{
	flipTexture(1);
}

} // namespace algorithm
} // namespace selection
