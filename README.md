# C++20 High-Performance Asynchronous Logger

- **Header only** . The library is a part of [kuro](https://github.com/alacrity-aya/Kuro) 

A lightweight, high-performance, header-only asynchronous logging library for C++20. It utilizes C++20's `<format>` feature for modern, type-safe formatting and employs an MPSC (Multi-Producer Single-Consumer) lock-free queue for highly efficient asynchronous log recording, supporting colorful console output and file persistence.

## ✨ Key Features

  * **⚡ High-Performance Async Mode**: Built-in MPSC (Multi-Producer Single-Consumer) lock-free queue offloads log writing from business threads, significantly reducing main thread latency.
  * **🛠️ Native C++20 Support**: Utilizes the modern `std::format` syntax, providing type-safe and intuitive formatting without external dependencies like fmtlib.
  * **🎨 Intelligent Color Management**:
      * Console output supports semantic color highlighting based on log level, timestamp, and thread ID.
      * File output automatically strips ANSI color codes for clean, readable log files.
  * **⏱️ Configurable Metadata**: Supports configurable enabling/disabling of **Timestamp** and **Thread ID** recording for flexible output format.
  * **🔌 Pluggable Appenders**: Supports multiple output targets (Stdout / File) and is easily extensible for custom output sources (e.g., network, database).
  * **📂 Header-only**: Simple integration—just include the header file.

## 📦 Requirements

  * C++20 Compliant Compiler (e.g., GCC 13+, Clang 16+, MSVC 19.30+)
  * Standard library support for `<format>`, `<jthread>`, etc.

## 🚀 Quick Start

### 1\. Include the Header

Copy `logger.hpp` into your project.

### 2\. Initialization and Usage

```cpp
#include "logger.hpp"
#include <iostream>

int main() {
    // 1. Initialization (Typically done once at program startup)
    logger::Logger::instance()
        // Add Console output (Stdout)
        .add_appender(std::make_shared<logger::StdoutAppender>())
        // Add File output
        .add_appender(std::make_shared<logger::FileAppender>("server.log"))
        // Enable Timestamp (YYYY-MM-DD HH:MM:SS)
        .enable_time_recording(true)
        // Enable Thread ID logging (New Feature)
        .enable_thread_id(true)
        // Set to Asynchronous mode (Recommended for production)
        .set_mode(logger::LogMode::ASYNC)
        // Set the minimum log level
        .set_priority(logger::LogPriority::TRACE);

    // 2. Logging (Supports std::format syntax)
    int port = 8080;
    std::string service = "AuthService";

    logger::info("Starting {} on port {}", service, port);
    
    // Example different levels
    logger::debug("Debug info: variable x = {}", 42);
    logger::warn("High memory usage detected: {}%", 85);
    logger::error("Connection failed: {}", "Timeout");
    
    // Example high priority log
    if (port < 1024) {
        logger::fatal("Cannot bind to privileged port {}", port);
    }
    
    // Note: The logger's worker thread will shut down automatically
    // when the program exits.
    return 0;
}
```

## ⚙️ Configuration

Use the `logger::Logger::instance()` singleton with chained calls for configuration:

| Method | Description | Default Value |
| :--- | :--- | :--- |
| `add_appender(ptr)` | Adds a log output destination (StdoutAppender / FileAppender) | None |
| `set_mode(mode)` | Set to `SYNC` (synchronous write) or `ASYNC` (asynchronous queue) | `SYNC` |
| `enable_time_recording(bool)`| Whether to display the timestamp in the log line | `false` |
| `enable_thread_id(bool)` | Whether to display the calling thread's ID (New Feature) | `false` |
| `set_priority(priority)` | Sets the global minimum log level to be recorded | `TRACE` |

### Log Priority Levels

The priority levels, from lowest to highest, along with their default console colors:

| Level | Priority | Console Color |
| :--- | :--- | :--- |
| `TRACE` | 0 | Deep Grey |
| `DEBUG` | 1 | Blue |
| `INFO` | 2 | Green |
| `WARN` | 3 | Yellow |
| `ERROR` | 4 | Red |
| `FATAL` | 5 | Magenta |

## 🏗️ Architecture

### Asynchronous Model

In `ASYNC` mode, the logger starts a dedicated background worker thread (`std::jthread`).

1.  **Producers**: When a business thread calls `logger::info(...)`, the log event is pushed onto the **MPSC Lock-Free Queue**. This process is highly non-blocking, minimizing the impact on application performance.
2.  **Consumer**: The background thread efficiently dequeues events and dispatches them to all registered Appenders (e.g., writing to disk, sending to the console).

### Output Format

The standardized log output format is:

```text
[Timestamp] [ThreadID] [LEVEL] Message Content
```

Example Console Output:

```text
[2025-11-25 22:23:01] [14023] [INFO ] Server started successfully.
```

*Note: Timestamps are **Grey**, Thread IDs are **Cyan**, and the Log Level is colored according to its priority.*

-----

## 📝 Extension

If you need to send logs to a network service or a database, simply inherit from `logger::LogAppender` and implement the `log` method:

```cpp
class NetworkAppender : public logger::LogAppender {
public:
    void log(std::string_view formatted_message) override {
        // Send formatted_message to the remote server...
    }
};
```
