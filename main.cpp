// TODO: Consider debug output? CSV -> load into an HTML project on my website?

#include <functional>
#include <algorithm>
#include <charconv>
#include <iostream>
#include <memory>
#include <string>
#include <cctype>
#include <thread>
#include <array>
#include <mutex>
#include <list>
#include <map>

#include "Process.hpp"
#include "util.hpp"
#include "rng.hpp"
#include "CPU.hpp"

#include "algo/PriorityScheduler.hpp"
#include "algo/SRTFScheduler.hpp"
#include "algo/FCFSScheduler.hpp"
#include "algo/SJFScheduler.hpp"
#include "algo/RRScheduler.hpp"

// Allow custom colours to work in Windows
#ifdef _WIN32
#include <windows.h>

namespace {
	void WIN_EnableColouredOutput()
	{
		// Get console handle
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hOut == INVALID_HANDLE_VALUE) {
			return;
		}

		// Get the current console mode
		DWORD dwMode = 0;
		if (!GetConsoleMode(hOut, &dwMode)) {
			return;
		}

		// Enable the virtual terminal processing flag
		dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		if (!SetConsoleMode(hOut, dwMode)) {
			return;
		}
	}
} // namespace
#endif

namespace cfg {
	std::uint32_t gProcessCreationCost    = 5;
	std::uint32_t gDispatchLatency        = 1000; // AKA cost of a context switch
	std::uint32_t gProcessBurstMinimum    = 5;
	std::uint32_t gProcessBurstMaximum    = 25;
	std::uint32_t gInitialBurstPrediction = 1000;
	std::uint32_t gRoundRobinTimeQuantum  = 2500;
} // namespace cfg

namespace {
	inline std::int64_t GetNumber(std::int64_t defaultValue)
	{
		std::string input;
		while (true) {
			if (!std::getline(std::cin, input)) {
				return defaultValue;
			}

			// Trim whitespace
			auto first = std::find_if_not(input.begin(), input.end(), [](unsigned char c) { return std::isspace(c); });
			auto last  = std::find_if_not(input.rbegin(), input.rend(), [](unsigned char c) { return std::isspace(c); }).base();

			if (first >= last) {
				// All whitespace (or empty)
				return defaultValue;
			}

			std::string_view sv(&*first, std::distance(first, last));

			// Parse the trimmed string
			std::int64_t value = 0;
			auto [ptr, ec]     = std::from_chars(sv.data(), sv.data() + sv.size(), value);
			if (ec == std::errc() && ptr == sv.data() + sv.size()) {
				return value;
			}
		}
	}

	inline std::int64_t GetProcesses(SchedulingAlgorithm algo)
	{
		std::cout << "[SETTINGS]" << std::endl;
		std::cout << "The following options are measured in ticks (ms):" << std::endl;

		std::cout << "1. What is the cost of creating a new process? [default - " << cfg::gProcessCreationCost << "] - ";
		cfg::gProcessCreationCost = static_cast<std::uint32_t>(GetNumber(cfg::gProcessCreationCost));

		std::cout << "2. What is the cost of a context switch? [default - " << cfg::gDispatchLatency << "] - ";
		cfg::gDispatchLatency = static_cast<std::uint32_t>(GetNumber(cfg::gDispatchLatency));

		std::cout << std::endl;
		std::cout << "3. The following options are measured in quantity:" << std::endl;

		std::cout << "4. What is the minimum burst count of a process? [default - " << cfg::gProcessBurstMinimum << "] - ";
		cfg::gProcessBurstMinimum = static_cast<std::uint32_t>(GetNumber(cfg::gProcessBurstMinimum));

		std::cout << "5. What is the maximum burst count of a process? [default - " << cfg::gProcessBurstMaximum << "] - ";
		cfg::gProcessBurstMaximum = static_cast<std::uint32_t>(GetNumber(cfg::gProcessBurstMaximum));

		std::cout << "6. How many processes do you want in this simulation? [default - 5] - ";
		std::int64_t procCount = static_cast<std::uint32_t>(GetNumber(5));

		if (algo == SchedulingAlgorithm::RoundRobin) {
			std::cout << "7. How long should the time quantum be? [default - " << cfg::gRoundRobinTimeQuantum << "] - ";
			cfg::gRoundRobinTimeQuantum = static_cast<std::uint32_t>(GetNumber(cfg::gRoundRobinTimeQuantum));
		}

		std::cout << "[/SETTINGS]" << std::endl << std::endl;

		return procCount;
	}

