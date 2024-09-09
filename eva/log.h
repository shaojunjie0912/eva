#pragma once

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// 在想要不要把跟时间相关的头文件转为 chrono?, 目前是 ctime，用起来也挺好
// 哪怕用chrono估计还是得转ctime

// 为什么新版在 LogAppender.Log 中删除了 LogLevel::Level level
// 貌似很多函数都删除了这个 level 形参

/**
 * @brief 使用流式方式将日志级别level的日志写入到logger
 * @details 构造一个LogEventWrap对象，包裹包含日志器和日志事件，在对象析构时调用日志器写日志事件
 * TODO: 协程id未实现，暂时写0
 */
#define EVA_LOG_LEVEL(logger, level)                                                         \
    if (logger->GetLevel() >= level)                                                         \
    eva::LogEventWrap{logger,                                                                \
                      eva::LogEvent::ptr{new eva::LogEvent{                                  \
                          logger->GetName(), level, __FILE__, __LINE__,                      \
                          eva::GetElapsedMS() - logger->GetCreateTime(), eva::GetThreadId(), \
                          eva::GetFiberId(), time(0), eva::GetThreadName()}}}                \
        .GetLogEvent()                                                                       \
        ->GetSs()

#define EVA_LOG_FATAL(logger) EVA_LOG_LEVEL(logger, eva::LogLevel::Level::FATAL)

#define EVA_LOG_ALERT(logger) EVA_LOG_LEVEL(logger, eva::LogLevel::Level::ALERT)

#define EVA_LOG_CRIT(logger) EVA_LOG_LEVEL(logger, eva::LogLevel::Level::CRIT)

#define EVA_LOG_ERROR(logger) EVA_LOG_LEVEL(logger, eva::LogLevel::Level::ERROR)

#define EVA_LOG_WARN(logger) EVA_LOG_LEVEL(logger, eva::LogLevel::Level::WARN)

#define EVA_LOG_NOTICE(logger) EVA_LOG_LEVEL(logger, eva::LogLevel::Level::NOTICE)

#define EVA_LOG_INFO(logger) EVA_LOG_LEVEL(logger, eva::LogLevel::Level::INFO)

#define EVA_LOG_DEBUG(logger) EVA_LOG_LEVEL(logger, eva::LogLevel::Level::DEBUG)

namespace eva {

// 日志级别
class LogLevel {
public:
    enum class Level {
        FATAL = 0,     // 致命情况，系统不可用
        ALERT = 100,   // 高优先级情况，例如数据库系统崩溃
        CRIT = 200,    // 严重错误，例如硬盘错误
        ERROR = 300,   // 错误
        WARN = 400,    // 警告
        NOTICE = 500,  // 正常但值得注意
        INFO = 600,    // 一般信息
        DEBUG = 700,   // 调试信息
        NOTSET = 800   // 未设置
    };

    static std::string ToString(LogLevel::Level const& level) {
        switch (level) {
#define XX(name)                \
    case LogLevel::Level::name: \
        return #name;
            XX(FATAL);
            XX(ALERT);
            XX(CRIT);
            XX(ERROR);
            XX(WARN);
            XX(NOTICE);
            XX(INFO);
            XX(DEBUG);
#undef XX
            default:
                return "NOTSET";
        }
    }

    static LogLevel::Level FromString(std::string const& str) {
#define XX(level, v)                   \
    if (str == #v) {                   \
        return LogLevel::Level::level; \
    }
        XX(FATAL, fatal)
        XX(ALERT, alert)
        XX(CRIT, crit)
        XX(ERROR, error)
        XX(WARN, warn)
        XX(NOTICE, notice)
        XX(INFO, info)
        XX(DEBUG, debug)

        XX(FATAL, FATAL)
        XX(ALERT, ALERT)
        XX(CRIT, CRIT)
        XX(ERROR, ERROR)
        XX(WARN, WARN)
        XX(NOTICE, NOTICE)
        XX(INFO, INFO)
        XX(DEBUG, DEBUG)
#undef XX
        return LogLevel::Level::NOTSET;
    }
};

// 日志事件
class LogEvent {
public:
    using ptr = std::shared_ptr<LogEvent>;

public:
    LogEvent(const std::string& logger_name, LogLevel::Level level, const char* file, int32_t line,
             int64_t elapse, uint32_t thread_id, uint64_t fiber_id, uint64_t time,
             const std::string& thread_name);

public:
    // Get 方法
    LogLevel::Level GetLevel() const { return level_; }

