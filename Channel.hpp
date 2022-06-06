#pragma once
#include <functional>
#include <memory>
#include "utils.h"

class Channel;
using SpChannel = std::shared_ptr<Channel>;
using CallBackFunc = std::function<void(SpChannel)>;
class EpollWrapper;
class Channel : public std::enable_shared_from_this<Channel>
{
	friend class EpollWrapper;
public:
	Channel() = delete;;
	Channel(int fd, std::shared_ptr<void> priv, int evt, CallBackFunc connect, CallBackFunc read, CallBackFunc send, CallBackFunc error);
	Channel(const Channel&) = delete;
	~Channel();

	int GetSocket() const { return _fd;}
	int GetEvents() const { return _events;}
	std::shared_ptr<void> GetSpPrivData() { return _priv.lock();}
	bool isListenChannel() const { return _onConnect != nullptr;}

	void HandleConnect(){
		if (_onConnect) _onConnect(shared_from_this());
	}
	void HandleRead(){
		if (_onRead) _onRead(shared_from_this());
	}
	void HandleSend(){
		if (_onSend) _onSend(shared_from_this());
	}
	void HandleError(){
		if (_onError) _onError(shared_from_this());
	}
	std::string& GetBuffer() { return _buffer;}
private:
	void SetEvents(int evts) { _events = evts;}
private:
	int _fd;
	std::weak_ptr<void> _priv;		//使用weak_ptr避免出现循环引用
	int _events;
	CallBackFunc _onConnect;;
	CallBackFunc _onRead;
	CallBackFunc _onSend;
	CallBackFunc _onError;
	std::string _buffer;
};

Channel::Channel(int fd, std::shared_ptr<void> priv, int evt, CallBackFunc connect, CallBackFunc read, CallBackFunc send, CallBackFunc error) :
	_fd(fd),
	_priv(priv),
	_events(evt),
	_onConnect(connect),
	_onRead(read),
	_onSend(send),
	_onError(error)
{
	_buffer.clear();
}

Channel::~Channel()
{
	if(_fd){
		close(_fd);
	}
}


/*--------------------------------- shared_ptr --------------------*/
SpChannel CreateSpChannel(int fd, std::shared_ptr<void> priv, int evt, CallBackFunc connect, CallBackFunc read, CallBackFunc send, CallBackFunc error)
{
	return std::make_shared<Channel>(fd, priv, evt, connect, read, send, error);
}

SpChannel CreateSpChannelListen(int port, std::shared_ptr<void> priv, CallBackFunc connect, CallBackFunc error)
{
	int listenFd = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in sin = {};
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);
	bind(listenFd, (sockaddr*)&sin, sizeof(sin));
	listen(listenFd, 20);
	fcntl(listenFd, F_SETFL, O_NONBLOCK);
	return std::make_shared<Channel>(listenFd, priv, ChannelEvent_e::IN, connect, nullptr, nullptr, error);
}


SpChannel CreateSpChannelReadSend(int fd, std::shared_ptr<void> priv, int evt, CallBackFunc read, CallBackFunc send, CallBackFunc error)
{
	return std::make_shared<Channel>(fd, priv, evt, nullptr, read, send, error);
}