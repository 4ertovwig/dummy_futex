#ifndef FUTEX_MUTEX_HPP_
#define FUTEX_MUTEX_HPP_

#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <linux/futex.h>

#include "common.hpp"

#include <iostream>

//! Simplest mutex realization via futex syscall. Based on:
//! https://preshing.com/20120226/roll-your-own-lightweight-mutex/
//! https://akkadia.org/drepper/futex.pdf
//! http://cbloomrants.blogspot.com/2011/07/07-15-11-review-of-many-mutex.html
//!
//! shared policy: whether a mutex can synchronize different processes or not
//! use_spinlock: whether to use a spinlock at the beginning of a lock.
//! http://www.alexonlinux.com/pthread-mutex-vs-pthread-spinlock
//! NOTE: with a spin loop it got even worse.
template< shared_policy policy, bool use_spinlock = false >
class futex_mutex : boost::noncopyable
{
	//! States of mutex
	enum : std::uint32_t
	{
		unlocked			= 0u,
		locked_no_waiters	= 0x1u,
		locked_has_waiters	= 0x2u
	};

public:
	futex_mutex() : m_state(unlocked)
	{
		if (!m_state.is_lock_free())
			THROW_EXCEPTION(futex_base_exception, "m_state must be lock-free");
		m_wait_op = (policy == shared_policy::inprocess ? FUTEX_WAIT_PRIVATE : FUTEX_WAIT);
		m_wake_op = (policy == shared_policy::inprocess ? FUTEX_WAKE_PRIVATE : FUTEX_WAKE);
	}

	void lock()
	{
		// simple spin loop
		if (use_spinlock && need_spinlock())
		{
			if (spin_loop())
				return;
		}

		std::uint32_t prev{0};
		if (!std::atomic_compare_exchange_strong(&m_state, &prev, (std::uint32_t)locked_no_waiters))
		// NOTE: previous string emulated this CAS semantics:
		//int CAS( int * pAddr, int nExpected, int nNew )
		//atomically {
		//	if ( *pAddr == nExpected ) {
		//		*pAddr = nNew ;
		//		return nExpected ;
		//	}
		//	else
		//		return *pAddr
		//	}
		{
			if (prev != locked_no_waiters)
				prev = std::atomic_exchange(&m_state, (std::uint32_t)locked_has_waiters);

			while (prev != unlocked)
			{
				int res = futex(&m_state, m_wait_op, locked_has_waiters, nullptr, nullptr, 0);
				if (res != 0 && errno != EAGAIN)
					THROW_EXCEPTION(futex_base_exception, std::strerror(errno));

				// now retry
				prev = std::atomic_exchange(&m_state, (std::uint32_t)locked_has_waiters);
			}
		}
	}

	void unlock() noexcept
	{
		std::uint32_t prev;
		// if (atomic_dec (val) != 1)
		if ((prev = std::atomic_fetch_sub(&m_state, 1u)) != locked_no_waiters)
		{
			m_state.store(unlocked);

			// Wake just one thread/process
			futex(&m_state, m_wake_op, 1, nullptr, nullptr, 0);
		}
	}

private:
	//! Mutex current state
	std::atomic< std::uint32_t > m_state;
	//! Futex options
	int m_wait_op;
	int m_wake_op;

	//! Base wrapper for futex syscall
	static int futex(void* uaddr, int futex_op, int val, const struct timespec* timeout, int* uaddr2, int val3) noexcept
	{
		return ::syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
	}

	//! Spin loop
	bool spin_loop() noexcept
	{
		int spin{100};
		while (spin--)
		{
			if (m_state.load() == (std::uint32_t)unlocked)
			{
				std::uint32_t val = (std::uint32_t)unlocked;
				if (std::atomic_compare_exchange_strong(&m_state, &val, (std::uint32_t)locked_no_waiters))
					return true;
			}

			//pause
			spinlock_pause();
		}
		return false;
	}
};


//! Basic RAII locker
template < typename MutexType >
class futex_mutex_lock_guard
{
public:
	explicit futex_mutex_lock_guard(MutexType& mutex) : m_mutex(mutex) { m_mutex.lock(); }
	~futex_mutex_lock_guard() noexcept { m_mutex.unlock(); }
private:
	MutexType& m_mutex;
};


//! Unique lock
//! NOTE: move semantics is not supported
template < typename MutexType >
class futex_mutex_unique_lock
{
public:
	//! ctor
	explicit futex_mutex_unique_lock(MutexType& mutex)
	: m_mutex(std::addressof(mutex))
	, m_owns(false)
	{
		lock();
		m_owns = true;
	}
	//! dctor
	~futex_mutex_unique_lock() noexcept
	{
		if (m_owns)
			unlock();
	}

	void lock()
	{
		if (!m_mutex)
			THROW_EXCEPTION(futex_scoped_error, "Mutex cannot be nullptr");
		else if (m_owns)
			THROW_EXCEPTION(futex_scoped_error, "Mutex already locked");
		else
		{
			m_mutex->lock();
			m_owns = true;
		}
	}

	void unlock()
	{
		if (!m_owns)
		{
			THROW_EXCEPTION(futex_scoped_error, "Mutex must be locked");
		}
		else if (m_mutex)
		{
			m_mutex->unlock();
			m_owns = false;
		}
	}

	MutexType* release() noexcept
	{
		MutexType* ret = m_mutex;
		m_mutex = nullptr;
		m_owns = false;
		return ret;
	}

private:
	MutexType*	m_mutex;
	bool m_owns;
};

#endif
