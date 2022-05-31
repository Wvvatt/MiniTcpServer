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
    SpReactor re = std::static_pointer_cast<Reactor>(chan->_priv);
    char recvBuffer[1024] = {};
    int ret = recv(chan->_fd, recvBuffer, sizeof(recvBuffer), 0);
    if (ret == 0) {
        minilog(LogLevel_e::WARRNIG, "peer close");
        re->DelChannel(chan);
    }
    else if (ret < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
        minilog(LogLevel_e::ERROR, strerror(errno));
        re->DelChannel(chan);
    }
    else {
        chan->_buffer.clear();
        chan->_buffer = recvBuffer;
        std::transform(chan->_buffer.begin(), chan->_buffer.end(), chan->_buffer.begin(), ::toupper);
        chan->_events = ChannelEvent_e::OUT;
        re->ModChannel(chan);
    }
    return;
}

void onSend(SpChannel chan)
{
    SpReactor re = std::static_pointer_cast<Reactor>(chan->_priv);
    if (!chan->_buffer.empty()) {
        auto sendLen = send(chan->_fd, chan->_buffer.c_str(), chan->_buffer.size(), 0);
        if (sendLen > 0) {
            chan->_buffer = chan->_buffer.substr(sendLen);
        }
    }
    if (chan->_buffer.empty()) {
        chan->_events = ChannelEvent_e::IN;
        re->ModChannel(chan);
    }
    return;
}

void onConnect(SpChannel chan)
{
    SpReactor re = std::static_pointer_cast<Reactor>(chan->_priv);
    int fd = chan->_fd;
    sockaddr_in clientAddr = {};
    socklen_t len = sizeof(clientAddr);
    int clientFd = accept(fd, (sockaddr*)&clientAddr, &len);
    minilog(LogLevel_e::INFO, "accept client address : %s:%d", inet_ntoa(clientAddr.sin_addr), clientAddr.sin_port);
    if (clientFd == -1) {
        if (errno != EAGAIN && errno != EINTR) {
            return;
        }
        minilog(LogLevel_e::ERROR, "accept error:%s", strerror(errno));
    }
    else {
        fcntl(clientFd, F_SETFL, O_NONBLOCK);
        re->AddChannel(CreateSpChannelReadSend(clientFd, re, ChannelEvent_e::IN, onRead, onSend, nullptr));
    }
}

int main()
{
    sem_init(sem, 0, 0);
    signal(SIGINT, signal_handler);
    
    SpReactor spRe = CreateSpReactor();
    spRe->AddChannel(CreateSpChannelListen(12222, spRe, onConnect, nullptr));
    MiniThread reThread;
    reThread.AddWorker(spRe);
    reThread.Start();

    sem_wait(sem);
    reThread.Stop();
    printf("main end\n");
    return 0;
}