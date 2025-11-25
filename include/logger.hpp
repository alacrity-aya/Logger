#pragma once

#include "singleton.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <print>
#include <regex>
#include <sstream>
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

// --- ANSI Color Codes ---
namespace colors {
constexpr std::string_view RESET = "\033[0m";
constexpr std::string_view GREY = "\033[90m"; // Time
constexpr std::string_view CYAN = "\033[36m"; // Thread
constexpr std::string_view RED = "\033[31m";
constexpr std::string_view GREEN = "\033[32m";
constexpr std::string_view YELLOW = "\033[33m";
constexpr std::string_view BLUE = "\033[34m";
constexpr std::string_view MAGENTA = "\033[35m";
} // namespace colors

// --- MPSC Lock-Free Queue Implementation ---
template <typename T> class MpscQueue {
public:
  MpscQueue() : _head(new Node), _tail(_head.load(std::memory_order_relaxed)) {}

  ~MpscQueue() {
    T temp;
    while (dequeue(temp))
      ;
    delete _head.load(std::memory_order_relaxed);
  }

  void enqueue(T &&data) {
    Node *new_node = new Node(std::move(data));
    Node *prev_head = _tail.exchange(new_node, std::memory_order_acq_rel);
    prev_head->_next.store(new_node, std::memory_order_release);
  }

  bool dequeue(T &output) {
    Node *head = _head.load(std::memory_order_relaxed);
    Node *next = head->_next.load(std::memory_order_acquire);
    if (next == nullptr)
      return false;
    output = std::move(next->_data);
    _head.store(next, std::memory_order_relaxed);
    delete head;
    return true;
  }

  [[nodiscard]] bool empty() const {
    Node *head = _head.load(std::memory_order_relaxed);
    return head->_next.load(std::memory_order_acquire) == nullptr;
  }

private:
  struct Node {
    T _data;
    std::atomic<Node *> _next{nullptr};
    Node() = default;
    explicit Node(T &&data) : _data(std::move(data)) {}
  };
  std::atomic<Node *> _head;
  std::atomic<Node *> _tail;
};

// --- Log Event ---
struct LogEvent {
  LogPriority priority;
  std::string priority_str; // e.g., "INFO"
  std::string message;
  std::string time_str;  // Raw time string
  std::string thread_id; // Raw thread id string
};

// --- Appenders ---

class LogAppender {
public:
  using ptr = std::shared_ptr<LogAppender>;
  virtual ~LogAppender() = default;

  // Interface changed slightly: receive the full pre-formatted string
  virtual void log(std::string_view formatted_message) = 0;
};

class FileAppender : public LogAppender {
public:
  using ptr = std::shared_ptr<FileAppender>;
  explicit FileAppender(std::string filename) : _filename{std::move(filename)} {
    _filestream.open(_filename, std::ios::app | std::ios::out);
  }

  ~FileAppender() override {
    if (_filestream.is_open())
      _filestream.close();
  }

  void log(std::string_view formatted_message) override {
    std::lock_guard<std::mutex> lock(_fs_mutex);
    if (_filestream.is_open()) {
      // Remove ANSI color codes for file output to keep it clean
      std::string clean_msg = strip_ansi(formatted_message);
      _filestream << clean_msg << '\n';
      _filestream.flush();
    }
  }

private:
  std::string _filename;
  std::ofstream _filestream;
  std::mutex _fs_mutex;

  // Helper to remove ANSI codes for file readability
  std::string strip_ansi(std::string_view input) {
    static const std::regex ansi_regex(
        R"(\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~]))");
    std::string s(input);
    return std::regex_replace(s, ansi_regex, "");
  }
};

class StdoutAppender : public LogAppender {
public:
  using ptr = std::shared_ptr<StdoutAppender>;
  void log(std::string_view formatted_message) override {
    std::println("{}", formatted_message);
  }
};

// --- Logger Class ---

class Logger : public Singleton<Logger> {
  friend class Singleton<Logger>;

public:
  using ptr = std::shared_ptr<Logger>;

  Logger() {
    _worker_thread = std::jthread(
        [this](const std::stop_token &st) { this->worker_loop(st); });
  }

  ~Logger() { _queue_counter.notify_all(); }

  Logger &enable_time_recording(bool enable = true) {
    _enable_time_recording = enable;
    return *this;
  }

  // New: Toggle Thread ID
  Logger &enable_thread_id(bool enable = true) {
    _enable_thread_id = enable;
    return *this;
  }

  Logger &set_mode(LogMode mode) {
    _mode = mode;
    return *this;
  }

  Logger &add_appender(const LogAppender::ptr &appender) {
    std::lock_guard<std::mutex> lock(_appender_mtx);
    _appenders.emplace_back(appender);
    return *this;
  }

