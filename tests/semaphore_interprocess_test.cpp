#include <thread>
#include <cmath>

#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <gtest/gtest.h>

#include "../include/futex_semaphore.hpp"

using namespace boost::interprocess;
TEST(binary_semaphore_interprocess, simple) {
	std::this_thread::sleep_for(std::chrono::seconds(5));
	std::cout << "=======interprocess binary semaphore simple test========\n";
	int forkstatus = ::fork();
	if (forkstatus < 0)
		throw std::runtime_error("error in fork syscall");

	struct binary_semaphore_shared_memory_buffer
	{
		//Semaphore
		binary_semaphore< shared_policy::interprocess > sem;
	};

	//Child process
	if (forkstatus == 0)
	{
		//Remove shared memory on destruction
		struct shm_remove
		{
			~shm_remove(){ shared_memory_object::remove("semaphore-test"); std::cout << "Dctor shmem child\n"; }
		} remover;
		(void)remover;

		shared_memory_object shm(open_only ,"semaphore-test", read_write);
		mapped_region region(shm, read_write);
		void* addr = region.get_address();
		binary_semaphore_shared_memory_buffer* data = static_cast< binary_semaphore_shared_memory_buffer* >(addr);

		//lock shared semaphore
		data->sem.wait();
		std::cout << "Semaphore child wait\n";
		std::this_thread::sleep_for(std::chrono::seconds(3));
		std::cout << "Semaphore child post\n";
		data->sem.post();
		std::cout << "Exit child process\n";
		return;
	}
	else
	{
		//Remove shared memory on construction and destruction
		struct shm_remove
		{
			shm_remove() { shared_memory_object::remove("semaphore-test"); std::cout << "Ctor shmem parent\n"; }
			~shm_remove(){ shared_memory_object::remove("semaphore-test"); std::cout << "Dctor shmem parent\n"; }
		} remover;
		(void)remover;

		shared_memory_object shm(create_only, "semaphore-test", read_write);

		shm.truncate(sizeof(binary_semaphore_shared_memory_buffer));
		mapped_region region(shm, read_write);
		void* addr = region.get_address();
		binary_semaphore_shared_memory_buffer* data = new (addr) binary_semaphore_shared_memory_buffer;

		bool result{false};
		//Wait semaphore post
		if (data->sem.wait_for(std::chrono::seconds(5)))
		{
			std::cout << "Semaphore posted by chidl process\n";
			result = true;
		}
		else
		{
			std::cout << "Timeout on semaphore wait\n";
		}
		std::this_thread::sleep_for(std::chrono::seconds(2));
		EXPECT_EQ(result, true);
		std::cout << "Exit parent process\n";
	}
}
