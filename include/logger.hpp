#pragma once

#include "singleton.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <expected>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <print>
#include <string>
#include <string_view>
#include <thread>
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

enum class LogMode : uint8_t { SYNC, ASYNC };

// --- MPSC Lock-Free Queue Implementation ---
template<typename T>
class MpscQueue {
public:
    MpscQueue(): _head(new Node), _tail(_head.load(std::memory_order_relaxed)) {}

    ~MpscQueue() {
        T temp;
        while (dequeue(temp))
            ;
        delete _head.load(std::memory_order_relaxed);
    }

    // Thread-safe enqueue (Wait-free)
    void enqueue(T&& data) {
        Node* new_node = new Node(std::move(data));
        Node* prev_head = _tail.exchange(new_node, std::memory_order_acq_rel);
        prev_head->_next.store(new_node, std::memory_order_release);
    }

    // Single-consumer dequeue
    bool dequeue(T& output) {
        Node* head = _head.load(std::memory_order_relaxed);
        Node* next = head->_next.load(std::memory_order_acquire);

        if (next == nullptr) {
            return false; // Queue is empty
        }

        output = std::move(next->_data);

        _head.store(next, std::memory_order_relaxed);
        delete head;
        return true;
    }

    [[nodiscard]] bool empty() const {
        Node* head = _head.load(std::memory_order_relaxed);
        return head->_next.load(std::memory_order_acquire) == nullptr;
    }

private:
    struct Node {
        T _data;
        std::atomic<Node*> _next { nullptr };

        Node() = default;
        explicit Node(T&& data): _data(std::move(data)) {}
    };

    std::atomic<Node*> _head;
    std::atomic<Node*> _tail;
};

// --- Log Event Structure for Async Queue ---
struct LogEvent {
    std::string priority_str;
    LogPriority priority;
    std::string message;
    std::string time_str; // capture time
};

// --- Appenders ---

class LogAppender {
public:
    using ptr = std::shared_ptr<LogAppender>;
    virtual ~LogAppender() = default;

    virtual void
    log(std::string_view time_str,
        std::string_view message_priority_str,
        LogPriority priority,
        std::string_view message) = 0;
};

class FileAppender: public LogAppender {
public:
    using ptr = std::shared_ptr<FileAppender>;
    explicit FileAppender(std::string filename): _filename { std::move(filename) } {
        _filestream.open(_filename, std::ios::app | std::ios::out);
    }

    ~FileAppender() override {
        if (_filestream.is_open()) {
            _filestream.close();
        }
    }

    std::expected<void, std::string> reopen_file() {
        std::lock_guard<std::mutex> lock(_fs_mutex);
        if (_filestream.is_open()) {
            _filestream.close();
        }
        _filestream.open(_filename, std::ios::app | std::ios::out);
        if (!_filestream.is_open()) {
            return std::unexpected(std::format("{}: Fail to open file", __func__));
        }
        return {};
    }

    void
    log(std::string_view time_str,
        std::string_view message_priority_str,
        LogPriority /*priority*/,
        std::string_view message) override {
        std::lock_guard<std::mutex> lock(_fs_mutex);
        if (_filestream.is_open()) {
            _filestream << time_str << message_priority_str << message << '\n';
            _filestream.flush(); // Can remove flush for better performance
        }
    }

private:
    std::string _filename;
    std::ofstream _filestream;
    std::mutex _fs_mutex; // for fstream
};

class StdoutAppender: public LogAppender {
public:
    using ptr = std::shared_ptr<StdoutAppender>;

    void
    log(std::string_view time_str,
        std::string_view message_priority_str,
        LogPriority /*priority*/,
        std::string_view message) override {
        std::println("{}{}{}", time_str, message_priority_str, message);
    }
};

// --- Logger Class ---

class Logger: public Singleton<Logger> {
    friend class Singleton<Logger>;

public:
    using ptr = std::shared_ptr<Logger>;

    Logger() {
        _worker_thread = std::jthread([this](const std::stop_token& st) { this->worker_loop(st); });
    }

    ~Logger() {
        _queue_counter.notify_all();
    }

    Logger& enable_time_recording(bool enable = true) {
        _enable_time_recording = enable;
        return *this;
    }

