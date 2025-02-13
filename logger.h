#pragma once

#include "singleton.h"
#include <algorithm>
#include <cstdint>
#include <expected>
#include <format>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <print>
#include <string>
#include <utility>
#include <vector>

namespace logger {

enum class LogPriority : uint8_t {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5,
};

class LogAppender {
public:
    using ptr = std::shared_ptr<LogAppender>;

    virtual ~LogAppender() = default;

    virtual void log(std::string_view message_priority_str, LogPriority message_priority, std::string message) = 0;

protected:
    LogPriority _priority;

    std::string format_string(std::string_view runtime_format_string, auto... val)
    {
        return std::format(std::runtime_format(runtime_format_string), val...);
    }
};

class FileAppender : public LogAppender {
public:
    using ptr = std::shared_ptr<FileAppender>;
    explicit FileAppender(std::string filename)
        : _filename { std::move(filename) }
    {
    }

    std::expected<void, std::string> reopen_file()
    {
        if (_filestream.is_open()) {
            _filestream.close();
        }
        _filestream.open(_filename, std::ios::app | std::ios::out);
        if (!_filestream.is_open()) {
            return std::unexpected(std::format("{}: Fail to open file", __func__));
        }
    }

    void log(std::string_view message_priority_str, LogPriority message_priority, std::string message) override
    {

        if (_priority <= message_priority) {

            if (auto ret = log_impl(); ret.has_value()) {
                // TODO: add time stamp
                _filestream << message_priority_str << '\t' << message << '\n';
            } else {
                std::println(std::cerr, "{}", ret.error());
            }
        }
    }

private:
    std::expected<void, std::string> log_impl()
    {
        std::println("{}", _filename);

        if (_filestream.is_open()) {
            _filestream.close();
        }

        _filestream.open(_filename, std::ios::app | std::ios::out);

        if (!_filestream.is_open()) {
            return std::unexpected("FileAppender::log(): Fail to open file");
        }

        return {};
    }

private:
    std::string _filename { "./log.txt" };
    std::ofstream _filestream;
};

class StdoutAppender : public LogAppender {
public:
    using ptr = std::shared_ptr<StdoutAppender>;

    void log(std::string_view message_priority_str, LogPriority message_priority, std::string message) override
    {
        if (_priority <= message_priority) {
            std::println("{}\t{}", message_priority_str, message);
        }
    }
};

class Logger : public Singleton<Logger> {
    friend class Singleton<Logger>;

public:
    using ptr = std::shared_ptr<Logger>;

private:
    LogPriority _priority { LogPriority::TRACE };
    std::mutex _mtx;
    std::vector<LogAppender::ptr> _apprenders;
    // TODO:每一种apprenders设置一个线程?

public:
    void add_appender(LogAppender::ptr appender)
    {
    }

    void set_priority(LogPriority new_priority)
    {
        _priority = new_priority;
    }

    template <typename... Args>
    void trace(const char* message, Args... args)
    {
        log("[Trace]\t", LogPriority::TRACE, message, args...);
    }

    template <typename... Args>
    void debug(const char* message, Args... args)

    {
        log("[Debug]\t", LogPriority::DEBUG, message, args...);
    }

    template <typename... Args>
    void info(const char* message, Args... args)
    {
        log("[Info]\t", LogPriority::INFO, message, args...);
    }

    template <typename... Args>
    void warn(const char* message, Args... args)
    {
        log("[Warn]\t", LogPriority::WARN, message, args...);
    }

    template <typename... Args>
    void error(const char* message, Args... args)
    {
        log("[Error]\t", LogPriority::ERROR, message, args...);
    }

    template <typename... Args>
    void critical(const char* message, Args... args)
    {
        log("[Critical]\t", LogPriority::FATAL, message, args...);
    }

private:
    template <typename... Args>
    void log(const char* message_priority_str, LogPriority message_priority, const char* message)
    {
        std::for_each(_apprenders.begin(), _apprenders.end(), [&](const LogAppender::ptr& appender) {
            std::unique_lock<std::mutex> lock(_mtx);
            appender->log(message_priority_str, message_priority, message);
        });
    }
};

}
