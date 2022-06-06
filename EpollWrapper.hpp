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
    bool Add(SpChannel);
    bool Delete(SpChannel);
    bool Modify(SpChannel, int);
    std::vector<SpChannel> Poll(int);

    bool IsChannelInEpoll(SpChannel) const;
    size_t GetChannelNum() const;
    int GetEpollFd() const { return _epoll;}
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
    for (auto iter = _channels.begin(); iter != _channels.end(); iter++){
        close(iter->first);
    }
    _channels.clear();
    close(_epoll);
}

bool EpollWrapper::Add(SpChannel chan) 
{
    int fd = chan->GetSocket();
    if(_channels.find(fd) != _channels.end()){
        return true;
    }
    epoll_event evt{};
    evt.events = chan->GetEvents();
    evt.data.fd = fd;
    if(epoll_ctl(_epoll, EPOLL_CTL_ADD, fd, &evt) < 0){
        minilog(LogLevel_e::ERROR, "epoll ctl add error = %s", strerror(errno));
        return false;
    }
    else{
        _channels[fd] = chan;
        return true;
    } 
}

bool EpollWrapper::Delete(SpChannel chan)
{
    int fd = chan->GetSocket();
    if(_channels.find(fd) == _channels.end()){
        return true;
    }
    epoll_event evt{};
    evt.data.fd = fd;
    if(epoll_ctl(_epoll, EPOLL_CTL_DEL, fd, &evt) < 0){
        minilog(LogLevel_e::ERROR, "epoll ctl del error = %s", strerror(errno));
        return false;
    }
    else{
        close(fd);
        _channels[fd].reset();
        _channels.erase(fd);
        return true;
    }
}

bool EpollWrapper::Modify(SpChannel chan, int evts)
{
    int fd = chan->GetSocket();
    if(_channels.find(fd) == _channels.end()){
        return false;
    }
    epoll_event evt{};
    evt.data.fd = fd; 
    evt.events = evts;
    if(epoll_ctl(_epoll, EPOLL_CTL_MOD, fd, &evt) < 0){
        minilog(LogLevel_e::ERROR, "epoll ctl mod error = %s", strerror(errno));
        return false;
    }
    else{
        _channels[fd]->SetEvents(evts);
        return true;
    }
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