#pragma once
#include "MiniThread.hpp"
#include "Reactor.hpp"

class ReactorThread
{
public:
    ReactorThread() = delete;
    ReactorThread(const std::string& name);
    ~ReactorThread();
    bool Open();
    void Close();
    SpReactor Reactor(){return _reactor;}
private:
    SpReactor _reactor;
    SpThread _thread;
};

ReactorThread::ReactorThread(const std::string& name)
{
    _reactor = CreateSpReactor(name);
    _thread = CreateSpThread();
}

ReactorThread::~ReactorThread()
{
    
}

bool ReactorThread::Open()
{
    if(!_reactor || !_thread){
        return false;
    }
    _thread->Start();
    _thread->AddWorker(_reactor);
}

void ReactorThread::Close()
{
    _thread->DelWorker(_reactor);
    _thread->Stop();
}

using SpReactorThread = std::shared_ptr<ReactorThread>;
SpReactorThread CreateSpReactorThread(const std::string& name)
{
    return std::make_shared<ReactorThread>(name);
}