#include <thread>
#include <iostream>
#include <chrono>
#include <cmath>

#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <gtest/gtest.h>

#include "../include/futex_mutex.hpp"

TEST(mutex_interprocess, simple_lock) {
	std::cout << "=======interprocess mutex simple lock test========\n";
	int forkstatus = ::fork();
	if (forkstatus < 0)
		throw std::runtime_error("error in fork syscall");

	using namespace boost::interprocess;

	struct mutex_shared_memory_buffer
	{
		//Mutex to protect and synchronize access
		futex_mutex< shared_policy::interprocess > mutex;
		//Shared flag
		bool shared_flag = false;
	};

	//Child process
	if (forkstatus == 0)
	{
		struct shm_remove
		{
			~shm_remove(){ shared_memory_object::remove("mutex-test"); std::cout << "Dctor shmem child\n"; }
		} remover;
		(void)remover;

		shared_memory_object shm(open_only ,"mutex-test", read_write);
		mapped_region region(shm, read_write);
		void* addr = region.get_address();
		mutex_shared_memory_buffer* data = static_cast< mutex_shared_memory_buffer* >(addr);

		//lock shared mutex
		futex_mutex_lock_guard< decltype(mutex_shared_memory_buffer::mutex) > lock(data->mutex);
		std::this_thread::sleep_for(std::chrono::seconds(3));
		data->shared_flag = true;
		std::cout << "Exit child process\n";
		return;
	}
	else
	{
		struct shm_remove
		{
			shm_remove() { shared_memory_object::remove("mutex-test"); std::cout << "Ctor shmem parent\n"; }
			~shm_remove(){ shared_memory_object::remove("mutex-test"); std::cout << "Dctor shmem parent\n"; }
		} remover;
		(void)remover;

		shared_memory_object shm(create_only, "mutex-test", read_write);

		shm.truncate(sizeof(mutex_shared_memory_buffer));
		mapped_region region(shm, read_write);
		void* addr = region.get_address();
		mutex_shared_memory_buffer* data = new (addr) mutex_shared_memory_buffer;

		std::this_thread::sleep_for(std::chrono::seconds(2));
		bool result{false};
		//Wait on shared mutex
		futex_mutex_lock_guard< decltype(mutex_shared_memory_buffer::mutex) > lock(data->mutex);
		std::cout << "Read shared variable\n";
		result = data->shared_flag;

		std::this_thread::sleep_for(std::chrono::seconds(2));
		EXPECT_EQ(result, true);
		std::cout << "Exit parent process\n";
	}
}
