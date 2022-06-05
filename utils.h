#pragma once
// c common
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
// c++ common
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <memory>
// net
#include <arpa/inet.h>	// ip
#include <sys/socket.h> // tcp/udp
#include <fcntl.h>		// ctrl socket
#include <sys/epoll.h>	// epoll

constexpr int EPOLL_MAX_EVENT = 4096;

enum ChannelEvent_e : int
{
	IN = 0x01,
	OUT = 0x04
};
#define sleep_ms(x) usleep(x * 1000)