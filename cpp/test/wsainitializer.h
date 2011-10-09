#include <winsock2.h>
#include <Windows.h>
#pragma once

class WSAInitializer
{
public:
	WSAInitializer()
	{
		m_error = static_cast<DWORD>(::WSAStartup(MAKEWORD(2, 2), &m_wd));
	}

	~WSAInitializer()
	{
		::WSACleanup();
	}

private:
	WSADATA m_wd;
	DWORD m_error;

private:
	WSAInitializer(const WSAInitializer&); // = delete;
	WSAInitializer& operator=(const WSAInitializer&); // = delete;
};

WSAInitializer WsaInit;
