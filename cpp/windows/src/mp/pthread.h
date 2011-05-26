//
// mpio pthread
//
// Copyright (C) 2008-2010 FURUHASHI Sadayuki
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
#ifndef MP_PTHREAD_H__
#define MP_PTHREAD_H__

#include "mp/exception.h"
#include "mp/functional.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <pthread.h>
#endif
#include <memory>

namespace mp {


struct pthread_error : system_error {
	pthread_error(int errno_, const std::string& msg) :
		system_error(errno_, msg) {}
};


struct pthread_thread {
private:
	typedef function<void ()> function_t;

public:
	void create(DWORD (WINAPI *func)(void*), void* user)
	{
		DWORD threadId;
		m_thread = CreateThread(NULL, 0, func, user, 0, &threadId);
		if(m_thread == NULL) { throw pthread_error(GetLastError(), "failed to create thread"); }
	}

	void run(function_t func)
	{
		std::auto_ptr<function_t> f(new function_t(func));
		create(&trampoline, reinterpret_cast<void*>(f.get()));
		f.release();
	}

	void detach()
	{
		BOOL ret = CloseHandle(m_thread);
		if(!ret) { throw pthread_error(GetLastError(), "failed to detach thread"); }
	}

//	void* join()
	void join()
	{
		DWORD ret = WaitForSingleObject(m_thread, INFINITE);
		if(ret != WAIT_OBJECT_0) { throw pthread_error(GetLastError(), "failed to join thread"); }
		//return ret;
	}

	//void cancel()
	//{
	//	pthread_cancel(m_thread);
	//}


	//bool operator== (const pthread_thread& other) const
	//{
	//	return m_thread == other.m_thread;
	//}

	//bool operator!= (const pthread_thread& other) const
	//{
	//	return !(*this == other);
	//}

	static void exit(void* retval = NULL)
	{
		ExitThread(0);
	}

private:
	HANDLE m_thread;

	static DWORD WINAPI trampoline(void* user);
};


template <typename IMPL>
struct pthread_thread_impl : public pthread_thread {
	pthread_thread_impl() : pthread_thread(this) { }
	virtual ~pthread_thread_impl() { }
};


class pthread_mutex {
public:
	//pthread_mutex(const pthread_mutexattr_t *attr = NULL)
	//{
	//	pthread_mutex_init(&m_mutex, attr);
	//}

	pthread_mutex()
	{
		BOOL ret = InitializeCriticalSectionAndSpinCount(&m_mutex, 4000);
	}
#ifndef BOOST_NO_RVALUE_REFERENCES
	pthread_mutex(pthread_mutex&& y)
	{
		m_mutex = y.m_mutex;
		y.m_mutex = CRITICAL_SECTION();
	}
#endif
	//pthread_mutex(int kind)
	//{
	//	pthread_mutexattr_t attr;
	//	pthread_mutexattr_init(&attr);
	//	pthread_mutexattr_settype(&attr, kind);
	//	pthread_mutex_init(&m_mutex, &attr);
	//}

	~pthread_mutex()
	{
		DeleteCriticalSection(&m_mutex);
	}

public:
	void lock()
	{
		EnterCriticalSection(&m_mutex);
		//if(err != 0) { throw pthread_error(-err, "failed to lock pthread mutex"); }
	}

	bool trylock()
	{
		return !!TryEnterCriticalSection(&m_mutex);
		//int err = pthread_mutex_trylock(&m_mutex);
		//if(err != 0) {
		//	if(err == EBUSY) { return false; }
		//	throw pthread_error(-err, "failed to trylock pthread mutex");
		//}
		//return true;
	}

	void unlock()
	{
		LeaveCriticalSection(&m_mutex);
		//if(err != 0) { throw pthread_error(-err, "failed to unlock pthread mutex"); }
	}

public:
	CRITICAL_SECTION* get() { return &m_mutex; }
private:
	CRITICAL_SECTION m_mutex;
private:
	pthread_mutex(const pthread_mutex&);
};


//class pthread_rwlock {
//public:
//	pthread_rwlock(const pthread_rwlockattr_t *attr = NULL)
//	{
//		pthread_rwlock_init(&m_mutex, attr);
//	}
//
//	// FIXME kind
//	//pthread_rwlock(int kind)
//	//{
//	//	pthread_rwlockattr_t attr;
//	//	pthread_rwlockattr_init(&attr);
//	//	pthread_rwlockattr_settype(&attr, kind);
//	//	pthread_rwlock_init(&m_mutex, &attr);
//	//}
//
//	~pthread_rwlock()
//	{
//		pthread_rwlock_destroy(&m_mutex);
//	}
//
//public:
//	void rdlock()
//	{
//		int err = pthread_rwlock_rdlock(&m_mutex);
//		if(err != 0) { throw pthread_error(-err, "failed to read lock pthread rwlock"); }
//	}
//
//	bool tryrdlock()
//	{
//		int err = pthread_rwlock_tryrdlock(&m_mutex);
//		if(err != 0) {
//			if(err == EBUSY) { return false; }
//			throw pthread_error(-err, "failed to read trylock pthread rwlock");
//		}
//		return true;
//	}
//
//	void wrlock()
//	{
//		int err = pthread_rwlock_wrlock(&m_mutex);
//		if(err != 0) { throw pthread_error(-err, "failed to write lock pthread rwlock"); }
//	}
//
//	bool trywrlock()
//	{
//		int err = pthread_rwlock_trywrlock(&m_mutex);
//		if(err != 0) {
//			if(err == EBUSY) { return false; }
//			throw pthread_error(-err, "failed to write trylock pthread rwlock");
//		}
//		return true;
//	}
//
//	void unlock()
//	{
//		int err = pthread_rwlock_unlock(&m_mutex);
//		if(err != 0) { throw pthread_error(-err, "failed to unlock pthread rwlock"); }
//	}
//
//public:
//	pthread_rwlock_t* get() { return &m_mutex; }
//private:
//	pthread_rwlock_t m_mutex;
//private:
//	pthread_rwlock(const pthread_rwlock&);
//};


class pthread_scoped_lock {
public:
	pthread_scoped_lock() : m_mutex(NULL) { }

