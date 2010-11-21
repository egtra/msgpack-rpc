//
// mpio exception
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
#ifndef MP_EXCEPTION_H__
#define MP_EXCEPTION_H__

#include <stdexcept>
#include <string>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

namespace mp {


struct system_error : std::runtime_error {
	static std::string errno_string(int err)
	{
		char buf[512];
#if defined(__linux__)
		char *ret;
		ret = strerror_r(err, buf, sizeof(buf)-1);
		return std::string(ret);
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__APPLE__) || defined(__SunOS__)
		strerror_r(err, buf, sizeof(buf)-1);
		return std::string(buf);
#elif defined(_WIN32)
		FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, err, LANG_USER_DEFAULT, buf, sizeof buf, NULL);
		return std::string(buf);
#else
		return std::string(strerror(err));
#endif
	}

	system_error(int err, const std::string& msg) :
		std::runtime_error(msg + ": " + errno_string(err)) {}
};


struct event_error : system_error {
	event_error(int err, const std::string& msg) :
		system_error(errno, msg) {}
};

#ifdef _WIN32

struct timespec {
	time_t tv_sec;
	long tv_nsec;
};

inline DWORD timespec_to_ms(const struct timespec *ts)
{
	return (DWORD)ts->tv_sec + ts->tv_nsec / 1000000 + (ts->tv_nsec % 1000000 > 500000);
}

#endif


}  // namespace mp

#endif /* mp/exception.h */

