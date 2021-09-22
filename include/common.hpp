#ifndef FUTEX_COMMON_HPP_
#define FUTEX_COMMON_HPP_

#include <immintrin.h>

#include <cstring>
#include <stdexcept>
#include <atomic>

#include <boost/throw_exception.hpp>
#include <boost/system/system_error.hpp>
#include <boost/exception/info.hpp>
#include <boost/exception/enable_error_info.hpp>
#include <boost/exception/diagnostic_information.hpp>

//! Exception info wrapper
template< typename ExceptionType >
void throw_exception(const char* descr, const char* func, const char* file, std::uint32_t line)
{
	throw boost::enable_error_info(ExceptionType(descr)) << boost::throw_file(file) << boost::throw_line(line) << boost::throw_function(func);
}

//! Throws an exception macro
#if !defined(THROW_EXCEPTION)
#define THROW_EXCEPTION(e, descr)\
	throw_exception< e >((descr), __func__, __FILE__, __LINE__)
#else
#	error THROW_EXCEPTION macro was defined...
#endif


//! Spinlock pause function
//! https://github.com/ifduyue/musl/blob/master/src/thread/pthread_spin_lock.c
//! NOTE: very strange spinlock realisation via atomic CAS of temporary variable
inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__)) spinlock_pause()
{
#if defined(USE_CAS_SPINLOCK)
	std::atomic< std::uint32_t > tmp{0}; std::uint32_t tmp_ = 0;
	std::atomic_compare_exchange_strong(&tmp, &tmp_, 0u);
#elif defined(__linux__)
	_mm_pause();
#else
#endif
}


//! On one-processor machine spinlock is dummy code
inline bool __attribute__((__gnu_inline__, __always_inline__, __artificial__)) need_spinlock()
{
	long cores = ::sysconf(_SC_NPROCESSORS_ONLN);
	//! On one-processor machine spinlock is dummy code
	if (BOOST_UNLIKELY(cores == 1l))
		return false;
	return true;
}


//! The exception is used to indicate errors in futex syscall
class futex_base_exception : public std::runtime_error
{
public:
	futex_base_exception() : std::runtime_error("Runtime futex exception") {}
	explicit futex_base_exception(const char* descr) : std::runtime_error(descr) {}
	~futex_base_exception() noexcept = default;
};


//! The exception is used to indicate errors in futex lock/unlock
class futex_scoped_error : public std::logic_error
{
public:
	futex_scoped_error() : std::logic_error("Error in futex lock/unlock") {}
	explicit futex_scoped_error(const char* descr) : std::logic_error(descr) {}
	~futex_scoped_error() noexcept = default;
};


//! Access policy for futex-based synchronization primitives
enum class shared_policy
{
	inprocess		= 0x1u,
	interprocess	= 0x2u
};

#endif
