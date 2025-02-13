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
#include <string_view>
#include <utility>
#include <vector>

namespace logger {

enum class LogPriority : uint8_t {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Fatal = 5,
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
            std::println("{}{}", message_priority_str, message);
        }
    }
};

class Logger : public Singleton<Logger> {
    friend class Singleton<Logger>;

public:
    using ptr = std::shared_ptr<Logger>;

private:
    LogPriority _priority { LogPriority::Trace };
    mutable std::mutex _mtx;
    std::vector<LogAppender::ptr> _apprenders;
    // TODO:每一种apprenders设置一个线程?

public:
    void add_appender(const LogAppender::ptr appender)
    {
        _apprenders.emplace_back(appender);
    }

    void set_priority(LogPriority new_priority)
    {
        _priority = new_priority;
    }

    // std::string format_string(std::string_view runtime_format_string, auto... val)
    // {
    //     return std::format(std::runtime_format(runtime_format_string), val...);
    // }

#define XX(func, arg, priority)                                                                            \
    void func(std::string_view runtime_format_string, auto... val)                                         \
    {                                                                                                      \
        log(#arg, LogPriority::priority, std::format(std::runtime_format(runtime_format_string), val...)); \
    }

    XX(trace, [Trace]\t, Trace)
    XX(debug, [Debug]\t, Debug)
    XX(info, [Info]\t, Info)
    XX(warn, [Warn]\t, Warn)
    XX(error, [Error]\t, Error)
    XX(fatal, [Fatal]\t, Fatal)

#undef XX

private:
    void log(const char* message_priority_str, LogPriority message_priority, std::string message) const
    {
        std::ranges::for_each(_apprenders, [&](const auto& appender) {
            std::unique_lock<std::mutex> lock(_mtx);
            appender->log(message_priority_str, message_priority, message);
        });
    }
};

} // namespace
