#include <algorithm>
#include <iostream>
#include <vector>

#include "IScheduler.hpp"
#include "Process.hpp"
#include "util.hpp"
#include "CPU.hpp"
#include "rng.hpp"

void CPU::AddProcess(ProcessControlBlock* other)
{
	switch (other->mState.load()) {
	case ProcessState::Created:
		SleepForTime(cfg::gProcessCreationCost);
		AssignPID(*other);
		other->mState.store(ProcessState::Ready);
		mScheduler->OnNewProcess(other);
		break;
	case ProcessState::Ready:
		mScheduler->OnReadyProcess(other);
		break;

	case ProcessState::Running:
	case ProcessState::Blocked:
	case ProcessState::Terminated:
	default:
		PanicMsg("[SCHEDULER] TRY ADD PROCESS ISN'T NEW OR READY (?)");
		break;
	}
}

void CPU::SleepForTime(std::uint64_t timeInMs) { std::this_thread::sleep_for(std::chrono::milliseconds(timeInMs)); }

void CPU::TerminateProcess(ProcessControlBlock* process)
{
	std::scoped_lock lk(mMutex);

	mScheduler->OnTerminate(process);
	if (mScheduler->IsFullProcessListEmpty()) {
		ThreadPrint("NO PROCESSES REMAIN, EXITING...");
		mIsActive = false;
	}

	// Just to be sure
	process->mState.store(ProcessState::Terminated);
	ThreadPrint("PID[", process->mProcessIdentifier, "] TERMINATED\r\n");

	if (mActiveProcess == process) {
		mActiveProcess = nullptr;
	}
}

void CPU::AssignPID(ProcessControlBlock& process)
{
	//! NOTE: No need to worry about race conditions due to program control flow

	// The process ID is the first non-used incremental number starting from 0.
	// So if [P - 0] [P - 1] [P - 3], the next would be assigned [P - 2].
	const auto& processList = mScheduler->GetProcessList();
	std::size_t n           = processList.size();

	// Mark which IDs 0..n are taken
	std::vector<std::int8_t> used(n + 1, false);
	for (ProcessControlBlock* pcb : processList) {
		std::uint32_t id = pcb->mProcessIdentifier;
		if (id <= n) {
			used[id] = 1;
		}
	}

	// Scan for the first hole
	for (std::size_t i = 0; i <= n; ++i) {
		if (!used[i]) {
			process.mProcessIdentifier = static_cast<std::uint16_t>(i);
			return;
		}
	}
}

void CPU::ContextSwitch(ProcessControlBlock* block)
{
	REQUIRE(block != nullptr);

	// Critical section
	{
		std::scoped_lock lk(mMutex);

		// It's been swapped with something else
		if (mActiveProcess) {
			mActiveProcess->mState.store(ProcessState::Ready);

			ProcessWork* burst = mActiveProcess->mProcess.GetBurst();
			if (burst) {
				if (!burst->IsComplete()) {
					ThreadPrint("PID[", mActiveProcess->mProcessIdentifier, "] - > SPENT [", burst->mProgress, " ticks] IN WORK");
				}

				// Burst is still in progress, so update the prediction
				mActiveProcess->mProcess.UpdatePredictedBurst();
			}

			mActiveProcess = nullptr;
		}

		// Pretend to save data from previous PCB, flush TLS, etc.
		SleepForTime(cfg::gDispatchLatency);

		mActiveProcess = block;
		mActiveProcess->mState.store(ProcessState::Running);

		if (mScheduler->GetAlgorithm() == SchedulingAlgorithm::Priority) {
			mActiveProcess->mInactivePriorityTimer = 0;
		}

		mQuantumTimer = 0;
	}

	std::stringstream ss;

	ss << "[D/L - " << cfg::gDispatchLatency << "ms] ";

	// Print the duration of idle CPU time, if we were just idle for X amount of time
	if (mIsIdle) {
		using namespace std::literals;
		const auto end  = std::chrono::steady_clock::now();
		auto difference = end - mIdleStartTime;
		ss << "CPU IDLED FOR [" << difference / 1ms << "ms (" << difference / 1s << "s)] [" << mActiveProcess->mProcessIdentifier
		   << "] IS ACTIVE";

		mIsIdle = false;
	} else {
		ss << "[" << mActiveProcess->mProcessIdentifier << "] IS ACTIVE";
	}

	ThreadPrint(ss.str());
}

void CPU::Run()
{
	const auto processCount = mScheduler->GetProcessList().size();

	// Reset state
	mTick         = 0;
	mQuantumTimer = 0;

	mIsIdle   = false;
	mIsActive = true;

	mActiveProcess = nullptr;

	// Execution begins!
	while (mIsActive) {
		Step();
	}

	ThreadPrint("CPU TERMINATED EXECUTION [", mTick, "] TICKS WITH [", processCount, "] PROCESSES\r\n");
}

