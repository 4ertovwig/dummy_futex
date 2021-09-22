#include <thread>
#include <iostream>
#include <chrono>
#include <cmath>

#include <gtest/gtest.h>
#include "../include/futex_condition_variable.hpp"

TEST(condition_variable_inprocess, notify_test_once) {
	std::cout << "==========futex condition variable test notify once=======\n";
	futex_mutex< shared_policy::inprocess > mutex;
	futex_condition_variable< shared_policy::inprocess > cond;

	std::atomic_bool flag {false};
	int increment_variable = 0;
	auto prod = [&]()
	{
		std::this_thread::sleep_for(std::chrono::seconds(1u));
		std::cout << "fake producer notify once..." << "\n";
		flag = true;
		cond.notify_one();
		std::this_thread::sleep_for(std::chrono::milliseconds(100u));
		flag = false;
	};
	auto cons = [&](int id)
	{
		futex_mutex_unique_lock< decltype(mutex) > lock(mutex);
		if (cond.wait_for(lock, std::chrono::seconds(5), [&flag](){ return flag == true; }))
		{
			std::cout << "notified... id: " << id << "\n";
			increment_variable++;
		}
		else
		{
			std::cout << "timeout... id: " << id << "\n";
		}
	};
	std::thread producer(prod);
	std::array< std::thread, 10 > threads;
	for (auto i = 0u; i < 10; ++i)
		threads[i] = std::thread(cons, i);
	for (auto&& thread : threads)
		thread.join();
	producer.join();
	EXPECT_EQ(increment_variable, 1);
}

TEST(condition_variable_inprocess, notify_test_loop) {
	std::cout << "==========futex condition variable test notify loop=======\n";
	futex_mutex< shared_policy::inprocess > mutex;
	futex_condition_variable< shared_policy::inprocess > cond;

	std::atomic_bool flag {false};
	int increment_variable = 0;
	auto prod = [&]()
	{
		int iterations = 10;
		while (iterations--)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1u));
			std::cout << "fake producer notify once..." << "\n";
			flag = true;
			cond.notify_one();
			std::this_thread::sleep_for(std::chrono::milliseconds(100u));
			flag = false;
		}
	};
	auto cons = [&](int id)
	{
		futex_mutex_unique_lock< decltype(mutex) > lock(mutex);
		if (cond.wait_for(lock, std::chrono::seconds(120), [&flag](){ return flag == true; }))
		{
			std::cout << "notified... id: " << id << "\n";
			increment_variable++;
		}
		else
		{
			std::cout << "timeout... id: " << id << "\n";
		}
	};
	std::thread producer(prod);
	std::array< std::thread, 10 > threads;
	for (auto i = 0u; i < 10; ++i)
		threads[i] = std::thread(cons, i);
	for (auto&& thread : threads)
		thread.join();
	producer.join();
	EXPECT_EQ(increment_variable, 10);
}

TEST(condition_variable_inprocess, notifyall_test) {
	std::cout << "==========futex condition variable test notify_all=======\n";
	futex_mutex< shared_policy::inprocess > mutex;
	futex_condition_variable< shared_policy::inprocess > cond;

	std::atomic_bool flag {false};
	int increment_variable = 0;
	auto prod = [&]()
	{
		std::this_thread::sleep_for(std::chrono::seconds(1u));
		std::cout << "fake producer notify all..." << "\n";
		flag = true;
		cond.notify_all();
	};
	auto cons = [&](int id)
	{
		futex_mutex_unique_lock< decltype(mutex) > lock(mutex);
		if (cond.wait_for(lock, std::chrono::seconds(5), [&flag](){ return flag == true; }))
		{
			std::cout << "notified... id: " << id << "\n";
			increment_variable++;
		}
		else
		{
			std::cout << "timeout... id: " << id << "\n";
		}
	};
	std::thread producer(prod);
	std::array< std::thread, 10 > threads;
	for (auto i = 0u; i < 10; ++i)
		threads[i] = std::thread(cons, i);
	for (auto&& thread : threads)
		thread.join();
	producer.join();
	EXPECT_EQ(increment_variable, 10);
}
