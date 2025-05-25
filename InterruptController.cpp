#include <iostream>
#include <string>

#include "InterruptController.hpp"
#include "Process.hpp"
#include "util.hpp"
#include "CPU.hpp"

InterruptController::InterruptController()
    : mIoThread([this](std::stop_token st) { this->IOWorker(st); })
{
}

InterruptController::~InterruptController()
{
	mIoThread.request_stop();

	// Wake up!! It's bed time!!
	mCv.notify_one();
}

void InterruptController::NotifyBlocked(ProcessControlBlock* pcb)
{
	std::lock_guard lg(mMutex);

	// Only enqueue if not already pending
	REQUIRE(std::find(mNewBlocks.begin(), mNewBlocks.end(), pcb) == mNewBlocks.end());

	mNewBlocks.push_back(pcb);
	mCv.notify_one();
}

void InterruptController::IOWorker(std::stop_token st)
{
	std::unique_lock lock(mMutex);

	while (!st.stop_requested()) {
		// Add any new blocks to the pending events list
		auto now = std::chrono::steady_clock::now();
		for (ProcessControlBlock* pcb : mNewBlocks) {
			IOEvent newEvent;
			newEvent.mPcb  = pcb;
			newEvent.mWhen = now + std::chrono::milliseconds(pcb->mProcess.GetBurst()->mDuration);
			mPendingEvents.push(newEvent);
		}
		mNewBlocks.clear();

		// Wait for a new blocked process if there are no pending events
		if (mPendingEvents.empty()) {
			mCv.wait(lock, [&] { return st.stop_requested() || !mNewBlocks.empty(); });
			continue;
		}

		// We have pending events, so wait until that's done, or if a stop is requested
		auto nextTime = mPendingEvents.top().mWhen;
		mCv.wait_until(lock, nextTime, [&] { return st.stop_requested() || !mNewBlocks.empty(); });
		if (st.stop_requested()) {
			break;
		}

		// If your time has come, so be it
		now = std::chrono::steady_clock::now();
		while (!mPendingEvents.empty() && mPendingEvents.top().mWhen <= now) {
			IOEvent top = mPendingEvents.top();
			mPendingEvents.pop();

			ProcessControlBlock* pcb = top.mPcb;

			// Consume the I/O burst
			pcb->mProcess.PopCurrentBurst();

			// If there are any bursts remaining, re-ready it
			if (pcb->mProcess.GetBurst()) {
				ThreadPrint("PID[", pcb->mProcessIdentifier, "] - > [UNBLOCKED FROM I/O BURST]");
				pcb->mState.store(ProcessState::Ready);
				pcb->mProcess.GetParentCPU()->AddProcess(pcb);
			} else {
				ThreadPrint("PID[", pcb->mProcessIdentifier, "] - > [EXIT FROM I/O BURST]");
				pcb->mState.store(ProcessState::Terminated);
				pcb->mProcess.GetParentCPU()->TerminateProcess(pcb);
			}
		}
	}
}
