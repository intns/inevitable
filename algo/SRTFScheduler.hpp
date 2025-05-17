#ifndef _SRTFSCHEDULER_HPP
#define _SRTFSCHEDULER_HPP

#include <vector>
#include <deque>
#include <mutex>

#include "../IScheduler.hpp"
#include "../util.hpp"

// For specific function info see 'IScheduler.hpp'
class SRTFScheduler : public IScheduler {
public:
	virtual ~SRTFScheduler() = default;

	ProcessControlBlock* PopNext() override;
	std::vector<ProcessControlBlock*> GetProcessList() const override;
	std::vector<ProcessControlBlock*> GetReadyList() const override;
	bool IsFullProcessListEmpty() const override;

	SchedulingAlgorithm GetAlgorithm() const override { return SchedulingAlgorithm::SRTF; }

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