    std::string GetContent() const { return ss_.str(); }

    // WARN: 返回引用不能加 const
    std::stringstream& GetSs() { return ss_; }

    char const* GetFile() const { return file_; }

    int32_t GetLine() const { return line_; }

    int64_t GetElapse() const { return elapse_; }

    uint32_t GetThreadId() const { return thread_id_; }

    uint32_t GetFiberId() const { return fiber_id_; }

    uint64_t GetTime() const { return time_; }

    std::string GetThreadName() const { return thread_name_; }

    std::string GetLoggerName() const { return logger_name_; }

private:
    // 内存对齐
    LogLevel::Level level_;      // 日志级别
    std::stringstream ss_;       // 日志内容(流式写入日志)
    const char* file_{nullptr};  // 文件名
    int32_t line_{0};            // 行号
    uint32_t elapse_{0};         // 程序启动开始到现在的毫秒数
    uint32_t thread_id_{0};      // 线程 id
    uint32_t fiber_id_{0};       // 协程 id
    uint64_t time_{0};           // 时间戳
    std::string thread_name_;    // 线程名称
    std::string logger_name_;    // 日志器名称
};

// 日志格式器
class LogFormatter {
public:
    using ptr = std::shared_ptr<LogFormatter>;
    /**
     * @brief 构造函数
     * @param[in] pattern 格式模板，参考eva与log4cpp
     * @details 模板参数说明：
     * - %%m 消息
     * - %%p 日志级别
     * - %%c 日志器名称
     * - %%d 日期时间，后面可跟一对括号指定时间格式，比如%%d{%%Y-%%m-%%d
     * %%H:%%M:%%S}，这里的格式字符与C语言strftime一致
     * - %%r 该日志器创建后的累计运行毫秒数
     * - %%f 文件名
     * - %%l 行号
     * - %%t 线程id
     * - %%F 协程id
     * - %%N 线程名称
     * - %%% 百分号
     * - %%T 制表符
     * - %%n 换行
     *
     * 默认格式：%%d{%%Y-%%m-%%d %%H:%%M:%%S}%%T%%t%%T%%N%%T%%F%%T[%%p]%%T[%%c]%%T%%f:%%l%%T%%m%%n
     *
     * 默认格式描述：年-月-日 时:分:秒 [累计运行毫秒数] \\t 线程id \\t 线程名称 \\t 协程id \\t
     * [日志级别] \\t [日志器名称] \\t 文件名:行号 \\t 日志消息 换行符
     */
    LogFormatter(std::string const& pattern =
                     "%d{%Y-%m-%d %H:%M:%S} [%rms]%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n");

public:
    /**
     * @brief 初始化，解析格式模板，提取模板项
     */
    void Init();

    /**
     * @brief 对日志事件进行格式化，返回格式化日志文本
     * @param[in] event 日志事件
     * @return 格式化日志字符串
     */
    std::string Format(LogEvent::ptr event);

    /**
     * @brief 对日志事件进行格式化，返回格式化日志流
     * @param[in] event 日志事件
     * @param[in] os 日志输出流
     * @return 格式化日志流
     */
    std::ostream& Format(std::ostream& os, LogEvent::ptr event);

public:
    /**
     * @brief 日志内容格式化项，虚基类，用于派生出不同的格式化项
     */
    class FormatItem {
    public:
        using ptr = std::shared_ptr<FormatItem>;
        virtual ~FormatItem() {}