	pthread_scoped_lock(pthread_mutex& mutex) : m_mutex(NULL)
	{
		mutex.lock();
		m_mutex = &mutex;
	}

	~pthread_scoped_lock()
	{
		if(m_mutex) {
			m_mutex->unlock();
		}
	}

public:
	void unlock()
	{
		if(m_mutex) {
			m_mutex->unlock();
			m_mutex = NULL;
		}
	}

	void relock(pthread_mutex& mutex)
	{
		unlock();
		mutex.lock();
		m_mutex = &mutex;
	}

	bool owns() const
	{
		return m_mutex != NULL;
	}

private:
	pthread_mutex* m_mutex;
private:
	pthread_scoped_lock(const pthread_scoped_lock&);
};


//class pthread_scoped_rdlock {
//public:
//	pthread_scoped_rdlock() : m_mutex(NULL) { }
//
//	pthread_scoped_rdlock(pthread_rwlock& mutex) : m_mutex(NULL)
//	{
//		mutex.rdlock();
//		m_mutex = &mutex;
//	}
//
//	~pthread_scoped_rdlock()
//	{
//		if(m_mutex) {
//			m_mutex->unlock();
//		}
//	}
//
//public:
//	void unlock()
//	{
//		if(m_mutex) {
//			m_mutex->unlock();
//			m_mutex = NULL;
//		}
//	}
//
//	void relock(pthread_rwlock& mutex)
//	{
//		unlock();
//		mutex.rdlock();
//		m_mutex = &mutex;
//	}
//
//	bool owns() const
//	{
//		return m_mutex != NULL;
//	}
//
//private:
//	pthread_rwlock* m_mutex;
//private:
//	pthread_scoped_rdlock(const pthread_scoped_rdlock&);
//};


//class pthread_scoped_wrlock {
//public:
//	pthread_scoped_wrlock() : m_mutex(NULL) { }
//
//	pthread_scoped_wrlock(pthread_rwlock& mutex) : m_mutex(NULL)
//	{
//		mutex.wrlock();
//		m_mutex = &mutex;
//	}
//
//	~pthread_scoped_wrlock()
//	{
//		if(m_mutex) {
//			m_mutex->unlock();
//		}
//	}
//
//public:
//	void unlock()
//	{
//		if(m_mutex) {
//			m_mutex->unlock();
//			m_mutex = NULL;
//		}
//	}
//
//	void relock(pthread_rwlock& mutex)
//	{
//		unlock();
//		mutex.wrlock();
//		m_mutex = &mutex;
//	}
//
//	bool owns() const
//	{
//		return m_mutex != NULL;
//	}
//
//private:
//	pthread_rwlock* m_mutex;
//private:
//	pthread_scoped_wrlock(const pthread_scoped_wrlock&);
//};


class pthread_cond {
public:
	pthread_cond()
	{
		InitializeConditionVariable(&m_cond);
	}

	~pthread_cond()
	{
	}

public:
	void signal()
	{
		WakeConditionVariable(&m_cond);
	}

	void broadcast()
	{
		WakeAllConditionVariable(&m_cond);
	}

	void wait(pthread_mutex& mutex)
	{
		BOOL ret = SleepConditionVariableCS(&m_cond, mutex.get(), INFINITE);
		if(ret == 0) { throw pthread_error(GetLastError(), "failed to wait pthread cond"); }
	}

	bool timedwait(pthread_mutex& mutex, const struct timespec *abstime)
	{
		BOOL err = SleepConditionVariableCS(&m_cond, mutex.get(), timespec_to_ms(abstime));
		if(err != 0) {
			DWORD err = GetLastError();
			if(err == WAIT_TIMEOUT) { return false; }
			throw pthread_error(err, "failed to timedwait pthread cond");
		}
		return true;
	}

public:
	CONDITION_VARIABLE* get() { return &m_cond; }
private:
	CONDITION_VARIABLE m_cond;
private:
	pthread_cond(const pthread_cond&);
};


}  // namespace mp


#include <iostream>
#include <typeinfo>
#ifndef MP_NO_CXX_ABI_H
#include <cxxabi.h>
#endif

namespace mp {


inline DWORD WINAPI pthread_thread::trampoline(void* user)
try {
	std::auto_ptr<function_t> f(reinterpret_cast<function_t*>(user));
	(*f)();
	return 0;

} catch (std::exception& e) {
	try {
#ifndef MP_NO_CXX_ABI_H
		int status;
		std::cerr
			<< "thread terminated with throwing an instance of '"
			<< abi::__cxa_demangle(typeid(e).name(), 0, 0, &status)
			<< "'\n"
			<< "  what():  " << e.what() << std::endl;
#else
		std::cerr
			<< "thread terminated with throwing an instance of '"
			<< typeid(e).name()
			<< "'\n"
			<< "  what():  " << e.what() << std::endl;
#endif
	} catch (...) {}
	throw;

} catch (...) {
	try {
		std::cerr << "thread terminated with throwing an unknown object" << std::endl;
	} catch (...) {}
	throw;
}


}  // namespace mp

#endif /* mp/pthread.h */

