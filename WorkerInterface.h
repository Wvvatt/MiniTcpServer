#pragma once
#include <memory>

class WorkerInterface
{
public:
	virtual bool Work() = 0;
};

using SpWorker = std::shared_ptr<WorkerInterface>;