#pragma once

namespace textool {

class Renderable
{
public:
	virtual ~Renderable() {}

	/** greebo: Renders the object representation (points, lines, whatever).
	 */
	virtual void render() = 0;
};

} // namespace TexTool
