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
	void Start();
	void Stop();
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
	_thread(std::thread(&MiniThread::Work, this)),
	_running(false)
{

}

MiniThread::~MiniThread()
{
	_running = false;
	_thread.join();
}

void MiniThread::Start()
{
	_running = true;
}

void MiniThread::Stop()
{
	_running = false;
}

void MiniThread::AddWorker(SpWorker worker)
{
	std::lock_guard<std::mutex> lock(_threadMutex);
	_workers.emplace_back(worker);
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
		for (auto worker : _workers) {
			if (worker) {
				if (!worker->Work()) {
					DelWorker(worker);
					break;
				}
			}
		}
	}
}