void CPU::Step()
{
	{
		std::scoped_lock lk(mMutex);
		if (mActiveProcess) {
			ProcessState state = mActiveProcess->mState.load();

			if (state != ProcessState::Running) {
				ThreadPrint("PID[", mActiveProcess->mProcessIdentifier, "] STATE CHANGED TO [", StateToString(state),
				            "] EXTERNALLY -> DROPPING FROM CPU");
				mActiveProcess = nullptr;
			}
		}
	}

	mTick++;

	// Handle priority bumping after ... time
	if (mScheduler->GetAlgorithm() == SchedulingAlgorithm::Priority) {
		HandlePriorityAging();
	}

	if (mActiveProcess) {
		// PCB can only be running in this control flow; see if statement above

		// If I/O burst: transition to blocked, notify and drop the PCB
		Process& proc      = mActiveProcess->mProcess;
		ProcessWork* burst = proc.GetBurst();

		if (!burst) {
			// No computation left, we're done
			mActiveProcess->mState.store(ProcessState::Terminated);
			ThreadPrint("PID[", mActiveProcess->mProcessIdentifier, "] DONE");
			return;
		}

		// [If I/O] Block immediately; IOWorker will resume it later
		if (burst->mType == ProcessWork::Type::IO) {
			ThreadPrint("PID[", mActiveProcess->mProcessIdentifier, "] - > [BLOCKED I/O FOR ", burst->mDuration, "ms]");
			mActiveProcess->mState.store(ProcessState::Blocked);
			mIrqController.NotifyBlocked(mActiveProcess);
			mActiveProcess = nullptr;
			return;
		}

		// Otherwise it's a CPU burst
		bool isProcDone = proc.Step();

		mActiveProcess->mProgramCounter++;

		// Process has completed execution, transition to done!
		if (isProcDone) {
			TerminateProcess(mActiveProcess);
			return;
		}

		const SchedulingAlgorithm algo = mScheduler->GetAlgorithm();

		// Handle decay every 1500 ticks
		if (algo == SchedulingAlgorithm::Priority) {
			if (mTick % 1500 == 0 && mActiveProcess->mPriority > mActiveProcess->mBasePriority) {
				mActiveProcess->mPriority--;
				ThreadPrint("[PRIO] PID[", mActiveProcess->mProcessIdentifier, "] DECAYED TO [", mActiveProcess->mPriority, "]");
				CheckPriorityPreempts();
			}
		}

		// Handle timeslice pre-emption for RR if we're not done
		if (algo == SchedulingAlgorithm::RoundRobin) {
			mQuantumTimer++;

			if (mQuantumTimer >= cfg::gRoundRobinTimeQuantum) {
				if (!mScheduler->GetReadyList().empty()) {
					// If there is a process after this, we'll transition to that one
					ThreadPrint("[RR] TIMESLICE ENDED");

					ProcessControlBlock* currentPcb = mActiveProcess;
					ContextSwitch(mScheduler->PopNext());
					mScheduler->OnReadyProcess(currentPcb);
				} else {
					// No context switch occurs but we'll get a fresh quantum regardless
					mQuantumTimer = 0;
				}
			}
		}

		return;
	}

	// No active PCB at the moment, let the scheduler decide!
	ProcessControlBlock* next = mScheduler->PopNext();

	if (next) {
		ContextSwitch(next);
	} else if (!mIsIdle) {
		mIsIdle        = true;
		mIdleStartTime = std::chrono::steady_clock::now();
	}
}

void CPU::HandlePriorityAging()
{
	const auto& readyList                 = mScheduler->GetReadyList();
	ProcessControlBlock* highestPrioReady = nullptr;

	for (ProcessControlBlock* process : readyList) {
		// Skip the currently active process
		if (process == mActiveProcess) {
			continue;
		}

		// Handle priority aging for the process
		std::uint64_t& prioTimer = process->mInactivePriorityTimer;
		if (++prioTimer > 5000) {
			// Check against the max value for the priority type
			if (process->mPriority < std::numeric_limits<decltype(process->mPriority)>::max()) {
				++process->mPriority;
				ThreadPrint("[PRIO] PID[", process->mProcessIdentifier, "] BUMPED TO [", process->mPriority, "]");
			}

			prioTimer = 0;
		}

		// Keep track of the highest priority ready process found so far
		if (!highestPrioReady || process->mPriority > highestPrioReady->mPriority) {
			highestPrioReady = process;
		}
	}

	// Perform the preemption check after the loop
	if (highestPrioReady && mActiveProcess && highestPrioReady->mPriority > mActiveProcess->mPriority) {
		ThreadPrint("[PRIO] PID[", highestPrioReady->mProcessIdentifier, "] (PRIO ", highestPrioReady->mPriority, ") PREEMPTS PID[",
		            mActiveProcess->mProcessIdentifier, "] (PRIO ", mActiveProcess->mPriority, ") AFTER AGING");

		ProcessControlBlock* oldActive = mActiveProcess;
		ContextSwitch(mScheduler->PopNext());
		mScheduler->OnReadyProcess(oldActive);
	}
}

void CPU::CheckPriorityPreempts()
{
	const auto& readyList                 = mScheduler->GetReadyList();
	ProcessControlBlock* highestPrioReady = nullptr;

	for (ProcessControlBlock* process : readyList) {
		// Skip current process
		if (process == mActiveProcess) {
			continue;
		}

		if (!highestPrioReady || process->mPriority > highestPrioReady->mPriority) {
			highestPrioReady = process;
		}
	}

	// After everything, check if preemption is OK
	if (highestPrioReady && mActiveProcess && highestPrioReady->mPriority > mActiveProcess->mPriority) {
		ThreadPrint("[PRIO] PID[", highestPrioReady->mProcessIdentifier, "] (PRIO ", highestPrioReady->mPriority, ") PREEMPTS PID[",
		            mActiveProcess->mProcessIdentifier, "] (PRIO ", mActiveProcess->mPriority, ") AFTER AGING");

		ProcessControlBlock* oldActive = mActiveProcess;
		ContextSwitch(mScheduler->PopNext());
		mScheduler->OnReadyProcess(oldActive);
	}
}