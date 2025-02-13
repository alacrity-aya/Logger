#include "logger.h"
#include <print>

int main(int argc, char* argv[])
{
    // logger::FileAppender::ptr appender = std::make_shared<logger::FileAppender>("../../../../a.txt");
    // appender->log("[CRITICAL]:", logger::LogPriority::FATAL, "123231232");

    logger::StdoutAppender::ptr appender = std::make_shared<logger::StdoutAppender>();
    appender->log("[CRITICAL]:", logger::LogPriority::Fatal, "123231232");

    auto logger = logger::Logger::get_instance();
    // logger->debug(const char* message, Args args...)
}
