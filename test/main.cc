#include "logger.hpp"

int main(int /*argc*/, char * /*argv*/[]) {
  logger::Logger &logger = logger::Logger::instance();

  logger::StdoutAppender::ptr stdout_appender =
      std::make_shared<logger::StdoutAppender>();
  logger.set_priority(logger::LogPriority::TRACE)
      .add_appender(stdout_appender)
      .enable_time_recording()
      .set_mode(logger::LogMode::ASYNC);
  logger.trace("function main start");
}
