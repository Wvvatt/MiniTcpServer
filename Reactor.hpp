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
class Reactor : public WorkerInterface, public std::enable_shared_from_this<Reactor>, noncopyable
{
public:
	Reactor() = delete;
	explicit Reactor(const std::string &);
	~Reactor();
	bool Work() override;
	void SetThreadId(const std::thread::id &) override;
	std::string GetName() const { return _name; };

	void AddChannel(SpChannel, ChannelEvent_e);
	void DelChannel(SpChannel);
	void EnableEvents(SpChannel, ChannelEvent_e);
	void DisableEvents(SpChannel, ChannelEvent_e);
	void PushFunctor(std::function<void(void)>);
private:
	void Init();
	void wakeup();
	bool isInSelfWorkThread() { return std::this_thread::get_id() == _thrdId; }
	void handlePendingFunctors();
	
private:
	bool _init;
	std::string _name;
	std::mutex _pendingMutex;
	std::queue<std::function<void(void)>> _pendingFunctors;
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
	auto wakeUpChannel = CreateSpChannel(_wakeUpFd[1], shared_from_this(), wake_up_call_back, nullptr, nullptr);
	_epoll->Add(wakeUpChannel, ChannelEvent_e::IN);
	_init = true;
}

bool Reactor::Work()
{
	if(!_init){
		Init();
	}
	_epoll->PollOnce(1000);
	handlePendingFunctors();
	return true;
}

void Reactor::SetThreadId(const std::thread::id &id)
{
	_thrdId = id;
}

void Reactor::AddChannel(SpChannel channel, ChannelEvent_e events)
{
	PushFunctor([this, channel, events](){
		if (_epoll->IsChannelInEpoll(channel))
		{
			minilog(LogLevel_e::WARRNIG, "[%s] channel(fd = %d) has already in epoll(fd = %d)", _name.c_str(), channel->GetSocket(), this->_epoll->GetEpollFd());
		}
		else{
			minilog(LogLevel_e::DEBUG, "[%s] add channel(fd = %d, events = %d)", _name.c_str(), channel->GetSocket(), channel->GetEvents());
			_epoll->Add(channel, events);
		}
	});
}

void Reactor::DelChannel(SpChannel channel)
{
	PushFunctor([this, channel](){
		if (!_epoll->IsChannelInEpoll(channel))
		{
			minilog(LogLevel_e::WARRNIG, "[%s] This channel(fd = %d) is not in epoll(fd = %d)", _name.c_str(), channel->GetSocket(), _epoll->GetEpollFd());
		}
		else{
			minilog(LogLevel_e::DEBUG, "[%s] delete channel(fd = %d, events = %d)", _name.c_str(), channel->GetSocket(), channel->GetEvents());
			_epoll->Delete(channel);
		}
	});
}

void Reactor::EnableEvents(SpChannel channel, ChannelEvent_e events)
{
	PushFunctor([this, channel, events](){
		if (!_epoll->IsChannelInEpoll(channel))
		{
			minilog(LogLevel_e::ERROR, "[%s] This channel(fd = %d) is not in epoll(fd = %d)", _name.c_str(), channel->GetSocket(), _epoll->GetEpollFd());
		}
		else{
			ChannelEvent_e oldEvts = channel->GetEvents();
			ChannelEvent_e newEvts = static_cast<ChannelEvent_e>(oldEvts | events);
			_epoll->Modify(channel, newEvts);
			minilog(LogLevel_e::DEBUG, "[%s] enable events %d, channel(fd = %d, events %d -> %d)", _name.c_str(), events, channel->GetSocket(), oldEvts, channel->GetEvents());
		}
	});
}

void Reactor::DisableEvents(SpChannel channel, ChannelEvent_e events){
	PushFunctor([this, channel, events](){
		if (!_epoll->IsChannelInEpoll(channel))
		{
			minilog(LogLevel_e::ERROR, "[%s] This channel(fd = %d) is not in epoll(fd = %d)", _name.c_str(), channel->GetSocket(), _epoll->GetEpollFd());
		}
		else{
			ChannelEvent_e oldEvts = channel->GetEvents();
			ChannelEvent_e newEvts = static_cast<ChannelEvent_e>(oldEvts & ~events);
			_epoll->Modify(channel, newEvts);
			minilog(LogLevel_e::DEBUG, "[%s] disable events %d, channel(fd = %d, events %d -> %d)", _name.c_str(), events, channel->GetSocket(), oldEvts, channel->GetEvents());
		}
	});
}

void Reactor::PushFunctor(std::function<void(void)> functor)
{
	{
		std::lock_guard<std::mutex> lock(_pendingMutex);
		_pendingFunctors.push(functor);
	}
	if (!isInSelfWorkThread())
	{
		wakeup();
	}
}

void Reactor::wakeup()
{
	char one = '1';
	auto n = write(_wakeUpFd[0], &one, sizeof(one));
	if (n != sizeof(one))
	{
		minilog(LogLevel_e::ERROR, "can't wakeup this epoll loop");
	}
}

void Reactor::handlePendingFunctors()
{
	std::lock_guard<std::mutex> lock(_pendingMutex);
	while (_pendingFunctors.size() > 0)
	{
		auto func = _pendingFunctors.front();
		if(func) func();
		_pendingFunctors.pop();
	}
}

/*--------------- shared_ptr -----------*/
SpReactor CreateSpReactor(const std::string &name)
{
	return std::make_shared<Reactor>(name);
}