#pragma once
#include <mutex>
#include <queue>
#include <stdarg.h>
#include <memory>
#include <mutex>
#include <thread>
#include "utils.h"

#define minilog(level, fmt, ...)                                                      \
    do                                                                                \
    {                                                                                 \
        Logger::GetInstance()->Log(level, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__); \
    } while (0)

enum class LogLevel_e
{
	INFO = 0,
	DEBUG,
	WARRNIG,
	ERROR
};

static std::string convert_level_to_string(LogLevel_e level)
{
	std::string levelStr;
	switch (level)
	{
	case LogLevel_e::INFO:
		levelStr = "INFO";
		break;
	case LogLevel_e::DEBUG:
		levelStr = "DEBUG";
		break;
	case LogLevel_e::WARRNIG:
		levelStr = "WARRNING";
		break;
	case LogLevel_e::ERROR:
		levelStr = "ERROR";
		break;
	default:
		break;
	}
	return levelStr;
}

class Logger
{
public:
	Logger(const Logger&) = delete;
	Logger operator=(const Logger&) = delete;
	static std::shared_ptr<Logger> GetInstance();
	void Log(LogLevel_e level, const char* file, const char* func, int line, const char* format, ...);
	void Work();

private:
	Logger()
	{
		_thread = std::thread(&Logger::Work, this);
		_running = true;
	}
	~Logger()
	{
		_running = false;
		_thread.join();
	}
	static std::shared_ptr<Logger> _inst;
	std::mutex _queueMutex;
	std::queue<std::string> _queue;
	std::thread _thread;
	bool _running;

};

std::shared_ptr<Logger> Logger::_inst = std::shared_ptr<Logger>(new Logger, [](Logger* p) {delete p; });
std::shared_ptr<Logger> Logger::GetInstance()
{
	return _inst;
}

void Logger::Log(LogLevel_e level, const char* file, const char* func, int line, const char* format, ...)
{
	char variaty[1024];
	va_list args;
	va_start(args, format);
	vsprintf(variaty, format, args);
	va_end(args);
	char res[2048];
	sprintf(res, "[%s] [%s:%d - %s] %s\n", convert_level_to_string(level).c_str(), file, line, func, variaty);
	std::string oneLog = res;
	std::lock_guard<std::mutex> lock(_queueMutex);
	_queue.push(std::move(oneLog));
}

void Logger::Work()
{
	while (_running) {
		_queueMutex.lock();
		if (_queue.size() > 0)
		{
			std::string& log = _queue.front();
			printf("%s", log.c_str());
			_queue.pop();
		}
		else {
			_queueMutex.unlock();
			sleep_ms(1);
		}
		_queueMutex.unlock();
	}
}
