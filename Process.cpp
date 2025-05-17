#include <iostream>
#include <string>
#include <sstream>
#include "Process.hpp"
#include "CPU.hpp"
#include "rng.hpp"

Process::Process(std::size_t bursts, CPU* parent, ProcessControlBlock* parentBlock)
    : mParentCpu(parent)
    , mParentBlock(parentBlock)
{
	// Populate our work buffer
	std::bernoulli_distribution coin(0.7f); // 70% - CPU, 30% - IO
	std::uniform_int_distribution<std::uint32_t> cpuDurationRange(100, 2500);
	std::uniform_int_distribution<std::uint32_t> ioDurationRange(1000, 7500);

	for (std::size_t i = 0; i < bursts; ++i) {
		if (coin(rng::GetRandomEngine())) {
			mWork.push({ ProcessWork::Type::CPU, cpuDurationRange(rng::GetRandomEngine()) });
		} else {
			mWork.push({ ProcessWork::Type::IO, ioDurationRange(rng::GetRandomEngine()) });
		}
	}

	mPredictedBurstLength = mPreviousPredictedLength = static_cast<float_t>(cfg::gInitialBurstPrediction);
}

bool Process::Step()
{
	// We're out of work to do, all done!
	if (mWork.empty()) {
		return true;
	}

	ProcessWork* burst = GetBurst();
	if (burst->Step()) {
		// If the burst is complete, pop it and keep going
		UpdatePredictedBurst();

		mWork.pop();

		// We're out of work to do, all done!
		if (mWork.empty()) {
			return true;
		}

		std::stringstream ss;
		ss << "[" << mParentBlock->mProcessIdentifier << "] - > SPENT [" << burst->mDuration << " ticks] IN WORK";

		// Only show predicted burst length if contextually relevant (SRTF / SJF)
		auto algorithm = mParentCpu->GetScheduler()->GetAlgorithm();
		if (algorithm == SchedulingAlgorithm::SJF || algorithm == SchedulingAlgorithm::SRTF) {
			ss << " ~[" << GetRemainingPredictedBurstLength() << "ms]";
		}
		ThreadPrint(ss.str());
	}

	// Keep going!
	return false;
}

void Process::UpdatePredictedBurst()
{
	// If non-existent, hasn't progressed at all, or isn't CPU, exit
	const ProcessWork* burst = GetBurst();
	if (!burst || !burst->mProgress || burst->mType != ProcessWork::Type::CPU) {
		return;
	}

	mPreviousPredictedLength = mPredictedBurstLength;
	const float_t progress   = static_cast<float_t>(burst->mProgress);

	// tau_next = alpha * t_n + (1 - alpha) * tau_n
	constexpr float_t a   = 0.5f; // [0 -> 1] 1 = recent bursts mean more
	mPredictedBurstLength = a * progress + (1.0f - a) * mPreviousPredictedLength;
}

void Process::PopCurrentBurst()
{
	if (!mWork.empty()) {
		mWork.pop();
	}
}

ProcessWork* Process::GetBurst() { return mWork.empty() ? nullptr : &mWork.front(); }

const std::queue<ProcessWork>& Process::GetWorkQueue() const { return mWork; }

float_t Process::GetRemainingPredictedBurstLength()
{
	ProcessWork* burst = GetBurst();

	if (!burst || burst->mType != ProcessWork::Type::CPU) {
		return 0.0f;
	}

	UpdatePredictedBurst();
	return std::max(0.0f, mPredictedBurstLength - static_cast<float_t>(burst->mProgress));
}

ProcessControlBlock::ProcessControlBlock(CPU* parentCpu)
    : mProcess(rng::GetUniformRandomNumber(rng::RandomIntRange(cfg::gProcessBurstMinimum, cfg::gProcessBurstMaximum)), parentCpu, this)
{
	mState.store(ProcessState::Created);

	if (parentCpu->GetScheduler()->GetAlgorithm() == SchedulingAlgorithm::Priority) {
		mPriority     = static_cast<std::uint16_t>(rng::GetUniformRandomNumber(rng::RandomIntRange(0, 10)));
		mBasePriority = mPriority;
	} else {
		mBasePriority = mPriority = 0;
	}
}

ProcessControlBlock::~ProcessControlBlock()
{
	ThreadPrint("PID[", mProcessIdentifier, "] IS TERMINATING");
	REQUIRE(mState.load() == ProcessState::Terminated);
}