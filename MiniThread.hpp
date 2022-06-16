#pragma once
#include <thread>
#include <functional>
#include <vector>
#include <mutex>
#include <string>
#include <algorithm>
#include "WorkerInterface.h"

class MiniThread
{
public:
	MiniThread();
	virtual ~MiniThread();
	MiniThread(const MiniThread&) = delete;
	MiniThread& operator=(const MiniThread&) = delete;
	void AddWorker(SpWorker);
	void DelWorker(SpWorker);
private:
	void Work();
private:
	bool _running;
	std::mutex _threadMutex;
	std::thread _thread;
	std::vector<SpWorker> _workers;
};

MiniThread::MiniThread() :
	_running(true),
	_thread(std::thread(&MiniThread::Work, this))
{

}

MiniThread::~MiniThread()
{
	_running = false;
	_thread.join();
}

void MiniThread::AddWorker(SpWorker worker)
{
	std::lock_guard<std::mutex> lock(_threadMutex);
	_workers.emplace_back(worker);
	worker->SetThreadId(_thread.get_id());
}

void MiniThread::DelWorker(SpWorker worker) {
	std::lock_guard<std::mutex> lock(_threadMutex);
	auto iter = std::find(_workers.begin(), _workers.end(), worker);
	if (_workers.end() != iter) {
		_workers.erase(iter);
	}
}

void MiniThread::Work()
{
	while (_running) {
		std::lock_guard<std::mutex> lock(_threadMutex);
		if(_workers.size() == 0){
			sleep_ms(10);
		}
		for (auto worker : _workers) {
			if (worker) {
				if (!worker->Work())
				{
					auto iter = std::find(_workers.begin(), _workers.end(), worker);
					if (_workers.end() != iter){
						_workers.erase(iter);
					}
					break;
				}
			}
		}
	}
}

/* ---------------------- ref ptr -----------------------*/
using SpThread = std::shared_ptr<MiniThread>;
SpThread CreateSpThread()
{
	return std::make_shared<MiniThread>();
}