#ifndef _RRSCHEDULER_HPP
#define _RRSCHEDULER_HPP

#include "FCFSScheduler.hpp"

// RR is just FCFS with a time quantum (the running time is tracked)
class RRScheduler : public FCFSScheduler {
public:
	virtual ~RRScheduler() = default;

	virtual SchedulingAlgorithm GetAlgorithm() const { return SchedulingAlgorithm::RoundRobin; }
};

#endif