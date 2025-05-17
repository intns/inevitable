#ifndef _PROCESS_HPP
#define _PROCESS_HPP

#include <optional>
#include <queue>
#include "util.hpp"

class CPU;
struct ProcessControlBlock;

struct ProcessWork {
	// Work is only CPU / IO in this simulation, IO being non-CPU work
	enum class Type {
		CPU = 0,
		IO,
	};

	ProcessWork(Type t, std::uint32_t d)
	    : mType(t)
	    , mDuration(d)
	{
	}

	// True if complete, false if not
	inline bool Step() { return ++mProgress == mDuration; }
	inline bool IsComplete() const noexcept { return mProgress == mDuration; }

	inline const char* GetTypeString() const { return mType == Type::CPU ? "CPU" : "I/O"; }

	Type mType              = Type::CPU;
	std::uint32_t mDuration = 0;
	std::uint32_t mProgress = 0;
};

// A process / thread is really just a list of 'work' for the CPU to complete
class Process {
public:
	NON_COPYABLE(Process)

	Process(std::size_t bursts, CPU* parent, ProcessControlBlock* parentBlock);
	Process() = delete;

	inline void AssignCPU(CPU* p) { mParentCpu = p; }

	bool Step();

	void UpdatePredictedBurst();
	void PopCurrentBurst();
	ProcessWork* GetBurst();
	inline const std::queue<ProcessWork>& GetWorkQueue() const;

	inline const float_t GetPredictedBurstLength() const { return mPredictedBurstLength; }
	float_t GetRemainingPredictedBurstLength();

	inline CPU* GetParentCPU() { return mParentCpu; }

private:
	float_t mPredictedBurstLength    = 0.0f;
	float_t mPreviousPredictedLength = 0.0f;

	std::queue<ProcessWork> mWork;
	CPU* mParentCpu                   = nullptr;
	ProcessControlBlock* mParentBlock = nullptr;
};

enum class ProcessState {
	Created = 0,
	Ready,
	Running,
	Blocked, // AKA Waiting
	Terminated,
};

constexpr const char* StateToString(ProcessState st)
{
	switch (st) {
	case ProcessState::Created:
		return "CREATED";
	case ProcessState::Ready:
		return "READY";
	case ProcessState::Running:
		return "RUNNING";
	case ProcessState::Blocked:
		return "BLOCKED";
	case ProcessState::Terminated:
		return "TERMINATED";
	default:
		PanicExit("UNKNOWN STATE IN PROCESS");
	}
}

struct ProcessControlBlock {
	NON_COPYABLE(ProcessControlBlock)

	explicit ProcessControlBlock(CPU* parentCpu);
	~ProcessControlBlock();

	// Process state
	std::atomic<ProcessState> mState = ProcessState::Created; // The process state
	std::uint32_t mProcessIdentifier = 0;                     // PID

	// Priority
	std::uint32_t mBasePriority          = 0; // The original priority
	std::uint32_t mPriority              = 0;
	std::uint64_t mInactivePriorityTimer = 0; // For bumping priority after time

	// Process
	std::uint32_t mProgramCounter = 0; // How many 'instructions' have been executed
	Process mProcess;                  // The process this block controls / contains information about

	// ... Would also contain CPU registers, etc.
};

#endif
