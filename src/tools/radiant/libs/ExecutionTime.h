#ifndef EXECUTIONTIME_H_
#define EXECUTIONTIME_H_

#ifdef DEBUG
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#include "itextstream.h"
#include "string/string.h"
#endif

class ExecutionTime {
private:
	const std::string _id;
#ifdef DEBUG
	mutable timespec start;

	inline timespec diff (const timespec &start, const timespec &end) const
	{
		timespec temp;
		if ((end.tv_nsec - start.tv_nsec) < 0) {
			temp.tv_sec = end.tv_sec - start.tv_sec - 1;
			temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
		} else {
			temp.tv_sec = end.tv_sec - start.tv_sec;
			temp.tv_nsec = end.tv_nsec - start.tv_nsec;
		}
		return temp;
	}
#endif
public:
	ExecutionTime (const std::string& id) :
			_id(id)
	{
#ifdef DEBUG
		start.tv_sec = 0;
		start.tv_nsec = 0;
		clock_gettime(CLOCK_REALTIME, &start);
#endif
	}

	~ExecutionTime ()
	{
#ifdef DEBUG
		timespec end;
		end.tv_sec = 0;
		end.tv_nsec = 0;
		clock_gettime(CLOCK_REALTIME, &end);
		timespec d = diff(start, end);
		globalOutputStream() << _id << ": " << string::format("%i,%09i", d.tv_sec, d.tv_nsec) << "s\n";
#endif
	}
};

#endif /* EXECUTIONTIME_H_ */
