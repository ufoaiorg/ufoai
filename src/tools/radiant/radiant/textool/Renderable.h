#ifndef TEXTOOL_RENDERABLE_H_
#define TEXTOOL_RENDERABLE_H_

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

#endif /*TEXTOOL_RENDERABLE_H_*/
