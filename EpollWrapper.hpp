#pragma once

#include <map>
#include <vector>

#include "utils.h"
#include "MiniLog.hpp"
#include "Channel.hpp"

class EpollWrapper
{
public:
    EpollWrapper();
    ~EpollWrapper();
    void Add(SpChannel);
    void Delete(SpChannel);
    void Modify(SpChannel);
    std::vector<SpChannel> Poll(int);

    bool IsChannelInEpoll(SpChannel) const;
    size_t GetChannelNum() const;
private:
    int _epoll;
    std::map<int, SpChannel> _channels;
};

EpollWrapper::EpollWrapper():
    _epoll(epoll_create(EPOLL_MAX_EVENT))
{

}

EpollWrapper::~EpollWrapper()
{
    close(_epoll);
}

void EpollWrapper::Add(SpChannel chan) 
{
    int fd = chan->_fd;
    epoll_event evt{};
    evt.events = chan->_events;
    evt.data.fd = fd;
    epoll_ctl(_epoll, EPOLL_CTL_ADD, fd, &evt);
    _channels[fd] = chan;
}

void EpollWrapper::Delete(SpChannel chan)
{
    int fd = chan->_fd;
    epoll_event evt{};
    evt.data.fd = fd;
    epoll_ctl(_epoll, EPOLL_CTL_DEL, fd, &evt);
    close(fd);
    _channels[fd].reset();
	_channels.erase(fd);
}

void EpollWrapper::Modify(SpChannel chan)
{
    int fd = chan->_fd;
    epoll_event evt{};
    evt.events = chan->_events;
    evt.data.fd = fd; 
    epoll_ctl(_epoll, EPOLL_CTL_MOD, fd, &evt);
    _channels[fd] = chan;
}

std::vector<SpChannel> EpollWrapper::Poll(int timeout)
{
    std::vector<SpChannel> ret = {};
    epoll_event activeEvts[EPOLL_MAX_EVENT] = {};
    auto activeFds = epoll_wait(_epoll, activeEvts, EPOLL_MAX_EVENT, timeout);
    if (activeFds < 0){
        minilog(LogLevel_e::ERROR, "epoll_wait error!");
        return std::move(ret);
    }
    for (int i = 0; i < activeFds; i++){
        int fd = activeEvts[i].data.fd;
        if(_channels.find(fd) != _channels.end()){
            ret.push_back(_channels[fd]);
        }
    }
    return std::move(ret);
}

bool EpollWrapper::IsChannelInEpoll(SpChannel chan) const
{
    if(_channels.find(chan->_fd) != _channels.end()){
        return true;
    }
    return false;
}

size_t EpollWrapper::GetChannelNum() const
{
    return _channels.size();
}

/* -------------------- shared_ptr ---------------------*/
using SpEpoll = std::shared_ptr<EpollWrapper>;
SpEpoll CreateSpEpoll()
{
    return std::make_shared<EpollWrapper>();
}