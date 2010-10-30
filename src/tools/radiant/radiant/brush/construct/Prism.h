#ifndef PRISM_H_
#define PRISM_H_

#include "BrushConstructor.h"
#include "../../xyview/GlobalXYWnd.h"

namespace brushconstruct
{
	class Prism: public BrushConstructor
	{
		private:
			int getViewAxis () const
			{
				switch (GlobalXYWnd().getActiveViewType()) {
				case XY:
					return 2;
				case XZ:
					return 1;
				case YZ:
					return 0;
				}
				return 2;
			}

			float getMaxExtent2D (const Vector3& extents, int axis) const
			{
				switch (axis) {
				case 0:
					return std::max(extents[1], extents[2]);
				case 1:
					return std::max(extents[0], extents[2]);
				default:
					return std::max(extents[0], extents[1]);
				}
			}

		private:

			static const std::size_t _minSides = 3;
			static const std::size_t _maxSides = c_brush_maxFaces - 2;

		public:
			void generate (Brush& brush, const AABB& bounds, std::size_t sides, const TextureProjection& projection,
					const std::string& shader);

			const std::string getName() const;

			static BrushConstructor& getInstance ()
			{
				static Prism _prism;
				return _prism;
			}
	};
}
#endif /* PRISM_H_ */
