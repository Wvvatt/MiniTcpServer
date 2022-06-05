#pragma once
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
#include "EpollWrapper.hpp"

enum class ActionType_e
{
	ADD = 0,
	DELETE,
	MODIFY
};

class Reactor : public WorkerInterface
{
public:
	Reactor() = delete;
	explicit Reactor(const std::string &);
	~Reactor();
	bool Work() override;
	void SetThreadId(const std::thread::id &) override;
	void Close();
	void PushChannel(ActionType_e, SpChannel);
	bool isInSelfWorkThread() { return std::this_thread::get_id() == _thrdId; }
	void WakeUp();
	std::string GetName() const {return _name;};
private:
	struct ChannelAction
	{
		ActionType_e action;
		SpChannel channel;
	};
	void handlePendingChannel();

private:
	std::string _name;
	std::mutex _pendingMutex;
	std::queue<ChannelAction> _pendingChanlActions;
	SpEpoll _epoll;
	bool _running;
	std::thread::id _thrdId;
	int _wakeUpFd[2];
};

Reactor::Reactor(const std::string &name) : _name(name),
											_epoll(CreateSpEpoll())
{
	socketpair(AF_UNIX, SOCK_STREAM, 0, _wakeUpFd);
	auto wakeUpChannel = CreateSpChannelReadSend(_wakeUpFd[1], nullptr, ChannelEvent_e::IN,
		[this](SpChannel chan) {
			int fd = chan->_fd;
			char one = {};
			int n = read(fd, &one, sizeof(one));
			if(n != sizeof(one)){
				minilog(LogLevel_e::ERROR, "[%s] wakeup failed!", this->GetName().c_str());
			}
			else{
				minilog(LogLevel_e::INFO, "[%s] wake up!", this->GetName().c_str());
			}
		},
		nullptr,
		nullptr);
	_epoll->Add(wakeUpChannel);
}

Reactor::~Reactor()
{
	Close();
}

void Reactor::Close()
{

}

bool Reactor::Work()
{
	auto activeChans = _epoll->Poll(1000);
	for (auto chan : activeChans)
	{
		if (chan->_events & ChannelEvent_e::IN)
		{
			if (chan->_onConnect)
			{
				chan->_onConnect(chan);
				continue;
			}
			if (chan->_onRead)
			{
				chan->_onRead(chan);
			}
		}
		if (chan->_events & ChannelEvent_e::OUT)
		{
			if (chan->_onSend)
			{
				chan->_onSend(chan);
			}
		}
	}
	handlePendingChannel();
	return true;
}

void Reactor::SetThreadId(const std::thread::id &id)
{
	_thrdId = id;
}

void Reactor::PushChannel(ActionType_e action, SpChannel chan)
{
	{
		std::lock_guard<std::mutex> lock(_pendingMutex);
		//minilog(LogLevel_e::DEBUG, "action = %d, channel(fd = %d, events = %d)", action, chan->_fd, chan->_events);
		_pendingChanlActions.push({action, chan});
	}
	if (!isInSelfWorkThread())
	{
		WakeUp();
	}
}

void Reactor::WakeUp()
{
	char one = '1';
	auto n = write(_wakeUpFd[0], &one, sizeof(one));
	if(n != sizeof(one)){
		minilog(LogLevel_e::ERROR, "can't wakeup this epoll loop");
	}
}

void Reactor::handlePendingChannel()
{
	std::lock_guard<std::mutex> lock(_pendingMutex);
	while (_pendingChanlActions.size() > 0)
	{
		auto chaAct = _pendingChanlActions.front();
		auto channel = chaAct.channel;
		auto action = chaAct.action;
		minilog(LogLevel_e::DEBUG, "[%s] handle channel(fd = %d, events = %ld), action = %d", _name.c_str(), channel->_fd, channel->_events, action);
		if (action == ActionType_e::ADD)
		{
			if (_epoll->IsChannelInEpoll(channel))
			{
				minilog(LogLevel_e::WARRNIG, "[%s] channel(fd = %d) has already in epoll(fd = %d), so modify", _name.c_str(), channel->_fd, _epoll);
				_pendingChanlActions.pop();
				_pendingChanlActions.push({ActionType_e::MODIFY, channel});
				continue;
			}
			_epoll->Add(channel);
			//minilog(LogLevel_e::DEBUG, "[%s] Add new channel(fd = %d, events = %d), sum to %ld channels", _name.c_str(), channel->_fd, channel->_events, _epoll->GetChannelNum());
		}
		else if (action == ActionType_e::DELETE)
		{
			if (!_epoll->IsChannelInEpoll(channel))
			{
				minilog(LogLevel_e::WARRNIG, "[%s] This channel(fd = %d) is not in epoll(fd = %d)", _name.c_str(), channel->_fd, _epoll);
				_pendingChanlActions.pop();
				continue;
			}
			_epoll->Delete(channel);
			//minilog(LogLevel_e::DEBUG, "[%s] Delete channel(fd = %d), sum to %ld channels", _name.c_str(), channel->_fd, _epoll->GetChannelNum());
		}
		else if (action == ActionType_e::MODIFY)
		{
			if (!_epoll->IsChannelInEpoll(channel))
			{
				minilog(LogLevel_e::ERROR, "[%s] This channel(fd = %d) is not in epoll(fd = %d)", _name.c_str(), channel->_fd, _epoll);
				_pendingChanlActions.pop();
				continue;
			}
			_epoll->Modify(channel);
			//minilog(LogLevel_e::DEBUG, "[%s] Modify channel(fd = %d) to event %d", _name.c_str(), channel->_fd, channel->_events);
		}
		_pendingChanlActions.pop();
	}
}

/*--------------- shared_ptr -----------*/
using SpReactor = std::shared_ptr<Reactor>;
SpReactor CreateSpReactor(const std::string &name)
{
	return std::make_shared<Reactor>(name);
}