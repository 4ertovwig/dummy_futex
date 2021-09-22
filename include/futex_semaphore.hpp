#ifndef FUTEX_SEMAPHORE_HPP_
#define FUTEX_SEMAPHORE_HPP_

#include "futex_mutex.hpp"
#include "futex_condition_variable.hpp"

//! Simple and naive semaphore realization with own mutex and condition variable
//! Semantics are similar to <semaphore.h>
template< shared_policy policy >
class futex_semaphore : boost::noncopyable
{
	//! Internal types
	using mutex_t = futex_mutex< policy >;
	using condition_t = futex_condition_variable< policy >;
public:
	explicit futex_semaphore(std::int32_t maximum_simultaneous_workers) : m_limit(maximum_simultaneous_workers), m_waiters(0u)
	{
		if (m_limit <= 0u)
			THROW_EXCEPTION(std::runtime_error, "Maximum waiters must be greater than zero");
	}

	void wait()
	{
		futex_mutex_unique_lock< mutex_t > lk(m_mutex);
		++m_waiters;
		m_cond.wait(lk, [&](){ return m_waiters.load() <= m_limit; });
	}

	template< typename Rep, typename Period >
	bool wait_for(const std::chrono::duration< Rep, Period >& waited_time)
	{
		futex_mutex_unique_lock< mutex_t > lk(m_mutex);
		++m_waiters;
		if (!m_cond.wait_for(lk, waited_time, [&](){ return m_waiters.load() <= m_limit; }))
		{
			--m_waiters;
			return false;
		}

		return true;
	}

	void post()
	{
		futex_mutex_lock_guard< decltype(m_mutex) > lk(m_mutex);
		if (m_waiters.load() == 0u)
			return;

		--m_waiters;
		m_cond.notify_one();
	}

private:
	const std::uint32_t m_limit;
	std::atomic< std::int32_t > m_waiters;
	//! Synchronized mutex
	mutex_t m_mutex;
	//! Condition variable
	condition_t m_cond;
};

//! Mutex 'analog' with 1 maximum worker
template< shared_policy policy >
class binary_semaphore : public futex_semaphore< policy >
{
public:
	binary_semaphore() : futex_semaphore< policy >(1u) {}
};

#endif
