#include <log/log.h>

eva::Logger::ptr g_logger{EVA_LOG_ROOT()};

int main() {
    EVA_LOG_FATAL(g_logger) << "fatal msg";
    EVA_LOG_ERROR(g_logger) << "err msg";
    EVA_LOG_WARN(g_logger) << "warn msg";
    EVA_LOG_INFO(g_logger) << "info msg";
    EVA_LOG_DEBUG(g_logger) << "debug msg";

    g_logger->SetLevel(eva::LogLevel::Level::WARN);

    // EVA_LOG_FMT_FATAL(g_logger, "fatal %s:%d\n", __FILE__, __LINE__);
    // EVA_LOG_FMT_ERROR(g_logger, "err %s:%d\n", __FILE__, __LINE__);
    // EVA_LOG_FMT_INFO(g_logger, "info %s:%d\n", __FILE__, __LINE__);
    // EVA_LOG_FMT_DEBUG(g_logger, "debug %s:%d\n", __FILE__, __LINE__);

    auto file_appender{std::make_shared<eva::FileLogAppender>("/home/sjj/eva/log.txt")};
    g_logger->AddAppender(file_appender);

    EVA_LOG_FATAL(g_logger) << "fatal msg";
    EVA_LOG_ERROR(g_logger) << "err msg";
    EVA_LOG_WARN(g_logger) << "warn msg";
    EVA_LOG_INFO(g_logger) << "info msg";
    EVA_LOG_DEBUG(g_logger) << "debug msg";

    return 0;
}
