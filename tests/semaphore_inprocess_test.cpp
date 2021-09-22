#include <thread>
#include <iostream>
#include <chrono>
#include <cmath>

#include <gtest/gtest.h>
#include "../include/futex_semaphore.hpp"

TEST(binary_semaphore_inprocess, wait_for) {
	std::cout << "==========binary semaphore inprocess test=======\n";
	binary_semaphore< shared_policy::inprocess > sem;
	bool notified_flag{false};
	std::thread producer([&](){
		sem.wait();
		std::this_thread::sleep_for(std::chrono::seconds(2));
		std::cout << "Notify semaphore\n";
		sem.post();
	});
	std::thread consumer([&](){
		std::this_thread::sleep_for(std::chrono::seconds(1));
		if (sem.wait_for(std::chrono::seconds(4)))
		{
			std::cout << "Norm...\n";
			notified_flag = true;
		}
		else
		{
			std::cout << "Bad...\n";
		}
	});
	producer.join(); consumer.join();
	EXPECT_EQ(notified_flag, true);
}

//4 simultaneous worker threads, 1 - waited
TEST(semaphore_inprocess, wait_for) {
	std::cout << "==========semaphore inprocess test for multiple waiters=======\n";
	futex_semaphore< shared_policy::inprocess > sem{4};
	std::uint32_t locked{0}, timeouts{0};
	auto locker = [&sem, &locked, &timeouts](int id)
	{
		if (sem.wait_for(std::chrono::seconds(5)))
		{
			std::cout << "lock id: " << id << "\n";
			locked++;
		}
		else
		{
			std::cout << "timeout id: " << id << "\n";
			timeouts++;
		}
		std::this_thread::sleep_for(std::chrono::seconds(3u));
		sem.post();
	};
	std::array< std::thread, 10 > threads;
	for (int i = 0; i < 10; ++i)
		threads[i] = std::thread(locker, i + 1);
	for (auto&& thrd : threads)
		thrd.join();
	EXPECT_EQ(locked, 8);
	EXPECT_EQ(timeouts, 2);
}
