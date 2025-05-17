#include "PriorityScheduler.hpp"
#include "../Process.hpp"
#include "../CPU.hpp"

#include <algorithm>

void PriorityScheduler::OnNewProcess(ProcessControlBlock* pcb)
{
	std::scoped_lock lk(mMutex);
	mFullProcessList.push_back(pcb);

	if (pcb->mState == ProcessState::Ready) {
		mReadyList.push_back(pcb);
	}
}

void PriorityScheduler::OnReadyProcess(ProcessControlBlock* pcb)
{
	std::scoped_lock lk(mMutex);

	CPU* parent                  = pcb->mProcess.GetParentCPU();
	ProcessControlBlock* current = parent->GetCurrentProcess();

	// Check if we should preempt the current process
	if (current && pcb->mPriority > current->mPriority) {
		ThreadPrint("[PRIO] PID[", pcb->mProcessIdentifier, "] (PRIO ", pcb->mPriority, ") PREEMPTS PID[", current->mProcessIdentifier,
		            "] (PRIO ", current->mPriority, ")");

		mReadyList.push_back(current);
		parent->ContextSwitch(pcb);
	} else {
		mReadyList.push_back(pcb);
	}

	// Sort the ready list
	SortReady();
}

void PriorityScheduler::OnTerminate(ProcessControlBlock* pcb)
{
	std::scoped_lock lk(mMutex);
	std::erase(mFullProcessList, pcb);
	std::erase(mReadyList, pcb);
}

void PriorityScheduler::SortReady()
{
	std::sort(mReadyList.begin(), mReadyList.end(),
	          [&](ProcessControlBlock*& a, ProcessControlBlock*& b) { return a->mPriority > b->mPriority; });
}

ProcessControlBlock* PriorityScheduler::PopNext()
{
	std::scoped_lock lk(mMutex);
	if (mReadyList.empty()) {
		return nullptr;
	}

	SortReady();

	ProcessControlBlock* next = mReadyList.front();
	mReadyList.erase(mReadyList.begin());
	return next;
}

std::vector<ProcessControlBlock*> PriorityScheduler::GetProcessList() const
{
	std::scoped_lock lk(mMutex);
	return mFullProcessList;
}

std::vector<ProcessControlBlock*> PriorityScheduler::GetReadyList() const
{
	std::scoped_lock lk(mMutex);
	return mReadyList;
}

bool PriorityScheduler::IsFullProcessListEmpty() const
{
	std::scoped_lock lk(mMutex);
	return mFullProcessList.empty();
}
