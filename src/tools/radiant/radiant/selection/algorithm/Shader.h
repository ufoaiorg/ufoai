#ifndef SELECTION_ALGORITHM_SHADER_H_
#define SELECTION_ALGORITHM_SHADER_H_

#include <string>
#include "math/Vector2.h"

class TextureProjection;
class Face;
class SelectionTest;

namespace selection {
	namespace algorithm {

	/** greebo: Retrieves the shader name from the current selection.
	 *
	 * @returns: the name of the shader that is shared by every selected instance or
	 * the empty string "" otherwise.
	 */
	std::string getShaderFromSelection();

	/** greebo: Applies the shader in the clipboard to the nearest
	 * 			texturable object (using the given SelectionTest)
	 *
	 * @param test: the SelectionTest needed (usually a SelectionVolume).
	 *
	 * @param projected: Set this to TRUE if the texture is projected onto patches using the
	 * 			   face in the shaderclipboard as reference plane
	 * 			   Set this to FALSE if a natural texturing of patches is attempted.
	 *
	 * @param entireBrush: Set this to TRUE if all brush faces should be textured,
	 * 				 given the case that the SelectionTest is resulting in a brush.
	 */
	void pasteShader(SelectionTest& test, bool entireBrush = false);

	/** greebo: The command target of "CopyTexure". This tries to pick the shader
	 * 			from the current selection and copies it to the clipboard.
	 */
	void pickShaderFromSelection();

	/** greebo: The command target of "PasteTexture". This tries to get the Texturables
	 * 			from the current selection and pastes the clipboard shader onto them.
	 */
	void pasteShaderToSelection();

	/** greebo: The command target of "PasteTextureNatural". This tries to get
	 * 			the Texturables	from the current selection and
	 * 			pastes the clipboard shader "naturally" (undistorted) onto them.
	 */
	void pasteShaderNaturalToSelection();

	/** greebo: Retrieves the texture projection from the current selection.
	 *
	 * @return the TextureProjection of the last selected face/brush.
	 */
	TextureProjection getSelectedTextureProjection();

	/** greebo: Applies the given textureprojection to the selected
	 * brushes and brush faces.
	 */
	void applyTextureProjectionToFaces(TextureProjection& projection);

	/** greebo: Get the width/height of the shader of the last selected
	 * face instance.
	 *
	 * @returns: A Vector2 with <width, height> of the shader or <0,0> if empty.
	 */
	Vector2 getSelectedFaceShaderSize();

	/** greebo: Rescales the texture of the selected primitives to fit
	 * <repeatX> times horizontally and <repeatY> times vertically
	 */
	void fitTexture(const float& repeatS, const float& repeatT);

	/** greebo: Flips the texture about the given <flipAxis>
	 *
	 * @flipAxis: 0 = flip S, 1 = flip T
	 */
	void flipTexture (unsigned int flipAxis);

	/** greebo: The command Targets for flipping the textures about the
	 * 			S and T axes respectively.
	 * 			Passes the call to flipTexture() method above.
	 */
	void flipTextureS ();
	void flipTextureT ();

	/** greebo: Applies the texture "naturally" to the selected
	 * primitives. Natural makes use of the currently active default scale.
	 */
	void naturalTexture();

	/** greebo: Shifts the texture of the current selection about
	 * the specified Vector2
	 */
	void shiftTexture(const Vector2& shift);

	/** greebo: These are the shortcut methods that scale/shift/rotate the
	 * texture of the selected primitives about the values that are currently
	 * active in the Surface Inspector.
	 */
	void shiftTextureLeft();
	void shiftTextureRight();
	void shiftTextureUp();
	void shiftTextureDown();
	void scaleTextureLeft();
	void scaleTextureRight();
	void scaleTextureUp();
	void scaleTextureDown();
	void rotateTextureClock();
	void rotateTextureCounter();

	/** greebo: Replaces all <find> shaders with <replace>.
	 *
	 * @param find/replace: the full shadernames ("textures/darkmod/bleh")
	 * @param selectedOnly: if TRUE, searches the current selection only.
	 *
	 * @return the number of replaced occurrences.
	 */
	int findAndReplaceShader(const std::string& find,
							 const std::string& replace,
							 bool selectedOnly);

	} // namespace algorithm
} // namespace selection

#endif /*SELECTION_ALGORITHM_SHADER_H_*/
