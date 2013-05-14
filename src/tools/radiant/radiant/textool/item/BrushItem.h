#pragma once

#include "../../brush/Brush.h"
#include "../TexToolItem.h"

namespace textool {

class BrushItem :
	public TexToolItem
{
	// The brush this control is referring to
	Brush& _sourceBrush;

public:
	// Constructor, allocates all child FacItems
	BrushItem(Brush& sourceBrush);

	~BrushItem();

	/** greebo: Saves the undoMemento of this brush,
	 * 			so that the operation can be undone later.
	 */
	virtual void beginTransformation();

}; // class BrushItem

} // namespace TexTool
