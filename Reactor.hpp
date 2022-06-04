#pragma once
// c common
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
// net
#include <arpa/inet.h>	// ip
#include <sys/socket.h> // tcp/udp
#include <fcntl.h>		// ctrl socket
#include <sys/epoll.h>	// epoll
// c++ utils
#include <functional>
#include <vector>
#include <memory>
#include <thread>
#include <algorithm>
#include <map>
#include <queue>

#include "utils.h"
#include "Channel.hpp"
#include "WorkerInterface.h"
#include "MiniLog.hpp"

constexpr int MAX_EVENT = 2048;

class Reactor;
using SpReactor = std::shared_ptr<Reactor>;
class Reactor : public WorkerInterface
{
public:
	Reactor();
	~Reactor();
	bool Work() override;
	void Close();
	void AddChannel(SpChannel);
	void DelChannel(SpChannel);
	void ModChannel(SpChannel);

private:
	enum class ActionType_e
	{
		ADD,
		DELETE,
		MODIFY
	};
	struct ChannelAction
	{
		ActionType_e action;
		SpChannel channel;
	};
	void handlePendingChannel();

private:
	std::mutex _pendingMutex;
	std::queue<ChannelAction> _pendingChanlActions;
	int _epoll;
	std::map<int, SpChannel> _channels;
	bool _running;
	void *_owner;
};

Reactor::Reactor() : _epoll(epoll_create(MAX_EVENT))
{
}

Reactor::~Reactor()
{
	Close();
}

void Reactor::Close()
{
	for (auto iter = _channels.begin(); iter != _channels.end(); iter++)
	{
		close(iter->second->_fd);
	}
	_channels.clear();
	close(_epoll);
}

bool Reactor::Work()
{
	epoll_event activeEvts[MAX_EVENT] = {};
	auto activeFds = epoll_wait(_epoll, activeEvts, MAX_EVENT, 1000);
	if (activeFds < 0)
	{
		minilog(LogLevel_e::ERROR, "epoll_wait error, break");
		return false;
	}
	for (int i = 0; i < activeFds; i++)
	{
		int fd = activeEvts[i].data.fd;
		auto chanIter = _channels.find(fd);
		if (chanIter == _channels.end())
		{
			minilog(LogLevel_e::ERROR, "unknown fd(%d) is actived!", fd);
			continue;
		}
		auto spChan = chanIter->second;
		if (spChan->_events & EPOLLIN)
		{
			if (spChan->_onConnect)
			{
				spChan->_onConnect(spChan);
				continue;
			}
			if (spChan->_onRead)
			{
				spChan->_onRead(spChan);
			}
		}
		if (spChan->_events & EPOLLOUT)
		{
			if (spChan->_onSend)
			{
				spChan->_onSend(spChan);
			}
		}
	}
	handlePendingChannel();
	return true;
}

void Reactor::AddChannel(SpChannel chan)
{
	std::lock_guard<std::mutex> lock(_pendingMutex);
	_pendingChanlActions.push({ActionType_e::ADD, chan});
}

void Reactor::DelChannel(SpChannel chan)
{
	std::lock_guard<std::mutex> lock(_pendingMutex);
	_pendingChanlActions.push({ActionType_e::DELETE, chan});
}

void Reactor::ModChannel(SpChannel chan)
{
	std::lock_guard<std::mutex> lock(_pendingMutex);
	_pendingChanlActions.push({ActionType_e::MODIFY, chan});
}

void Reactor::handlePendingChannel()
{
	std::lock_guard<std::mutex> lock(_pendingMutex);
	while (_pendingChanlActions.size() > 0)
	{
		auto chaAct = _pendingChanlActions.front();
		auto channel = chaAct.channel;
		auto action = chaAct.action;
		int fd = channel->_fd;
		if (action == ActionType_e::ADD)
		{
			epoll_event evt = {};
			if (_channels.find(fd) != _channels.end())
			{
				minilog(LogLevel_e::WARRNIG, "channel(fd = %d) has already in epoll(fd = %d), so modify", channel->_fd, _epoll);
				_pendingChanlActions.pop();
				_pendingChanlActions.push({ActionType_e::MODIFY, channel});
				continue;
			}
			evt.events = channel->_events;
			evt.data.fd = fd;
			epoll_ctl(_epoll, EPOLL_CTL_ADD, fd, &evt);
			_channels[fd] = channel;
			minilog(LogLevel_e::DEBUG, "Add new channel(fd = %d, events = %d), sum to %ld channels", channel->_fd, channel->_events, _channels.size());
		}
		else if (action == ActionType_e::DELETE)
		{
			if (_channels.find(channel->_fd) == _channels.end())
			{
				minilog(LogLevel_e::WARRNIG, "This channel(fd = %d) is not in epoll(fd = %d)", channel->_fd, _epoll);
				_pendingChanlActions.pop();
				continue;
			}
			epoll_event evt = {};
			evt.data.fd = fd;
			epoll_ctl(_epoll, EPOLL_CTL_DEL, channel->_fd, &evt);
			close(fd);
			_channels[fd].reset();
			_channels.erase(fd);
			minilog(LogLevel_e::DEBUG, "Delete channel(fd = %d), sum to %ld channels", channel->_fd, _channels.size());
		}
		else if (action == ActionType_e::MODIFY)
		{
			if (_channels.find(channel->_fd) == _channels.end())
			{
				minilog(LogLevel_e::ERROR, "This channel(fd = %d) is not in epoll(fd = %d)", channel->_fd, _epoll);
				_pendingChanlActions.pop();
				continue;
			}
			int oldEvents = _channels[fd]->_events;
			epoll_event evt = {};
			evt.events = channel->_events;
			evt.data.fd = fd;
			epoll_ctl(_epoll, EPOLL_CTL_MOD, fd, &evt);
			_channels[fd] = channel;
			minilog(LogLevel_e::DEBUG, "Modify channel(fd = %d), events : %d -> %d", channel->_fd, oldEvents, channel->_events);
		}
		_pendingChanlActions.pop();
	}
}

SpReactor CreateSpReactor()
{
	return std::make_shared<Reactor>();
}