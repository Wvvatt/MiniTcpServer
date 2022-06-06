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

	void AddChannel(SpChannel);
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
		UPDATE
	};
	struct ChannelAction
	{
		ActionType_e action;
		SpChannel channel;
		int events;
	};
	void PushChannel(ActionType_e, SpChannel, int);
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
	auto wakeUpChannel = CreateSpChannelReadSend(_wakeUpFd[1], shared_from_this(), ChannelEvent_e::IN, wake_up_call_back, nullptr, nullptr);
	_epoll->Add(wakeUpChannel);
	_init = true;
}

bool Reactor::Work()
{
	if(!_init){
		Init();
	}
	auto activeChans = _epoll->Poll(1000);
	for (auto chan : activeChans)
	{
		if (chan->GetEvents() & ChannelEvent_e::IN)
		{
			if (chan->isListenChannel())
			{
				chan->HandleConnect();
				continue;
			}
			chan->HandleRead();
		}
		if (chan->GetEvents() & ChannelEvent_e::OUT)
		{
			chan->HandleSend();
		}
	}
	handlePendingChannel();
	return true;
}

void Reactor::SetThreadId(const std::thread::id &id)
{
	_thrdId = id;
}

void Reactor::AddChannel(SpChannel chan)
{
	PushChannel(ActionType_e::ADD, chan, 0);
}

void Reactor::DelChannel(SpChannel chan)
{
	PushChannel(ActionType_e::DELETE, chan, 0);
}

void Reactor::EnableEvents(SpChannel chan, ChannelEvent_e evts)
{
	int oldEvents = chan->GetEvents();
	int newEvents = oldEvents | evts;
	PushChannel(ActionType_e::UPDATE, chan, newEvents);
}

void Reactor::DisableEvents(SpChannel chan, ChannelEvent_e evts)
{
	int oldEvents = chan->GetEvents();
	if (oldEvents & evts)
	{ //旧事件中有这个事件
		int newEvents = oldEvents ^ evts;
		PushChannel(ActionType_e::UPDATE, chan, newEvents);
	}
}

void Reactor::PushChannel(ActionType_e action, SpChannel chan, int evts)
{
	{
		std::lock_guard<std::mutex> lock(_pendingMutex);
		// minilog(LogLevel_e::DEBUG, "action = %d, channel(fd = %d, events = %d)", action, chan->_fd, chan->_events);
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
		minilog(LogLevel_e::DEBUG, "[%s] pending num = %d, handle channel(fd = %d, events = %d), action = %d, new events = %d",  _name.c_str(), _pendingChanlActions.size(), channel->GetSocket(), channel->GetEvents(), action, evts);
		switch (action)
		{
		case ActionType_e::ADD:
			if (_epoll->IsChannelInEpoll(channel))
			{
				minilog(LogLevel_e::WARRNIG, "[%s] channel(fd = %d) has already in epoll(fd = %d)", _name.c_str(), channel->GetSocket(), _epoll->GetEpollFd());
			}
			else{
				_epoll->Add(channel);
			}
			break;
		case ActionType_e::DELETE:
			if (!_epoll->IsChannelInEpoll(channel))
			{
				minilog(LogLevel_e::WARRNIG, "[%s] This channel(fd = %d) is not in epoll(fd = %d)", _name.c_str(), channel->GetSocket(), _epoll->GetEpollFd());
			}
			else{
				_epoll->Delete(channel);
			}
			break;
		case ActionType_e::UPDATE:
			if (!_epoll->IsChannelInEpoll(channel))
			{
				minilog(LogLevel_e::ERROR, "[%s] This channel(fd = %d) is not in epoll(fd = %d)", _name.c_str(), channel->GetSocket(), _epoll->GetEpollFd());
			}
			else{
				_epoll->Modify(channel, evts);
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