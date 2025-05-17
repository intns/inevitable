#include "SRTFScheduler.hpp"
#include "../Process.hpp"
#include "../CPU.hpp"

#include <sstream>
#include <algorithm>

void SRTFScheduler::OnNewProcess(ProcessControlBlock* pcb)
{
	std::scoped_lock lk(mMutex);

	mFullProcessList.push_back(pcb);

	if (pcb->mState.load() == ProcessState::Ready) {
		mReadyList.push_back(pcb);
	}
}

void SRTFScheduler::OnReadyProcess(ProcessControlBlock* newPcb)
{
	std::scoped_lock lk(mMutex);

	CPU* parent                    = newPcb->mProcess.GetParentCPU();
	ProcessControlBlock* oldPcb = parent->GetCurrentProcess();
	if (oldPcb && oldPcb->mProcess.GetRemainingPredictedBurstLength() > newPcb->mProcess.GetRemainingPredictedBurstLength()) {
		float_t currentRt = oldPcb->mProcess.GetRemainingPredictedBurstLength();
		float_t newRt     = newPcb->mProcess.GetRemainingPredictedBurstLength();
		
		ThreadPrint("[SRTF] PID[", oldPcb->mProcessIdentifier, "] (", currentRt, ") PREEMPT BY PID[", newPcb->mProcessIdentifier, "](", newRt,
		            ")");

		// Old -> New, and ready up Old
		parent->ContextSwitch(newPcb);
		mReadyList.push_back(oldPcb);
	} else {
		mReadyList.push_back(newPcb);
	}
}

void SRTFScheduler::OnTerminate(ProcessControlBlock* pcb)
{
	std::scoped_lock lk(mMutex);

	std::erase(mFullProcessList, pcb);
	std::erase(mReadyList, pcb);
}

void SRTFScheduler::SortReady()
{
	std::sort(mReadyList.begin(), mReadyList.end(), [&](auto*& a, auto*& b) {
		const float_t abl = a->mProcess.GetRemainingPredictedBurstLength();
		const float_t bbl = b->mProcess.GetRemainingPredictedBurstLength();

		return abl < bbl;
	});
}

ProcessControlBlock* SRTFScheduler::PopNext()
{
	std::scoped_lock lk(mMutex);

	if (mReadyList.empty()) {
		return nullptr;
	}

	SortReady();

	ProcessControlBlock* next = *mReadyList.begin();

#ifndef NDEBUG
	if (!mReadyList.empty()) {
		float_t shortestPredicted = next->mProcess.GetRemainingPredictedBurstLength();
		for (const auto& elem : mReadyList) {
			REQUIRE(elem->mProcess.GetRemainingPredictedBurstLength() >= shortestPredicted);
		}
	}
#endif

	mReadyList.erase(mReadyList.begin());
	return next;
}

std::vector<ProcessControlBlock*> SRTFScheduler::GetProcessList() const
{
	std::scoped_lock lk(mMutex);
	return mFullProcessList;
}

std::vector<ProcessControlBlock*> SRTFScheduler::GetReadyList() const
{
	std::scoped_lock lk(mMutex);
	return std::vector<ProcessControlBlock*>(mReadyList.begin(), mReadyList.end());
}

bool SRTFScheduler::IsFullProcessListEmpty() const
{
	std::scoped_lock lk(mMutex);
	return mFullProcessList.empty();
}
