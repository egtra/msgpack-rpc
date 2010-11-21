//
// mpio wavy timer
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
#ifndef DISABLE_TIMERFD
#ifndef WAVY_TIMER_H__
#define WAVY_TIMER_H__

#include "wavy_loop.h"
#include <time.h>

namespace mp {
namespace wavy {

class timer {
public:
	timer(HANDLE hiocp, double value_sec, double interval_sec,
			mp::function<bool ()> callback)
			: htimer(::CreateWaitableTimer(nullptr, FALSE, nullptr)), hiocp(hiocp), callback(callback)
	{
		assert(htimer != nullptr);

		LARGE_INTEGER value;
		value.QuadPart = static_cast<long long>(value_sec * -10000000);
		BOOL res = ::SetWaitableTimer(htimer.get(), &value, static_cast<long>(interval_sec * 1000), nullptr, nullptr, FALSE);
		assert(res != 0);

		HANDLE hw;
		res = ::RegisterWaitForSingleObject(&hw, htimer.get(), on_timer_entry, this, INFINITE, WT_EXECUTEDEFAULT);
		assert(res);
		hwait.reset(hw);
	}

private:
	static void CALLBACK on_timer_entry(void* parameter, BOOLEAN)
	{
		auto pthis = static_cast<timer*>(parameter);
		::PostQueuedCompletionStatus(pthis->hiocp, 0, 0, new overlapped_callback(mp::bind(&timer::on_timer, pthis)));
	}

	void on_timer()
	{
		if (!callback())
		{
			::CancelWaitableTimer(htimer.get());
		}
	}

	unique_handle htimer;
	unique_wait_handle hwait;
	HANDLE hiocp;
	mp::function<bool ()> callback;

	timer(const timer&);
	timer& operator =(const timer&);
};

namespace {
//struct kernel_timer {
//	kernel_timer(kernel& kern, const timespec* value, const timespec* interval)
//	{
//		if(kern.add_timer(&m_timer, value, interval) < 0) {
//			throw system_error(errno, "failed to create timer event");
//		}
//	}
//
//	~kernel_timer() { }
//
//protected:
//	int timer_ident() const
//	{
//		return m_timer.ident();
//	}
//
//	int read_timer(event& e)
//	{
//		return kernel::read_timer( static_cast<event_impl&>(e).get_kernel_event() );
//	}
//
//private:
//	kernel::timer m_timer;
//
//private:
//	kernel_timer();
//	kernel_timer(const kernel_timer&);
//};
//
//
//class timer_handler : public kernel_timer, public basic_handler {
//public:
//	timer_handler(kernel& kern, const timespec* value, const timespec* interval,
//			function<bool ()> callback) :
//		kernel_timer(kern, value, interval),
//		basic_handler(timer_ident(), this),
//		m_periodic(interval && (interval->tv_sec != 0 || interval->tv_nsec != 0)),
//		m_callback(callback)
//	{ }
//
//	~timer_handler() { }
//
//	bool operator() (event& e)
//	{
//		read_timer(e);
//		return m_callback() && m_periodic;
//	}
//
//private:
//	bool m_periodic;
//	function<bool ()> m_callback;
//};


}  // noname namespace
}  // namespace wavy
}  // namespace mp

#endif /* wavy_timer.h */
#endif

