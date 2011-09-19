#pragma once

// Windows NT 4.0 SP3
#define WINVER 0x0403
#define _WIN32_WINNT 0x0403

#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>

#undef ERROR // Region flag constant defined in <wingdi.h>
