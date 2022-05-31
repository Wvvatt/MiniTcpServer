#pragma once
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
enum ChannelEvent_e : int
{
	IN = 0x01,
	OUT = 0x04
};
#define sleep_ms(x) usleep(x * 1000)