	static const std::map<SchedulingAlgorithm, std::function<std::unique_ptr<IScheduler>()>> SchedulerFactoryMap {
		{ SchedulingAlgorithm::FCFS, [] { return std::make_unique<FCFSScheduler>(); } },
		{ SchedulingAlgorithm::SJF, [] { return std::make_unique<SJFScheduler>(); } },
		{ SchedulingAlgorithm::SRTF, [] { return std::make_unique<SRTFScheduler>(); } },
		{ SchedulingAlgorithm::RoundRobin, [] { return std::make_unique<RRScheduler>(); } },
		{ SchedulingAlgorithm::Priority, [] { return std::make_unique<PriorityScheduler>(); } },
	};

	std::unique_ptr<IScheduler> MakeScheduler(SchedulingAlgorithm algo)
	{
		// Search and run the factory function, if found
		if (auto it = SchedulerFactoryMap.find(algo); it != SchedulerFactoryMap.end()) {
			return it->second();
		}

		PanicExit("UNKNOWN SCHEDULING ALGORITHM SUPPLIED");
	}

	constexpr std::array<std::pair<std::string_view, std::string_view>, 6> AlgorithmProsCons {
		{ // FCFS
		  { "Simple to implement; minimal scheduler overhead", "Can suffer convoy effect; poor average waiting time" },
		  // SJF
		  { "Minimizes average waiting time for known bursts", "Requires prior knowledge of burst lengths; risk of starvation" },
		  // SRTF
		  { "Preemptive variant of SJF; reacts to shorter arrivals", "High context-switching overhead; starvation of long jobs" },
		  // RoundRobin
		  { "Time-sharing fairness; no starvation if quantum chosen well",
		    "Quantum too small -> high overhead; too large -> degenerates to FCFS" },
		  // Priority
		  { "Controls task importance directly; flexible policy",
		    "Low-priority starvation; priority inversion without extra handling" } }
	};

	static const std::map<SchedulingAlgorithm, std::string_view> AlgorithmNameMap {
		{ SchedulingAlgorithm::FCFS, "FCFS - First Come First Served" },
		{ SchedulingAlgorithm::SJF, "SJF - Shortest Job First" },
		{ SchedulingAlgorithm::SRTF, "SRTF - Shortest Remaining Time First" },
		{ SchedulingAlgorithm::RoundRobin, "Round Robin" },
		{ SchedulingAlgorithm::Priority, "Priority" },
	};

	inline SchedulingAlgorithm GetAlgorithm()
	{
		std::size_t i = 0;
		for (const auto& elem : AlgorithmNameMap) {
			std::cout << "[" << i++ << "] - " << elem.second << std::endl;
		}

		std::cout << "Pick an algorithm to use: ";
		const auto out = static_cast<SchedulingAlgorithm>(GetNumber(0));
		std::cout << std::endl;

		return out;
	}

} // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
	std::cout << "inevitable - A basic CPU scheduling simulator" << std::endl
	          << "  by intns, 2025" << std::endl
	          << "---------------------------------------------" << std::endl
	          << std::endl;

#ifdef _WIN32
	// Enable custom colours to work in Windows consoles (why not allow ANSI escape codes by default?)
	WIN_EnableColouredOutput();
#endif

	SchedulingAlgorithm algo = GetAlgorithm();
	CPU cpu(MakeScheduler(algo));

	// [FACT CHECK] independent reviewers have deemed this: TRUE
	{
		std::string_view name = AlgorithmNameMap.at(algo);
		std::cout << name << std::endl;
		std::cout << std::string(name.length(), '-') << std::endl;

		auto& [pros, cons] = AlgorithmProsCons[static_cast<std::size_t>(algo)];
		std::cout << "Pros - " << pros << std::endl << "Cons - " << cons << std::endl;
		std::cout << "Is preemption enabled for this algorithm? " << "[" << (cpu.IsPreemptionAllowed() ? "YES" : "NO") << "]" << std::endl
		          << std::endl;
	}

	// Dynamically create all processes based on the users input
	std::size_t processes = static_cast<std::size_t>(GetProcesses(algo));
	std::list<ProcessControlBlock> pcbs;
	for (std::size_t i = 0; i < processes; ++i) {
		pcbs.emplace_back(&cpu);
		cpu.AddProcess(&pcbs.back());
	}

	cpu.Run();

	return EXIT_SUCCESS;
}
