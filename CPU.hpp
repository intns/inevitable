#ifndef _CPU_HPP
#define _CPU_HPP

#include <chrono>
#include <memory>
#include <mutex>

#include "InterruptController.hpp"
#include "IScheduler.hpp"
#include "util.hpp"

class Process;
struct ProcessControlBlock;

template <typename Clock>
using TimePoint       = std::chrono::time_point<Clock>;
using SteadyTimePoint = TimePoint<std::chrono::steady_clock>;

class CPU {
public:
	NON_COPYABLE(CPU)

	CPU(std::unique_ptr<IScheduler> scheduler)
	    : mScheduler(std::move(scheduler))
	    , mActiveProcess(nullptr)
	{
	}

	~CPU() = default;

	void TerminateProcess(ProcessControlBlock* process);
	void AddProcess(ProcessControlBlock* process);
	void AssignPID(ProcessControlBlock& process);
	void ContextSwitch(ProcessControlBlock* next);
	void SleepForTime(std::uint64_t amount);
	void Run();
	void Step();

	inline const std::unique_ptr<IScheduler>& GetScheduler() const { return mScheduler; }
	inline ProcessControlBlock* GetCurrentProcess() { return mActiveProcess; }

	inline bool IsPreemptionAllowed() const
	{
		switch (mScheduler->GetAlgorithm()) {
		case SchedulingAlgorithm::FCFS:
		case SchedulingAlgorithm::SJF:
			return false;

		case SchedulingAlgorithm::Priority:
		case SchedulingAlgorithm::SRTF:
		case SchedulingAlgorithm::RoundRobin:
			return true;
		}

		PanicExit("UNKNOWN SCHEDULING TYPE IN SCHEDULER!");
	}

private:
	void HandlePriorityAging();
	void CheckPriorityPreempts();

	// Synchronisation
	std::mutex mMutex;
	std::uint64_t mTick = 0;

	// Scheduling
	std::uint64_t mQuantumTimer = 0;
	std::unique_ptr<IScheduler> mScheduler;

	// Interrupts & Processes
	InterruptController mIrqController;
	ProcessControlBlock* mActiveProcess = nullptr;

	// State
	SteadyTimePoint mIdleStartTime;
	bool mIsActive = true;
	bool mIsIdle   = true;
};

#endif
