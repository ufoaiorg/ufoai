#ifndef RENDERSTATISTICS_H_
#define RENDERSTATISTICS_H_

#include "../timer.h"
#include "stream/stringstream.h"

namespace render {

class RenderStatistics {
		StringOutputStream _statStr;

		int _countPrims;
		int _countStates;
		int _countTransforms;

		Timer _timer;
	public:
		const std::string getStatString() {
			_statStr.clear();
			_statStr << "prims: " << _countPrims << " | states: " << _countStates
					<< " | transforms: " << _countTransforms << " | msec: "
					<< _timer.elapsed_msec();
			return _statStr.toString();
		}

		void increasePrimitive() {
			_countPrims++;
		}

		void increaseStates() {
			_countStates++;
		}

		void increaseTransforms() {
			_countTransforms++;
		}

		void resetStats() {
			_countPrims = 0;
			_countStates = 0;
			_countTransforms = 0;
			_timer.start();
		}

		static RenderStatistics& Instance() {
			static RenderStatistics _instance;
			return _instance;
		}
};

} // namespace render

#endif /*RENDERSTATISTICS_H_*/