    Logger& set_mode(LogMode mode) {
        _mode = mode;
        return *this;
    }

    Logger& add_appender(const LogAppender::ptr& appender) {
        std::lock_guard<std::mutex> lock(_appender_mtx);
        _appenders.emplace_back(appender);
        return *this;
    }

    Logger& set_priority(LogPriority new_priority) {
        _priority.store(new_priority, std::memory_order_relaxed);
        return *this;
    }

#define XX(func, arg, priority) \
    template<class... Args> \
    void func(std::string_view fmt, Args&&... args) { \
        if (_priority.load(std::memory_order_relaxed) <= LogPriority::priority) { \
            auto msg = std::vformat(fmt, std::make_format_args(args...)); \
            log(#arg, LogPriority::priority, std::move(msg)); \
        } \
    }

    XX(trace, [Trace]\t, TRACE)
    XX(debug, [Debug]\t, DEBUG)
    XX(info, [Info]\t, INFO)
    XX(warn, [Warn]\t, WARN)
    XX(error, [Error]\t, ERROR)
    XX(fatal, [Fatal]\t, FATAL)

#undef XX

private:
    static std::string get_current_time_string() {
        auto now = std::chrono::system_clock::now();
        return std::format("{:%Y-%m-%d %H:%M:%S}", now);
    }

    static std::string apply_color(LogPriority pri, const std::string& text) {
        switch (pri) {
            case LogPriority::ERROR:
            case LogPriority::FATAL:
                return "\033[31m" + text + "\033[0m";
            case LogPriority::WARN:
                return "\033[33m" + text + "\033[0m";
            case LogPriority::INFO:
                return "\033[32m" + text + "\033[0m";
            case LogPriority::DEBUG:
                return "\033[36m" + text + "\033[0m";
            case LogPriority::TRACE:
                return "\033[90m" + text + "\033[0m";
            default:
                return text;
        }
    }

    void log(std::string pri_str, LogPriority msg_pri, std::string msg) {
        std::string time_str;
        if (_enable_time_recording) {
            time_str = get_current_time_string() + "\t";
        }

        if (_mode == LogMode::ASYNC) {
            _queue.enqueue(
                LogEvent { .priority_str = std::move(pri_str),
                           .priority = msg_pri,
                           .message = std::move(msg),
                           .time_str = std::move(time_str) }
            );
            _queue_counter.fetch_add(1, std::memory_order_release);
            _queue_counter.notify_one();
        } else {
            // write_to_appenders(time_str, pri_str, msg_pri, msg);
            write_to_appenders(
                LogEvent { .priority_str = pri_str,
                           .priority = msg_pri,
                           .message = msg,
                           .time_str = time_str }
            );
        }
    }

    void write_to_appenders(const LogEvent& event) {
        const auto& [pri_str, pri, msg, time_str] = event;

        auto colored_msg = apply_color(pri, msg);
        auto colored_pri = apply_color(pri, pri_str);

        std::lock_guard<std::mutex> lock(_appender_mtx);
        for (const auto& appender: _appenders) {
            appender->log(time_str, colored_pri, pri, colored_msg);
        }
    }

    void worker_loop(const std::stop_token& st) {
        while (!st.stop_requested()) {
            LogEvent event;
            if (_queue.dequeue(event)) {
                write_to_appenders(event);
                // reduce count not notify, since only customers are waiting
                _queue_counter.fetch_sub(1, std::memory_order_relaxed);
            } else {
                int curr = 0;
                _queue_counter.wait(curr, std::memory_order_acquire);
            }
        }

        LogEvent event;
        while (_queue.dequeue(event)) {
            write_to_appenders(event);
        }
    }

    std::atomic<LogPriority> _priority { LogPriority::TRACE };
    LogMode _mode { LogMode::SYNC };
    bool _enable_time_recording { false };

    std::mutex _appender_mtx;
    std::vector<LogAppender::ptr> _appenders;

    // Async components
    MpscQueue<LogEvent> _queue;
    std::jthread _worker_thread;
    std::atomic<int> _queue_counter { 0 }; // for notification
};

} // namespace logger
