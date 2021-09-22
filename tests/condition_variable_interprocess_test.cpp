#include <thread>
#include <cmath>

#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <gtest/gtest.h>

#include "../include/futex_condition_variable.hpp"

using namespace boost::interprocess;
TEST(condition_variable_interprocess1, notify_one) {
	std::this_thread::sleep_for(std::chrono::seconds(5));
	std::cout << "=======interprocess condition variable test notify========\n";
	int forkstatus = ::fork();
	if (forkstatus < 0)
		throw std::runtime_error("error in fork syscall");

	struct condition_variable_shared_memory_buffer
	{
		//Mutex for lock
		futex_mutex< shared_policy::interprocess > mutex;
		//Condition variable to protect and synchronize access
		futex_condition_variable< shared_policy::interprocess > cond;
		//Signalling flag
		bool signalling_flag = false;
	};

	//Child process
	if (forkstatus == 0)
	{
		//Remove shared memory on destruction
		struct shm_remove
		{
			~shm_remove(){ shared_memory_object::remove("cond-variable-test"); std::cout << "Dctor shmem child\n"; }
		} remover;
		(void)remover;

		shared_memory_object shm(open_only ,"cond-variable-test", read_write);
		mapped_region region(shm, read_write);
		void* addr = region.get_address();
		condition_variable_shared_memory_buffer* data = static_cast< condition_variable_shared_memory_buffer* >(addr);

		//lock shared mutex
		std::this_thread::sleep_for(std::chrono::seconds(3));
		data->signalling_flag = true;
		std::cout << "Notify parent process\n";
		data->cond.notify_one();
		std::cout << "Exit child process\n";
		return;
	}
	else
	{
		//Remove shared memory on construction and destruction
		struct shm_remove
		{
			shm_remove() { shared_memory_object::remove("cond-variable-test"); std::cout << "Ctor shmem parent\n"; }
			~shm_remove(){ shared_memory_object::remove("cond-variable-test"); std::cout << "Dctor shmem parent\n"; }
		} remover;
		(void)remover;

		shared_memory_object shm(create_only, "cond-variable-test", read_write);

		shm.truncate(sizeof(condition_variable_shared_memory_buffer));
		mapped_region region(shm, read_write);
		void* addr = region.get_address();
		condition_variable_shared_memory_buffer* data = new (addr) condition_variable_shared_memory_buffer;

		std::this_thread::sleep_for(std::chrono::seconds(2));
		bool result{false};
		//Wait on condition variable
		futex_mutex_unique_lock< decltype(condition_variable_shared_memory_buffer::mutex) > lock(data->mutex);
		if (data->cond.wait_for(lock, std::chrono::seconds(5), [&]{ return data->signalling_flag;}))
		{
			std::cout << "Condition variable was notified by child process\n";
			result = true;
		}
		else
		{
			std::cout << "Timeout on condition variable\n";
		}
		std::this_thread::sleep_for(std::chrono::seconds(2));
		EXPECT_EQ(result, true);
		std::cout << "Exit parent process\n";
	}
}

TEST(condition_variable_interprocess2, wait_for) {
	std::this_thread::sleep_for(std::chrono::seconds(5));
	std::cout << "=======interprocess condition variable test wait_for========\n";
	int forkstatus = ::fork();
	if (forkstatus < 0)
		throw std::runtime_error("error in fork syscall");

	struct condition_variable_shared_memory_buffer
	{
		//Mutex for lock
		futex_mutex< shared_policy::interprocess > mutex;
		//Condition variable to protect and synchronize access
		futex_condition_variable< shared_policy::interprocess > cond;
		//Signalling flag
		bool signalling_flag = false;
	};

	//Child process
	if (forkstatus == 0)
	{
		//Remove shared memory on destruction
		struct shm_remove
		{
			~shm_remove(){ shared_memory_object::remove("cond-variable-wait-for"); std::cout << "Dctor shmem child\n"; }
		} remover;
		(void)remover;

		shared_memory_object shm(open_only ,"cond-variable-wait-for", read_write);
		mapped_region region(shm, read_write);
		void* addr = region.get_address();
		condition_variable_shared_memory_buffer* data = static_cast< condition_variable_shared_memory_buffer* >(addr);

		//lock shared mutex
		std::this_thread::sleep_for(std::chrono::seconds(3));
		std::cout << "Exit child process\n";
		return;
	}
	else
	{
		//Remove shared memory on construction and destruction
		struct shm_remove
		{
			shm_remove() { shared_memory_object::remove("cond-variable-wait-for"); std::cout << "Ctor shmem parent\n"; }
			~shm_remove(){ shared_memory_object::remove("cond-variable-wait-for"); std::cout << "Dctor shmem parent\n"; }
		} remover;
		(void)remover;

		shared_memory_object shm(create_only, "cond-variable-wait-for", read_write);

		shm.truncate(sizeof(condition_variable_shared_memory_buffer));
		mapped_region region(shm, read_write);
		void* addr = region.get_address();
		condition_variable_shared_memory_buffer* data = new (addr) condition_variable_shared_memory_buffer;

		std::this_thread::sleep_for(std::chrono::seconds(2));
		bool result{false};
		//Wait on condition variable
		futex_mutex_unique_lock< decltype(condition_variable_shared_memory_buffer::mutex) > lock(data->mutex);
		if (data->cond.wait_for(lock, std::chrono::seconds(1), [&]{ return data->signalling_flag;}))
		{
			std::cout << "Condition variable was notified by child process\n";
			result = true;
		}
		else
		{
			std::cout << "Timeout on condition variable\n";
		}
		std::this_thread::sleep_for(std::chrono::seconds(2));
		EXPECT_EQ(result, false);
		std::cout << "Exit parent process\n";
	}
}
