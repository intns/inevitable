#ifndef UTIL_HPP
#define UTIL_HPP

#include <source_location>
#include <iostream>
#include <sstream>
#include <utility>
#include <iomanip>
#include <format>
#include <thread>
#include <mutex>
#include <chrono>

// To represent unique system resources
#define NON_COPYABLE(name)                 \
	name(const name&)            = delete; \
	name& operator=(const name&) = delete;

namespace cfg {
	// How long should it take to create a process? (in ticks)
	extern std::uint32_t gProcessCreationCost;

	// How many bursts can a process do? [min -> max]
	extern std::uint32_t gProcessBurstMinimum;
	extern std::uint32_t gProcessBurstMaximum;

	// How long does a context switch take to do? (in ms)
	extern std::uint32_t gDispatchLatency;

	// The initial predicted cost of a burst in a process (in ms)
	extern std::uint32_t gInitialBurstPrediction;

	// How long should processes be able to compute before being switched?
	extern std::uint32_t gRoundRobinTimeQuantum;

} // namespace cfg

namespace {
	// Defined underneath are two distinct types of assertions:
	// 1) Check   - reports but does NOT terminate
	// 2) Require - reports and terminates

	inline void PanicMsg(const char* msg, const std::source_location& origin = std::source_location::current())
	{
		std::cerr << origin.file_name() << ":" << origin.line() << " in [" << origin.function_name() << "] assertion failed: " << msg
		          << std::endl;
	}

	[[noreturn]] inline void PanicExit(const char* msg, const std::source_location& origin = std::source_location::current())
	{
		PanicMsg(msg, origin);
		std::terminate();
	}

	inline std::mutex gConsoleMutex;
	std::string ColorText(const std::string& text, int colorCode) { return "\033[" + std::to_string(colorCode) + "m" + text + "\033[0m"; }

	template <typename... Args>
	void ThreadPrint(Args&&... args)
	{
		std::lock_guard<std::mutex> lk(gConsoleMutex);
		std::stringstream stream;
		(stream << ... << std::forward<Args>(args));

		std::string message = stream.str();
		std::string prefix;
		int colorCode;

		if (message.find("TERMINATED") != std::string::npos) {
			prefix    = "[EXIT]\t\t| ";
			colorCode = 31; // Red
		} else if (message.find("SRTF") != std::string::npos) {
			prefix    = "[SCHEDULER]\t| ";
			colorCode = 32; // Green
		} else if (message.find("IS ACTIVE") != std::string::npos || message.find("DISPATCH LATENCY") != std::string::npos) {
			prefix    = "[CTX SWITCH]\t| ";
			colorCode = 33; // WHITE
		} else if (message.find("SPENT") != std::string::npos) {
			prefix    = "[CPU WORK]\t| ";
			colorCode = 34; // Cyan
		} else if (message.find("BLOCKED") != std::string::npos) {
			prefix    = "[I/O]\t\t| ";
			colorCode = 35; // Yellow
		} else {
			prefix    = "[INFO]\t\t| ";
			colorCode = 37; // White (default)
		}

		std::cout << ColorText(prefix + message, colorCode) << std::endl;
	}
} // namespace

#ifndef NDEBUG
#define REQUIRE(expr)               ((expr) ? (void(0)) : (PanicExit(#expr, std::source_location::current())))
#define REQUIRE_MSG(expr, fmt, ...) ((expr) ? (void(0)) : (PanicExit(fmt::format((fmt), __VA_ARGS__), std::source_location::current())))

#define CHECK(e)                  ((e) ? (void(0)) : (PanicMsg(#e)))
#define CHECK_MSG(expr, fmt, ...) ((expr) ? (void(0)) : (PanicMsg(fmt::format((fmt), __VA_ARGS__), std::source_location::current())))
#else
#define REQUIRE(e)               ((void)0)
#define REQUIRE_MSG(e, fmt, ...) ((void)0)

#define CHECK(e)               ((void)0)
#define CHECK_MSG(e, fmt, ...) ((void)0)
#endif

#endif