    public:
        /**
         * @brief 格式化日志事件
         */
        virtual void Format(std::ostream& os, LogEvent::ptr event) = 0;
    };

private:
    std::string pattern_;                 // 日志格式模板
    std::vector<FormatItem::ptr> items_;  // 解析后的格式模板数组
    bool error_;                          // 是否出错
};

// 日志输出地(纯虚基类)
class LogAppender {
public:
    using ptr = std::shared_ptr<LogAppender>;

    /**
     * @brief 构造函数
     * @param[in] default_formatter 默认日志格式器
     */
    LogAppender(LogFormatter::ptr default_formatter);

    virtual ~LogAppender() {}

public:
    /**
     * @brief 写入日志
     */
    virtual void Log(LogEvent::ptr event) = 0;

public:
    /**
     * @brief 获取日志格式器
     */
    LogFormatter::ptr GetFormatter() const {
        // 这里需要加锁？
        return formatter_ ? formatter_ : default_formatter_;
    }

    /**
     * @brief 设置日志格式器
     */
    void SetFormatter(LogFormatter::ptr formatter) {
        // 这里需要加锁？
        formatter_ = formatter;
    };

protected:
    std::mutex mtx_;
    LogFormatter::ptr formatter_;          // 日志格式器
    LogFormatter::ptr default_formatter_;  // 默认日志格式器
};

/**
 * 日志器
 *
 * 一个Logger包含多个LogAppender和一个日志级别，提供log方法，传入日志事件，
 * 判断该日志事件的级别高于日志器本身的级别之后调用LogAppender将日志进行输出，否则该日志被抛弃。
 *
 */
class Logger {
public:
    using ptr = std::shared_ptr<Logger>;
    Logger(std::string const& name = "default");

public:
    void Log(LogEvent::ptr event);

public:
    void AddAppender(LogAppender::ptr appender);

    void DelAppender(LogAppender::ptr appender);

    void ClearAppenders();

public:
    std::string GetName() const { return name_; }

    uint64_t GetCreateTime() const { return create_time_; }

    LogLevel::Level GetLevel() const { return level_; };

    void SetLevel(LogLevel::Level level) { level_ = level; };

private:
    std::mutex mtx_;
    std::string name_;                       // 日志器名称
    LogLevel::Level level_;                  // 日志器级别
    std::list<LogAppender::ptr> appenders_;  // Appender 集合（链表）
    uint64_t create_time_;                   // 创建时间(毫秒)
};

/**
 * @brief 日志事件包装器，方便宏定义，内部包含日志事件和日志器
 */
class LogEventWrap {
public:
    /**
     * @brief 构造函数
     * @param[in] logger 日志器
     * @param[in] event 日志事件
     */
    LogEventWrap(Logger::ptr logger, LogEvent::ptr event);

    /**
     * @brief 析构函数
     * @details 日志事件在析构时由日志器进行输出
     */
    ~LogEventWrap();

public:
    LogEvent::ptr GetLogEvent() const { return event_; }

private:
    Logger::ptr logger_;   // 日志器
    LogEvent::ptr event_;  // 日志事件
};

class LoggerManager {
public:
    LoggerManager();

public:
    void Init();
    Logger::ptr GetLogger(std::string const& name);
    Logger::ptr GetRoot() { return root_; }

private:
    std::mutex mtx_;
    std::map<std::string, Logger::ptr> loggers_;  // 日志器集合
    Logger::ptr root_;                            // root 日志器
};

// ------------------- 继承自 LogAppender -------------------

// 日志输出：标准输出
class StdoutLogAppender : public LogAppender {
public:
    using ptr = std::shared_ptr<StdoutLogAppender>;

    StdoutLogAppender();

public:
    void Log(LogEvent::ptr event) override;
};

// 日志输出：文件
class FileLogAppender : public LogAppender {
public:
    using ptr = std::shared_ptr<FileLogAppender>;

