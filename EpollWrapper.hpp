#pragma once

#include <map>
#include <vector>

#include "utils.h"
#include "MiniLog.hpp"
#include "Channel.hpp"

class EpollWrapper : noncopyable
{
public:
    EpollWrapper();
    ~EpollWrapper();
    bool Add(SpChannel, ChannelEvent_e);
    bool Delete(SpChannel);
    bool Modify(SpChannel, ChannelEvent_e);
    int PollOnce(int);

    bool IsChannelInEpoll(SpChannel) const;
    size_t GetChannelNum() const;
    int GetEpollFd() const { return _epoll;}
private:
    static const int kInitEventsListSize = 16;
    int _epoll;
    std::map<int, SpChannel> _channels;
    std::vector<epoll_event> _eventsList;
};

EpollWrapper::EpollWrapper():
    _epoll(epoll_create1(EPOLL_CLOEXEC)),
    _eventsList(kInitEventsListSize)
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

bool EpollWrapper::Add(SpChannel chan, ChannelEvent_e evts) 
{
    int fd = chan->GetSocket();
    if(_channels.find(fd) != _channels.end()){
        Modify(chan, evts);
        return true;
    }
    epoll_event evt{};
    evt.events = evts;
    evt.data.fd = fd;
    if(epoll_ctl(_epoll, EPOLL_CTL_ADD, fd, &evt) < 0){
        minilog(LogLevel_e::ERROR, "epoll ctl add error = %s", strerror(errno));
        return false;
    }
    else{
        chan->SetEvents(evts);
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

bool EpollWrapper::Modify(SpChannel chan, ChannelEvent_e evts)
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

int EpollWrapper::PollOnce(int timeout)
{
    int activeNums = epoll_wait(_epoll, &_eventsList[0], static_cast<int>(_eventsList.size()), timeout);
    if (activeNums < 0)
    {
        if (errno != EINTR){
            minilog(LogLevel_e::ERROR, "epoll_wait error!");
        }
        else{
            activeNums = 0;
        }
    }
    else if(0 == activeNums){
        // nothing happen
    }
    else
    {
        if(static_cast<size_t>(activeNums) == _eventsList.size()){
            _eventsList.resize(_eventsList.size()*2);
        }   
        for (int i = 0; i < activeNums; i++)
        {
            int fd = _eventsList[i].data.fd;
            int evts = _eventsList[i].events;
            if(_channels.find(fd) != _channels.end()){
                auto chan = _channels[fd];
                if(evts & EPOLLIN){
                    chan->HandleRead();
                }
                if(evts & EPOLLOUT){
                    chan->HandleSend();
                }
            }
            else{
                epoll_ctl(_epoll, EPOLL_CTL_DEL, fd, &_eventsList[i]);
            }
        }
    }
    return activeNums;
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