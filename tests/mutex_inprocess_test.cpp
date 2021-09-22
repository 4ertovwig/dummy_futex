#include <thread>
#include <iostream>
#include <chrono>
#include <cmath>

#include <gtest/gtest.h>
#include "../include/futex_mutex.hpp"

// increment shared variable in 256 thread simultaneously
TEST(mutex_inprocess, with_spinlock) {
	std::cout << "==========futex mutex with spinlock test with increment=======\n";
	auto start = std::chrono::steady_clock::now();
	futex_mutex< shared_policy::inprocess, true > mutex;
	double a = 0;
	const std::uint32_t max = 10000u;
	auto writer = [&mutex, &a, max](int id) {
		int cc{0u};
		while (cc++ < max)
		{
			futex_mutex_lock_guard< decltype(mutex) > lock(mutex);
			++a;
		}
	};
	std::array< std::thread, 256 > threads;
	for (auto i = 0u; i < 256; ++i)
		threads[i] = std::thread(writer, i);
	for (auto&& thread : threads)
		thread.join();
	auto finish = std::chrono::steady_clock::now();
	std::cout << "Result a: " << a << " test time: " << std::chrono::duration_cast< std::chrono::milliseconds >(start - finish).count() << " ms" <<  std::endl;
	//result: 2560000
	GTEST_CHECK_(std::fabs(a - 256 * max) < std::numeric_limits< double >::epsilon());
}


TEST(mutex_inprocess, without_spinlock) {
	std::cout << "==========futex mutex with spinlock test with increment=======\n";
	auto start = std::chrono::steady_clock::now();
	futex_mutex< shared_policy::inprocess, false > mutex;
	double a = 0;
	const std::uint32_t max = 10000u;
	auto writer = [&mutex, &a, max](int id) {
		int cc{0u};
		while (cc++ < max)
		{
			futex_mutex_lock_guard< decltype(mutex) > lock(mutex);
			++a;
		}
	};
	std::array< std::thread, 256 > threads;
	for (auto i = 0u; i < 256; ++i)
		threads[i] = std::thread(writer, i);
	for (auto&& thread : threads)
		thread.join();
	auto finish = std::chrono::steady_clock::now();
	std::cout << "Result a: " << a << " test time: " << std::chrono::duration_cast< std::chrono::milliseconds >(start - finish).count() << " ms" <<  std::endl;
	//result: 2560000
	GTEST_CHECK_(std::fabs(a - 256 * max) < std::numeric_limits< double >::epsilon());
}