    FileLogAppender(std::string const& filename);

public:
    void Log(LogEvent::ptr event) override;
    bool Reopen();

private:
    std::string filename_;      // 文件路径
    std::ofstream filestream_;  // 文件流
    uint64_t last_time_;        // 上次重打开时间(与当前时间戳比较)
    bool reopen_error_{false};  // 文件打开错误标识
};

// ------------------- 继承自 LogFormatter::FormatItem -------------------

class MessageFormatItem : public LogFormatter::FormatItem {
public:
    MessageFormatItem(const std::string& str) {}
    void Format(std::ostream& os, LogEvent::ptr event) override { os << event->GetContent(); }
};

class LevelFormatItem : public LogFormatter::FormatItem {
public:
    LevelFormatItem(const std::string& str) {}
    void Format(std::ostream& os, LogEvent::ptr event) override {
        os << LogLevel::ToString(event->GetLevel());
    }
};

class ElapseFormatItem : public LogFormatter::FormatItem {
public:
    ElapseFormatItem(const std::string& str) {}
    void Format(std::ostream& os, LogEvent::ptr event) override { os << event->GetElapse(); }
};

class LoggerNameFormatItem : public LogFormatter::FormatItem {
public:
    LoggerNameFormatItem(const std::string& str) {}
    void Format(std::ostream& os, LogEvent::ptr event) override { os << event->GetLoggerName(); }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem {
public:
    ThreadIdFormatItem(const std::string& str) {}
    void Format(std::ostream& os, LogEvent::ptr event) override { os << event->GetThreadId(); }
};

class FiberIdFormatItem : public LogFormatter::FormatItem {
public:
    FiberIdFormatItem(const std::string& str) {}
    void Format(std::ostream& os, LogEvent::ptr event) override { os << event->GetFiberId(); }
};

class ThreadNameFormatItem : public LogFormatter::FormatItem {
public:
    ThreadNameFormatItem(const std::string& str) {}
    void Format(std::ostream& os, LogEvent::ptr event) override { os << event->GetThreadName(); }
};

class DateTimeFormatItem : public LogFormatter::FormatItem {
public:
    DateTimeFormatItem(std::string const& format = "%Y-%m-%d %H:%M:%S") : format_(format) {
        // 防止传入空字符串
        if (format_.empty()) {
            format_ = "%Y-%m-%d %H:%M:%S";
        }
    }

    void Format(std::ostream& os, LogEvent::ptr event) override {
        // 将 UTC 时间转为字符串
        tm tm;
        time_t time = event->GetTime();
        localtime_r(&time, &tm);
        char buf[64];
        strftime(buf, sizeof(buf), format_.c_str(), &tm);
        os << buf;
    }

private:
    std::string format_;
};

class FileNameFormatItem : public LogFormatter::FormatItem {
public:
    FileNameFormatItem(const std::string& str) {}
    void Format(std::ostream& os, LogEvent::ptr event) override { os << event->GetFile(); }
};

class LineFormatItem : public LogFormatter::FormatItem {
public:
    LineFormatItem(const std::string& str) {}
    void Format(std::ostream& os, LogEvent::ptr event) override { os << event->GetLine(); }
};

class NewLineFormatItem : public LogFormatter::FormatItem {
public:
    NewLineFormatItem(const std::string& str) {}
    void Format(std::ostream& os, LogEvent::ptr event) override { os << std::endl; }
};

class StringFormatItem : public LogFormatter::FormatItem {
public:
    StringFormatItem(std::string const& str) : str_(str) {}
    void Format(std::ostream& os, LogEvent::ptr event) override { os << str_; }

private:
    std::string str_;
};

class TabFormatItem : public LogFormatter::FormatItem {
public:
    TabFormatItem(const std::string& str) {}
    void Format(std::ostream& os, LogEvent::ptr event) override { os << "\t"; }
};

class PercentSignFormatItem : public LogFormatter::FormatItem {
public:
    PercentSignFormatItem(const std::string& str) {}
    void Format(std::ostream& os, LogEvent::ptr event) override { os << "%"; }
};

};  // namespace eva
