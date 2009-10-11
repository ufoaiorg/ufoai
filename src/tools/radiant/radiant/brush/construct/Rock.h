#ifndef ROCK_H_
#define ROCK_H_

#include "BrushConstructor.h"

namespace brushconstruct
{
	class Rock: public BrushConstructor
	{
		private:
			static const std::size_t _minSides = 10;
			static const std::size_t _maxSides = 1000;

		public:
			void generate (Brush& brush, const AABB& bounds, std::size_t sides, const TextureProjection& projection,
					const std::string& shader);

			const std::string getName() const;

			static BrushConstructor& getInstance ()
			{
				static Rock _rock;
				return _rock;
			}
	};
}
#endif /* ROCK_H_ */
