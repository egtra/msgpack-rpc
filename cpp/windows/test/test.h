#include <cstdint>
#pragma once

const char SERVER_NAME[] = "192.168.56.205";
const std::uint16_t SERVER_PORT = 9191;

void async_call_0();
void async_server();
void sync_call();
void callback();
void error();
void notify();
void zone();
void udp();
