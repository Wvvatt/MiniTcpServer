#pragma once
#include <functional>
#include <memory>
#include "utils.h"

struct Channel;
using SpChannel = std::shared_ptr<Channel>;
using CallBackFunc = std::function<void(SpChannel)>;

struct Channel
{
	Channel() = delete;;
	Channel(int fd, std::shared_ptr<void> priv, int evt, CallBackFunc connect, CallBackFunc read, CallBackFunc send, CallBackFunc error);
	~Channel() = default;
	int _fd;
	std::shared_ptr<void> _priv;
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