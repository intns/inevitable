#ifndef _SCHEDULER_HPP
#define _SCHEDULER_HPP

#include <vector>
#include <deque>
#include <shared_mutex>
#include "util.hpp"

struct ProcessControlBlock;

enum class SchedulingAlgorithm : std::uint32_t {
	FCFS = 0,   // First come, first served
	SJF,        // Shortest job first (no pre-emption, unable to stop the process until it's done)
	SRTF,       // Shortest remaining time first (preempt if a new process comes with burst length < current burst)
	RoundRobin, // Give a quantum (CPU time) to each process so everything gets work done slowly
	Priority,   // Higher priority -> front of the list
};

struct IScheduler {
	virtual ~IScheduler() = default;

	////////////////////////
	// 'GETTER' FUNCTIONS //
	////////////////////////
	
	virtual std::vector<ProcessControlBlock*> GetProcessList() const = 0;
	virtual std::vector<ProcessControlBlock*> GetReadyList() const = 0;

	// Check if the full process list is empty (not the ready queue)
	virtual bool IsFullProcessListEmpty() const = 0;

	// Gets the algorithm 'this' Scheduler implements
	virtual SchedulingAlgorithm GetAlgorithm() const = 0;

	//////////////////////////////////
	// TRANSITORY / STATE FUNCTIONS //
	//////////////////////////////////

	// Selects and pops next process (or nullptr)
	virtual ProcessControlBlock* PopNext() = 0;

	// Called when a process first enters the system (after PCB assignment, etc.)
	virtual void OnNewProcess(ProcessControlBlock*) = 0;

	// Called when a process becomes ready (initial arrival or after I/O, preemption, etc.)
	virtual void OnReadyProcess(ProcessControlBlock*) = 0;

	// Removing a certain process from all data structures in the scheduler
	virtual void OnTerminate(ProcessControlBlock*) = 0;
};

#endif
