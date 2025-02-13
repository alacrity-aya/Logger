#include "logger.h"
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#define NUM 5

int main(int /*argc*/, char* /*argv*/[])
{
    logger::FileAppender::ptr appender1 = std::make_shared<logger::FileAppender>("../../../../a.txt");

    logger::StdoutAppender::ptr appender2 = std::make_shared<logger::StdoutAppender>();

    auto logger = logger::Logger::get_instance();

    logger->set_priority(logger::LogPriority::Error).add_appender(appender2).add_appender(appender1).enable_time_recording();

    std::vector<std::thread> threads {};
    threads.reserve(NUM);
    for (int i = 0; i < NUM; i++) {
        threads.emplace_back([&]() {
            logger->error("this thread{} called", std::this_thread::get_id());
            logger->fatal("this thread{} called", std::this_thread::get_id());
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    std::string str { "{}" };
    logger->fatal(str, 1);
}
