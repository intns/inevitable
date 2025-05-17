#ifndef _FCFCSSCHEDULER_HPP
#define _FCFCSSCHEDULER_HPP

#include <vector>
#include <deque>
#include <mutex>

#include "../IScheduler.hpp"
#include "../util.hpp"

// For specific function info see 'IScheduler.hpp'
class FCFSScheduler : public IScheduler {
public:
	virtual ~FCFSScheduler() = default;

	ProcessControlBlock* PopNext() override;
	std::vector<ProcessControlBlock*> GetProcessList() const override;
	std::vector<ProcessControlBlock*> GetReadyList() const override;
	bool IsFullProcessListEmpty() const override;

	SchedulingAlgorithm GetAlgorithm() const override { return SchedulingAlgorithm::FCFS; }

	void OnNewProcess(ProcessControlBlock*) override;
	void OnReadyProcess(ProcessControlBlock*) override;
	void OnTerminate(ProcessControlBlock*) override;

private:
	mutable std::mutex mMutex;
	std::deque<ProcessControlBlock*> mReadyList;
	std::vector<ProcessControlBlock*> mFullProcessList;
};

#endif