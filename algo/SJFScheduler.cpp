#include "SJFScheduler.hpp"
#include "../Process.hpp"
#include "../CPU.hpp"

#include <sstream>
#include <algorithm>

void SJFScheduler::OnNewProcess(ProcessControlBlock* pcb)
{
	std::scoped_lock lk(mMutex);

	mFullProcessList.push_back(pcb);

	if (pcb->mState.load() == ProcessState::Ready) {
		mReadyList.push_back(pcb);
	}
}

void SJFScheduler::OnReadyProcess(ProcessControlBlock* pcb)
{
	std::scoped_lock lk(mMutex);
	mReadyList.push_back(pcb);
}

void SJFScheduler::OnTerminate(ProcessControlBlock* pcb)
{
	std::scoped_lock lk(mMutex);

	std::erase(mFullProcessList, pcb);
	std::erase(mReadyList, pcb);
}

void SJFScheduler::SortReady()
{
	std::sort(mReadyList.begin(), mReadyList.end(), [&](auto*& a, auto*& b) {
		const float_t abl = a->mProcess.GetPredictedBurstLength();
		const float_t bbl = b->mProcess.GetPredictedBurstLength();

		return abl < bbl;
	});
}

ProcessControlBlock* SJFScheduler::PopNext()
{
	std::scoped_lock lk(mMutex);

	if (mReadyList.empty()) {
		return nullptr;
	}

	SortReady();

	ProcessControlBlock* next = *mReadyList.begin();

#ifndef NDEBUG
	if (!mReadyList.empty()) {
		float_t shortestPredicted = next->mProcess.GetPredictedBurstLength();
		for (const auto& elem : mReadyList) {
			REQUIRE(elem->mProcess.GetPredictedBurstLength() >= shortestPredicted);
		}
	}
#endif

	mReadyList.erase(mReadyList.begin());
	return next;
}

std::vector<ProcessControlBlock*> SJFScheduler::GetProcessList() const
{
	std::scoped_lock lk(mMutex);
	return mFullProcessList;
}

std::vector<ProcessControlBlock*> SJFScheduler::GetReadyList() const
{
	std::scoped_lock lk(mMutex);
	return std::vector<ProcessControlBlock*>(mReadyList.begin(), mReadyList.end());
}

bool SJFScheduler::IsFullProcessListEmpty() const
{
	std::scoped_lock lk(mMutex);
	return mFullProcessList.empty();
}
