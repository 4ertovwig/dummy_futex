#ifndef FUTEX_CONDITION_VARIABLE_HPP_
#define FUTEX_CONDITION_VARIABLE_HPP_

#include <limits.h>
#include <chrono>
#include "futex_mutex.hpp"


//! CV returned status
enum class futex_cv_status
{
	no_timeout,
	timeout
};

//! Common pseudo-code for condition variable:
//! class condition_variable {
//! mutex internal_mutex;            // Internal mutex for lock internal queue and data
//! queue<Thread> waitingThreads;     // Queue of waited threads
//!
//! void wait(mutex external_mutex)
//! {
//!		cvMut.lock();
//!		waitingThreads.push(this_thread);
//!		cvMut.unlock();
//!
//!		external_mutex.unlock(); // Unlock mutex
//!		suspend();               // Suspend
//!		external_mutex.lock();   // lock mutex again
//! }
//!
//! void post()
//! {
//!		internal_mutex.lock();
//!		// get thread from queue
//!		thread = waitingThreads.pop();
//!		internal_mutex.unlock();
//!
//!		if (queue is not empty)
//!			wake_up(thread);
//!	}
//!}
//!
//! Very simple condition_variable implementation via futex syscall.
//! Cancellation semantics is not realized.
//! Used ideas from:
//! sources musl standart c library: https://github.com/ifduyue/musl/blob/master/src/thread/pthread_cond_timedwait.c
//! https://static.lwn.net/images/conf/rtlws11/papers/proc/p10.pdf
//! https://www.remlab.net/op/futex-condvar.shtml
//! NOTE: incorrect exception-safe all functions wait*
//! TODO: except all errors. process SIGCHILD, SIGINTERRUPT in functions wait*
template< shared_policy policy >
class futex_condition_variable : boost::noncopyable
{
	//! Mutex type
	using mutex_t = futex_mutex< policy >;
public:
	//! ctor
	futex_condition_variable()
	: m_futex_val(0u), m_waiters(0u)
	{
		m_wait_op = (policy == shared_policy::inprocess ? FUTEX_WAIT_PRIVATE : FUTEX_WAIT);
		m_wake_op = (policy == shared_policy::inprocess ? FUTEX_WAKE_PRIVATE : FUTEX_WAKE);
		m_req_op = (policy == shared_policy::inprocess ? FUTEX_REQUEUE_PRIVATE : FUTEX_REQUEUE);
	}

	void wait(futex_mutex_unique_lock< mutex_t >& lock)
	{
		std::int32_t val, res;
		// lock internal mutex
		futex_mutex_unique_lock< mutex_t > internal_lock(m_internal_mutex);
		val = m_futex_val;
		++m_waiters;

		// unlock external mutex
		lock.unlock();

		do {
			internal_lock.unlock();
			// NOTE: don't care if futex wakes up spuriously. Because we used external flag
			res = futex(&m_futex_val, m_wait_op, val, nullptr, nullptr, 0);
			internal_lock.lock();
		}
		while (!res || errno == EINTR);

		internal_lock.unlock();

		try
		{
			// lock external mutex
			lock.lock();
			--m_waiters;
		}
		catch (...)
		{
			// Can only throw if internal error. We don't care about m_waiters_count in this case.
			lock.release();
			throw;
		}
	}

	template< typename Predicate >
	void wait(futex_mutex_unique_lock< mutex_t >& lock, Predicate pred)
	{
		while (!pred())
			wait(lock);
	}

	template< typename Rep, typename Period >
	futex_cv_status wait_for(futex_mutex_unique_lock< mutex_t >& lock, const std::chrono::duration< Rep, Period >& timeout_time)
	{
		std::int32_t val, res;
		std::chrono::system_clock::time_point timepoint{timeout_time};
		auto seconds = std::chrono::time_point_cast< std::chrono::seconds >(timepoint);
		auto nanoseconds = std::chrono::duration_cast< std::chrono::nanoseconds >(timepoint - seconds);
		struct timespec timeout = { static_cast< std::time_t >(seconds.time_since_epoch().count()), nanoseconds.count() };

		// lock internal mutex
		futex_mutex_unique_lock< mutex_t > internal_lock(m_internal_mutex);
		val = m_futex_val;
		++m_waiters;

		// unlock external mutex
		lock.unlock();

		do {
			internal_lock.unlock();
			// NOTE: don't care if futex wakes up spuriously. Because we used external flag
			res = futex(&m_futex_val, m_wait_op, val, &timeout, nullptr, 0);
			internal_lock.lock();
		}
		while (res != -1 || errno == EINTR);
		res = errno;
		internal_lock.unlock();

		try
		{
			// lock external mutex
			lock.lock();
			--m_waiters;
			return res == ETIMEDOUT ? futex_cv_status::timeout : futex_cv_status::no_timeout;
		}
		catch (...)
		{
			// Can only throw if internal error. We don't care about m_waiters_count in this case.
			lock.release();
			throw;
		}
	}

	template< typename Rep, typename Period, typename Predicate >
	bool wait_for(futex_mutex_unique_lock< mutex_t >& lock, const std::chrono::duration< Rep, Period >& timeout_time, Predicate pred)
	{
		while (!pred())
		{
			if (wait_for(lock, timeout_time) == futex_cv_status::timeout)
			{ return pred(); }
		}
		return true;
	}

	void notify_one() noexcept
	{
		// lock/unlock internal data
		{
			futex_mutex_lock_guard< mutex_t > lock(m_internal_mutex);
			// avoid extra futex syscall
			if (m_waiters <= 0)
				return;
			++m_futex_val;
		}

		futex(&m_futex_val, m_wake_op, 1, nullptr, nullptr, 0);
	}


	void notify_all() noexcept
	{
		// TODO: rewrite to futex_requeue semantics
		// FUTEX_REQUEUE:
		// if m_futex_val != 0:
		//     return EAGAIN
		// else
		//     requeue waiters to mutex's futex;
		//
		// ++m_futex_val;
		// futex(&m_futex_val, m_req_op, 1, INT_MAX, &external_mutex_futex_addr, 1);

		// lock/unlock internal data
		{
			futex_mutex_lock_guard< mutex_t > lock(m_internal_mutex);
			// avoid extra futex syscall
			if (m_waiters <= 0)
				return;
			++m_futex_val;
		}

		futex(&m_futex_val, m_wake_op, INT_MAX, nullptr, nullptr, 0);
	}

private:
	//! Base wrapper for futex syscall
	static int futex(void* uaddr, int futex_op, int val, const struct timespec* timeout, int* uaddr2, int val3) noexcept
	{
		return ::syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
	}

	//! Mutex for internal synchronization
	mutex_t m_internal_mutex;
	//! Futex value
	std::uint32_t m_futex_val;
	//! Waiters count
	std::uint32_t m_waiters;
	//! Futex options
	int m_wait_op;
	int m_wake_op;
	int m_req_op;
};

#endif
