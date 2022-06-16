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

#define sleep_ms(x) usleep(x * 1000)
class noncopyable
{
 public:
  noncopyable(const noncopyable&) = delete;
  void operator=(const noncopyable&) = delete;

 protected:
  noncopyable() = default;
  ~noncopyable() = default;
};