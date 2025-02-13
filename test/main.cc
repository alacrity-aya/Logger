#include "../src/logger.cc"
#include <print>

int main(int argc, char* argv[])
{
    logger::FileAppender::ptr appender1 = std::make_shared<logger::FileAppender>("../../../../a.txt");

    logger::StdoutAppender::ptr appender2 = std::make_shared<logger::StdoutAppender>();

    auto logger = logger::Logger::get_instance();

    logger->add_appender(appender1);
    logger->add_appender(appender2);

    logger->trace("this{} is{} fatal msg{}", 1, 2, 3);

    std::println("main function ended");
}
