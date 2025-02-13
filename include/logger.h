#pragma once

#include "singleton.h"
#include <algorithm>
#include <array>
#include <expected>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <ostream>
#include <print>
#include <string>
#include <string_view>
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

    virtual void log(std::string_view message_priority_str, LogPriority /*message_priority*/, std::string message) = 0;

protected:
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

    void log(std::string_view message_priority_str, LogPriority /*message_priority*/, std::string message) override
    {

        if (auto ret = log_impl(); ret.has_value()) {
            // TODO: add time stamp
            _filestream << message_priority_str << '\t' << message << '\n';
        } else {
            std::println(std::cerr, "{}", ret.error());
        }
    }

private:
    std::expected<void, std::string> log_impl()
    {
        if (_filestream.is_open()) {
            _filestream.close();
        }

        _filestream.open(_filename, std::ios::app | std::ios::out);

        if (!_filestream.is_open()) {
            return std::unexpected("FileAppender::log(): Fail to open file");
        }

        return {};
    }

    std::string _filename { "./log.txt" };
    std::ofstream _filestream;
};

class StdoutAppender : public LogAppender {
public:
    using ptr = std::shared_ptr<StdoutAppender>;

    void log(std::string_view message_priority_str, LogPriority /*message_priority*/, std::string message) override
    {
        std::println("{}{}", message_priority_str, message);
    }
};

class Logger : public Singleton<Logger> {
    friend class Singleton<Logger>;

public:
    using ptr = std::shared_ptr<Logger>;

public:
    Logger& enable_time_recording()
    {
        _enable_time_recording = true;
        return *this;
    }

    Logger& add_appender(const LogAppender::ptr& appender)
    {
        _apprenders.emplace_back(appender);
        return *this;
    }

    Logger& set_priority(LogPriority new_priority)
    {
        _priority = new_priority;
        return *this;
    }

#define XX(func, arg, priority)                                                                                \
    void func(std::string_view runtime_format_string, auto... val)                                             \
    {                                                                                                          \
        if (_priority <= LogPriority::priority) {                                                              \
            log(#arg, LogPriority::priority, std::format(std::runtime_format(runtime_format_string), val...)); \
        }                                                                                                      \
    }

    XX(trace, [Trace]\t, Trace)
    XX(debug, [Debug]\t, Debug)
    XX(info, [Info]\t, Info)
    XX(warn, [Warn]\t, Warn)
    XX(error, [Error]\t, Error)
    XX(fatal, [Fatal]\t, Fatal)

#undef XX

private:
    std::expected<std::string, std::string> get_current_time_string()
    {
        auto now = std::chrono::system_clock::now();
        time_t now_time = std::chrono::system_clock::to_time_t(now);
        tm local_time = *std::localtime(&now_time);

        // char buffer[100];
        constexpr size_t buf_length = 1000;
        std::array<char, buf_length> buffer {};
        if (0 == strftime(buffer.data(), buf_length, "%Y-%m-%d %H:%M:%S", &local_time)) {
            std::unexpected("fail to get currenttime");
        }
        return std::string(buffer.data());
    }

    void log(std::string message_priority_str, LogPriority message_priority, std::string message)
    {

        if (_enable_time_recording) {
#if 1
            if (auto ret = get_current_time_string(); ret.has_value()) {
                auto str = std::format("{}\t", ret.value());
                message_priority_str += str;
            } else {
                std::println(std::cerr, "{}", ret.error());
            }
#else // WARNING: why the following code cannot compile?
            auto now = std::chrono::system_clock::now();
            std::string time_str = std::format("{:%Y-%m-%d %H:%M:%S\t}", now);
#endif
        }

        std::ranges::for_each(_apprenders, [&](const auto& appender) {
            std::unique_lock<std::mutex> lock(_mtx);
            appender->log(message_priority_str, message_priority, message);
        });
    }

    LogPriority _priority { LogPriority::Trace };
    mutable std::mutex _mtx;
    std::vector<LogAppender::ptr> _apprenders;
    bool _enable_time_recording { false };
};

} // namespace
