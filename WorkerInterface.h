#pragma once
#include <memory>

class WorkerInterface
{
public:
	virtual bool Work() = 0;
	virtual void SetThreadId(const std::thread::id&) = 0;
};

using SpWorker = std::shared_ptr<WorkerInterface>;
using WpWorker = std::weak_ptr<WorkerInterface>;