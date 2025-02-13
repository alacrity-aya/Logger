#include "logger.h"
#include <expected>
#include <memory>
#include <print>

int main(int argc, char* argv[])
{
    logger::FileAppender::ptr appender = std::make_shared<logger::FileAppender>("../../../../a.txt");
    appender->log("[CRITICAL]:", logger::LogPriority::FATAL, "123231232");
}