  Logger &set_priority(LogPriority new_priority) {
    _priority.store(new_priority, std::memory_order_relaxed);
    return *this;
  }

  // Standardized simplified macros with padding for alignment
#define XX(func, arg, priority)                                                \
  template <class... Args> void func(std::string_view fmt, Args &&...args) {   \
    if (_priority.load(std::memory_order_relaxed) <= LogPriority::priority) {  \
      auto msg = std::vformat(fmt, std::make_format_args(args...));            \
      log(#arg, LogPriority::priority, std::move(msg));                        \
    }                                                                          \
  }

  XX(trace, TRACE, TRACE)
  XX(debug, DEBUG, DEBUG)
  XX(info, INFO, INFO)
  XX(warn, WARN, WARN)
  XX(error, ERROR, ERROR)
  XX(fatal, FATAL, FATAL)

#undef XX

private:
  static std::string get_current_time_string() {
    auto now = std::chrono::system_clock::now();
    // Use standard format, seconds precision
    return std::format("{:%Y-%m-%d %H:%M:%S}", now);
  }

  static std::string_view get_color(LogPriority pri) {
    switch (pri) {
    case LogPriority::FATAL:
      return colors::MAGENTA;
    case LogPriority::ERROR:
      return colors::RED;
    case LogPriority::WARN:
      return colors::YELLOW;
    case LogPriority::INFO:
      return colors::GREEN;
    case LogPriority::DEBUG:
      return colors::BLUE;
    case LogPriority::TRACE:
      return colors::GREY;
    default:
      return colors::RESET;
    }
  }

  void log(std::string pri_str, LogPriority msg_pri, std::string msg) {
    std::string time_str;
    if (_enable_time_recording) {
      time_str = get_current_time_string();
    }

    std::string tid_str;
    if (_enable_thread_id) {
      std::stringstream ss;
      ss << std::this_thread::get_id();
      tid_str = ss.str();
    }

    LogEvent event{.priority = msg_pri,
                   .priority_str = std::move(pri_str),
                   .message = std::move(msg),
                   .time_str = std::move(time_str),
                   .thread_id = std::move(tid_str)};

    if (_mode == LogMode::ASYNC) {
      _queue.enqueue(std::move(event));
      _queue_counter.fetch_add(1, std::memory_order_release);
      _queue_counter.notify_one();
    } else {
      write_to_appenders(event);
    }
  }

  void write_to_appenders(const LogEvent &event) {
    // Assemble the full formatted string here with colors
    // Format: [TIME] [TID] [LEVEL] Message
    std::string buffer;

    // 1. Time (Grey)
    if (!event.time_str.empty()) {
      buffer +=
          std::format("{}[{}]{} ", colors::GREY, event.time_str, colors::RESET);
    }

    // 2. Thread ID (Cyan)
    if (!event.thread_id.empty()) {
      buffer += std::format("{}[{}]{} ", colors::CYAN, event.thread_id,
                            colors::RESET);
    }

    // 3. Priority (Color based on level)
    // Pad priority string to align messages (e.g., "INFO " vs "WARN ")
    auto color = get_color(event.priority);
    buffer +=
        std::format("{}[{:<5}]{} ", color, event.priority_str, colors::RESET);

    // 4. Message (White/Default)
    buffer += event.message;

    // Apply distinct color to message text for Errors/Fatal only for visibility
    if (event.priority >= LogPriority::ERROR) {
      // Optional: Wrap the message itself in color for errors
      // buffer += std::format("{}{}{}", color, event.message, colors::RESET);
    }

    std::lock_guard<std::mutex> lock(_appender_mtx);
    for (const auto &appender : _appenders) {
      appender->log(buffer);
    }
  }

  void worker_loop(const std::stop_token &st) {
    while (!st.stop_requested()) {
      LogEvent event;
      if (_queue.dequeue(event)) {
        write_to_appenders(event);
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

  std::atomic<LogPriority> _priority{LogPriority::TRACE};
  LogMode _mode{LogMode::SYNC};
  bool _enable_time_recording{false};
  bool _enable_thread_id{false}; // Default false

  std::mutex _appender_mtx;
  std::vector<LogAppender::ptr> _appenders;
  MpscQueue<LogEvent> _queue;
  std::jthread _worker_thread;
  std::atomic<int> _queue_counter{0};
};

// Global Macros
#define XX(func)                                                               \
  template <class... Args> void func(std::string_view fmt, Args &&...args) {   \
    Logger::instance().func(fmt, args...);                                     \
  }

XX(trace)
XX(debug)
XX(info)
XX(warn)
XX(error)
XX(fatal)

#undef XX

} // namespace logger
