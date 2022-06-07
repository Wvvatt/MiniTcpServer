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

class Reactor;
using SpReactor = std::shared_ptr<Reactor>;
class Reactor : public WorkerInterface, public std::enable_shared_from_this<Reactor>
{
public:
	Reactor() = delete;
	explicit Reactor(const std::string &);
	~Reactor();
	bool Work() override;
	void SetThreadId(const std::thread::id &) override;

	void AddChannel(SpChannel, ChannelEvent_e);
	void DelChannel(SpChannel);
	void EnableEvents(SpChannel, ChannelEvent_e);
	void DisableEvents(SpChannel, ChannelEvent_e);

	bool isInSelfWorkThread() { return std::this_thread::get_id() == _thrdId; }
	void WakeUp();
	std::string GetName() const { return _name; };
private:
	void Init();
	enum class ActionType_e : uint8_t
	{
		ADD = 0,
		DELETE,
		ENABLE,
		DISABLE
	};
	struct ChannelAction
	{
		ActionType_e action;
		SpChannel channel;
		ChannelEvent_e events;
	};
	void PushChannel(ActionType_e action, SpChannel chan, ChannelEvent_e evts);
	void handlePendingChannel();
	
private:
	bool _init;
	std::string _name;
	std::mutex _pendingMutex;
	std::queue<ChannelAction> _pendingChanlActions;
	SpEpoll _epoll;
	bool _running;
	std::thread::id _thrdId;
	int _wakeUpFd[2];
};

static void wake_up_call_back(SpChannel chan)
{
	SpReactor re = std::static_pointer_cast<Reactor>(chan->GetSpPrivData());
	int fd = chan->GetSocket();
	char one = {};
	int n = read(fd, &one, sizeof(one));
	if (n != sizeof(one))
	{
		minilog(LogLevel_e::ERROR, "[%s] wakeup failed!", re->GetName().c_str());
	}
	else
	{
		minilog(LogLevel_e::INFO, "[%s] wake up!", re->GetName().c_str());
	}
}

Reactor::Reactor(const std::string &name) : 
	_name(name),
	_epoll(CreateSpEpoll())
{
	_init = false;
}

Reactor::~Reactor()
{

}

void Reactor::Init()
{
	socketpair(AF_UNIX, SOCK_STREAM, 0, _wakeUpFd);
	auto wakeUpChannel = CreateSpChannelReadSend(_wakeUpFd[1], shared_from_this(), wake_up_call_back, nullptr, nullptr);
	_epoll->Add(wakeUpChannel, ChannelEvent_e::IN);
	_init = true;
}

bool Reactor::Work()
{
	if(!_init){
		Init();
	}
	_epoll->PollOnce(1000);
	handlePendingChannel();
	return true;
}

void Reactor::SetThreadId(const std::thread::id &id)
{
	_thrdId = id;
}

void Reactor::AddChannel(SpChannel chan, ChannelEvent_e evts)
{
	PushChannel(ActionType_e::ADD, chan, evts);
}

void Reactor::DelChannel(SpChannel chan)
{
	PushChannel(ActionType_e::DELETE, chan, ChannelEvent_e::NONE);
}

void Reactor::EnableEvents(SpChannel chan, ChannelEvent_e evts)
{
	PushChannel(ActionType_e::ENABLE, chan, evts);
}

void Reactor::DisableEvents(SpChannel chan, ChannelEvent_e evts)
{
	PushChannel(ActionType_e::DISABLE, chan, evts);
}

void Reactor::PushChannel(ActionType_e action, SpChannel chan, ChannelEvent_e evts)
{
	{
		std::lock_guard<std::mutex> lock(_pendingMutex);
		//minilog(LogLevel_e::DEBUG, "action = %d, channel(fd = %d, events = %d), new events = %d", action, chan->GetSocket(), chan->GetEvents(), evts);
		_pendingChanlActions.push({action, chan, evts});
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
	if (n != sizeof(one))
	{
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
		auto evts = chaAct.events;
		switch (action)
		{
		case ActionType_e::ADD:
			if (_epoll->IsChannelInEpoll(channel))
			{
				minilog(LogLevel_e::WARRNIG, "[%s] channel(fd = %d) has already in epoll(fd = %d)", _name.c_str(), channel->GetSocket(), _epoll->GetEpollFd());
			}
			else{
				minilog(LogLevel_e::DEBUG, "[%s] add channel(fd = %d, events = %d)", _name.c_str(), channel->GetSocket(), channel->GetEvents());
				_epoll->Add(channel, evts);
			}
			break;
		case ActionType_e::DELETE:
			if (!_epoll->IsChannelInEpoll(channel))
			{
				minilog(LogLevel_e::WARRNIG, "[%s] This channel(fd = %d) is not in epoll(fd = %d)", _name.c_str(), channel->GetSocket(), _epoll->GetEpollFd());
			}
			else{
				minilog(LogLevel_e::DEBUG, "[%s] delete channel(fd = %d, events = %d)", _name.c_str(), channel->GetSocket(), channel->GetEvents());
				_epoll->Delete(channel);
			}
			break;
		case ActionType_e::ENABLE:
			if (!_epoll->IsChannelInEpoll(channel))
			{
				minilog(LogLevel_e::ERROR, "[%s] This channel(fd = %d) is not in epoll(fd = %d)", _name.c_str(), channel->GetSocket(), _epoll->GetEpollFd());
			}
			else{
				ChannelEvent_e oldEvts = channel->GetEvents();
				ChannelEvent_e newEvts = static_cast<ChannelEvent_e>(oldEvts | evts);
				_epoll->Modify(channel, newEvts);
				minilog(LogLevel_e::DEBUG, "[%s] enable events %d, channel(fd = %d, events %d -> %d)", _name.c_str(), evts, channel->GetSocket(), oldEvts, channel->GetEvents());
			}
			break;
		case ActionType_e::DISABLE:
			if (!_epoll->IsChannelInEpoll(channel))
			{
				minilog(LogLevel_e::ERROR, "[%s] This channel(fd = %d) is not in epoll(fd = %d)", _name.c_str(), channel->GetSocket(), _epoll->GetEpollFd());
			}
			else{
				ChannelEvent_e oldEvts = channel->GetEvents();
				ChannelEvent_e newEvts = static_cast<ChannelEvent_e>(oldEvts & ~evts);
				_epoll->Modify(channel, newEvts);
				minilog(LogLevel_e::DEBUG, "[%s] disable events %d, channel(fd = %d, events %d -> %d)", _name.c_str(), evts, channel->GetSocket(), oldEvts, channel->GetEvents());
			}
			break;
		default:
			break;
		}
		_pendingChanlActions.pop();
	}
}

/*--------------- shared_ptr -----------*/
SpReactor CreateSpReactor(const std::string &name)
{
	return std::make_shared<Reactor>(name);
}