#ifndef _PRIORITYSCHEDULER_HPP
#define _PRIORITYSCHEDULER_HPP

#include <deque>
#include <vector>
#include <mutex>

#include "../IScheduler.hpp"
#include "../util.hpp"

// For specific function info see 'IScheduler.hpp'
class PriorityScheduler : public IScheduler {
public:
	virtual ~PriorityScheduler() = default;

	ProcessControlBlock* PopNext() override;
	std::vector<ProcessControlBlock*> GetProcessList() const override;
	std::vector<ProcessControlBlock*> GetReadyList() const override;
	bool IsFullProcessListEmpty() const override;

	SchedulingAlgorithm GetAlgorithm() const override { return SchedulingAlgorithm::Priority; }

	void OnNewProcess(ProcessControlBlock*) override;
	void OnReadyProcess(ProcessControlBlock*) override;
	void OnTerminate(ProcessControlBlock*) override;

private:
	void SortReady();

	mutable std::mutex mMutex;
	std::vector<ProcessControlBlock*> mReadyList;
	std::vector<ProcessControlBlock*> mFullProcessList;
};

#endif