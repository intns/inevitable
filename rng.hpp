#ifndef _RNG_HPP
#define _RNG_HPP

#include <random>
#include "util.hpp"

namespace rng {
	using RandomIntRange   = std::pair<std::int32_t /* Minimum */, std::int32_t /* Maximum */>;
	using RandomFloatRange = std::pair<float_t /* Minimum */, float_t /* Maximum */>;

	std::default_random_engine& GetRandomEngine();
	std::int32_t GetUniformRandomNumber(RandomIntRange bounds);
	float_t GetUniformRandomNumber(RandomFloatRange bounds);
	std::int32_t GetLogRandomNumber(rng::RandomIntRange bounds);
	float_t GetLogRandomNumber(RandomFloatRange bounds);
} // namespace rng

inline std::default_random_engine& rng::GetRandomEngine()
{
	static std::default_random_engine re(std::random_device {}());
	return re;
}

inline std::int32_t rng::GetUniformRandomNumber(rng::RandomIntRange bounds)
{
	REQUIRE(bounds.first < bounds.second);

	return std::uniform_int_distribution<std::int32_t>(bounds.first, bounds.second)(GetRandomEngine());
}

inline float_t rng::GetUniformRandomNumber(RandomFloatRange bounds)
{
	REQUIRE(bounds.first < bounds.second);

	return std::uniform_real_distribution<float_t>(bounds.first, bounds.second)(GetRandomEngine());
}

inline std::int32_t rng::GetLogRandomNumber(rng::RandomIntRange bounds)
{
	REQUIRE(bounds.first > 0);
	REQUIRE(bounds.second > bounds.first);

	// Uniformly sample in log space
	std::uniform_real_distribution<float_t> d(std::log(static_cast<float_t>(bounds.first)), std::log(static_cast<float_t>(bounds.second)));

	// Map back to linear space (exponentiate) and round
	return static_cast<std::int32_t>(std::exp(d(GetRandomEngine())) + 0.5f);
}

inline float_t rng::GetLogRandomNumber(RandomFloatRange bounds)
{
	REQUIRE(bounds.first > 0);
	REQUIRE(bounds.second > bounds.first);

	// Uniformly sample in log space
	std::uniform_real_distribution<float_t> d(std::log(bounds.first), std::log(bounds.second));

	// Map back to linear space (exponentiate) and round
	return static_cast<float_t>(std::exp(d(GetRandomEngine())));
}

/*
    inline void TestRandomness() // unused
    {
#ifndef NDEBUG
        // Run some unit tests to ensure randomness works
        std::cout << "[rng]" << std::endl;

        constexpr std::uint32_t testCount = 10;

        std::cout << "-- UNIFORM INT [0 - 100] --" << std::endl;
        for (std::size_t i = 0; i < testCount; i++) {
            std::int32_t x = rng::GetUniformRandomNumber(rng::RandomIntRange(0, 100));
            CHECK(x >= 0 && x <= 100);
            std::cout << x << ' ';
        }
        std::cout << std::endl;

        std::cout << "-- UNIFORM FLOAT [0 - 100] --" << std::endl;
        for (std::size_t i = 0; i < testCount; i++) {
            float_t x = rng::GetUniformRandomNumber(rng::RandomFloatRange(0.0f, 100.0f));
            CHECK(x >= 0.0f && x <= 100.0f);

            std::cout << x << ' ';
        }
        std::cout << std::endl;

        std::cout << "-- LOG INT [0 - 100] --" << std::endl;
        for (std::size_t i = 0; i < testCount; i++) {
            std::int32_t x = rng::GetLogRandomNumber(rng::RandomIntRange(1, 100));
            CHECK(x >= 0 && x <= 100);
            std::cout << x << ' ';
        }
        std::cout << std::endl;

        std::cout << "-- LOG FLOAT [0 - 100] --" << std::endl;
        for (std::size_t i = 0; i < testCount; i++) {
            float_t x = rng::GetLogRandomNumber(rng::RandomFloatRange(0.01f, 100.0f));
            CHECK(x >= 0.0f && x <= 100.0f);
            std::cout << x << ' ';
        }
        std::cout << std::endl;
#endif
    }
*/

#endif
