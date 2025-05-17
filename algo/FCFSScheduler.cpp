#include "FCFSScheduler.hpp"
#include "../Process.hpp"

void FCFSScheduler::OnNewProcess(ProcessControlBlock* pcb)
{
	std::scoped_lock lk(mMutex);
	mFullProcessList.push_back(pcb);

	if (pcb->mState.load() == ProcessState::Ready) {
		mReadyList.push_back(pcb);
	}
}

void FCFSScheduler::OnReadyProcess(ProcessControlBlock* pcb)
{
	std::scoped_lock lk(mMutex);
	mReadyList.push_back(pcb);
}

void FCFSScheduler::OnTerminate(ProcessControlBlock* pcb)
{
	std::scoped_lock lk(mMutex);
	std::erase(mFullProcessList, pcb);
	std::erase(mReadyList, pcb);
}

ProcessControlBlock* FCFSScheduler::PopNext()
{
	std::scoped_lock lk(mMutex);
	if (mReadyList.empty()) {
		return nullptr;
	}

	ProcessControlBlock* next = mReadyList.front();
	mReadyList.pop_front();
	return next;
}

std::vector<ProcessControlBlock*> FCFSScheduler::GetProcessList() const
{
	std::scoped_lock lk(mMutex);
	return mFullProcessList;
}

std::vector<ProcessControlBlock*> FCFSScheduler::GetReadyList() const
{
	std::scoped_lock lk(mMutex);
	return std::vector<ProcessControlBlock*>(mReadyList.begin(), mReadyList.end());
}

bool FCFSScheduler::IsFullProcessListEmpty() const
{
	std::scoped_lock lk(mMutex);
	return mFullProcessList.empty();
}
