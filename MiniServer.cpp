#include <semaphore.h>
#include <signal.h>
#include <algorithm>
#include "MiniLog.hpp"
#include "MiniThread.hpp"
#include "Reactor.hpp"

static sem_t *sem = new sem_t;

static void signal_handler(int sig_num)
{
    //signal(SIGINT, signal_handler);
    printf("exit\n");
    sem_post(sem);
}

void onRead(SpChannel chan)
{
    int fd = chan->GetSocket();
    SpReactor re = std::static_pointer_cast<Reactor>(chan->GetSpPrivData());
    char recvBuffer[1024] = {};
    int ret = recv(fd, recvBuffer, sizeof(recvBuffer), 0);
    if (ret == 0) {
        minilog(LogLevel_e::WARRNIG, "peer close");
        re->DelChannel(chan);
    }
    else if (ret < 0) {
        minilog(LogLevel_e::DEBUG, "onRead ret < 0  errno = %d", errno);
        if(!(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)){
            minilog(LogLevel_e::ERROR, strerror(errno));
            re->DelChannel(chan);
        }
    }
    else{
        minilog(LogLevel_e::DEBUG, "onRead ret = %d", ret);
        chan->GetSendBuffer().clear();
        chan->AppendSendBuffer(std::string(recvBuffer));
        std::transform(chan->GetSendBuffer().begin(), chan->GetSendBuffer().end(), chan->GetSendBuffer().begin(), ::toupper);
        re->EnableEvents(chan, ChannelEvent_e::OUT);
    }
    return;
}

void onSend(SpChannel chan)
{
    SpReactor re = std::static_pointer_cast<Reactor>(chan->GetSpPrivData());
    if (!chan->GetSendBuffer().empty()) {
        auto sendLen = send(chan->GetSocket(), chan->GetSendBuffer().c_str(), chan->GetSendBuffer().size(), 0);
        minilog(LogLevel_e::DEBUG, "onSend sendLen = %d", sendLen);
        if (sendLen > 0) {
            chan->GetSendBuffer() = chan->GetSendBuffer().substr(sendLen);
        }
    }
    if (chan->GetSendBuffer().empty()) {
        re->DisableEvents(chan, ChannelEvent_e::OUT);
    }
    return;
}

void onConnect(SpChannel chan)
{
    SpReactor re = std::static_pointer_cast<Reactor>(chan->GetSpPrivData());
    int fd = chan->GetSocket();
    sockaddr_in clientAddr = {};
    socklen_t len = sizeof(clientAddr);
    int clientFd = accept(fd, (sockaddr*)&clientAddr, &len);
    minilog(LogLevel_e::INFO, "accept client address : %s:%d", inet_ntoa(clientAddr.sin_addr), htons(clientAddr.sin_port));
    if (clientFd == -1) {
        if (errno != EAGAIN && errno != EINTR) {
            return;
        }
        minilog(LogLevel_e::ERROR, "accept error:%s", strerror(errno));
    }
    else {
        fcntl(clientFd, F_SETFL, O_NONBLOCK);
        re->AddChannel(CreateSpChannelReadSend(clientFd, re, onRead, onSend, nullptr), ChannelEvent_e::IN);
    }
}

int main()
{
    sem_init(sem, 0, 0);
    signal(SIGINT, signal_handler);
    
    SpReactor listenRe = CreateSpReactor("listen reactor");
    SpReactor subRe = CreateSpReactor("sub reactor");
    MiniThread listenThrd;
    listenThrd.AddWorker(listenRe);
    listenThrd.Start();
    MiniThread subThrd;
    subThrd.AddWorker(subRe);
    subThrd.Start();

    listenRe->AddChannel(CreateSpChannelListen(12222, subRe, onConnect, nullptr), ChannelEvent_e::IN);

    sem_wait(sem);
    //listenThrd.Stop();
    //subThrd.Stop();
    printf("main end\n");
    return 0;
}