#ifndef _INTERRUPTCONTROLLER_HPP
#define _INTERRUPTCONTROLLER_HPP

#include <condition_variable>
#include <thread>
#include <vector>
#include <mutex>
#include <queue>

#include "util.hpp"

class CPU;
struct ProcessControlBlock;

struct IOEvent {
	std::chrono::steady_clock::time_point mWhen;
	ProcessControlBlock* mPcb = nullptr;
	bool operator<(IOEvent const& o) const { return mWhen > o.mWhen; }
};

class InterruptController {
public:
	NON_COPYABLE(InterruptController)

	InterruptController();
	~InterruptController();

	void NotifyBlocked(ProcessControlBlock*);

private:
	void IOWorker(std::stop_token);

	// Synchronisation
	std::jthread mIoThread;
	std::mutex mMutex;
	std::condition_variable mCv;

	// State
	std::vector<ProcessControlBlock*> mNewBlocks; // Newly blocked processes that haven't been added to pending events
	std::priority_queue<IOEvent> mPendingEvents;  // All pending events that are awaiting comp[letion
};

#endif
