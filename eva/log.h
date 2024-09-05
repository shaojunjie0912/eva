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

// 在想要不要把跟时间相关的都转为 chrono?, 目前是 ctime

// 为什么新版在 LogAppender.Log 中删除了 LogLevel::Level level

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

    // static LogLevel::Level FromString
};

// 日志事件
class LogEvent {
public:
    using ptr = std::shared_ptr<LogEvent>;

public:
    LogEvent(const std::string& logger_name, LogLevel::Level level, const char* file, int32_t line,
             int64_t elapse, uint32_t thread_id, uint64_t fiber_id, time_t time,
             const std::string& thread_name);

public:
    // Get 方法
    LogLevel::Level GetLevel() const { return level_; }

    std::string GetContent() const { return content_; }

    char const* GetFile() const { return file_; }

    int32_t GetLine() const { return line_; }

    int64_t GetElapse() const { return elapse_; }

    uint32_t GetThreadId() const { return thread_id_; }

    uint32_t GetFiberId() const { return fiber_id_; }

    time_t GetTime() const { return time_; }

    std::string GetThreadName() const { return thread_name_; }

    std::string GetLoggerName() const { return logger_name_; }

private:
    // 内存对齐
    LogLevel::Level level_;      // 日志级别
    const char* file_{nullptr};  // 文件名
    int32_t line_{0};            // 行号
    uint32_t elapse_{0};         // 程序启动开始到现在的毫秒数
    uint32_t thread_id_{0};      // 线程 id
    uint32_t fiber_id_{0};       // 协程 id
    time_t time_{0};             // 时间戳
    std::string content_;        // 日志内容
    std::string thread_name_;    // 线程名称
    std::string logger_name_;    // 日志器名称
};

// 日志格式器
class LogFormatter {
public:
    using ptr = std::shared_ptr<LogFormatter>;
    LogFormatter(std::string const& pattern =
                     "%d{%Y-%m-%d %H:%M:%S} [%rms]%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n");

public:
    std::string Format(LogEvent::ptr event);
    void Init();

    class FormatItem {
    public:
        using ptr = std::shared_ptr<FormatItem>;
        virtual ~FormatItem() {}

    public:
        virtual void Format(std::ostream& os, LogEvent::ptr event) = 0;
    };

private:
    std::string pattern_;
    std::vector<FormatItem::ptr> items_;
    bool error_;
};

// ------------------- 各种各样的 FormatItem -------------------
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
// ------------------- 各种各样的 FormatItem -------------------

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

    LogFormatter::ptr GetFormatter() const { return formatter_; }

    /**
     * @brief 设置日志格式器
     */
    void SetFormatter(LogFormatter::ptr formatter) { formatter_ = formatter; };

protected:
    // LogLevel::Level level_;
    std::mutex mtx_;
    LogFormatter::ptr formatter_;          // 日志格式器
    LogFormatter::ptr default_formatter_;  // 默认日志格式器
};

// 日志输出：标准输出
class StdoutLogAppender : public LogAppender {
public:
    using ptr = std::shared_ptr<StdoutLogAppender>;
    StdoutLogAppender() : LogAppender(LogFormatter::ptr{new LogFormatter}) {}

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
    std::string filename_;
    std::ofstream filestream_;
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
    Logger(std::string const& name = "rooot");

public:
    void Log(LogLevel::Level level, LogEvent::ptr event);

public:
    void Debug(LogEvent::ptr event);
    void Info(LogEvent::ptr event);
    void Warn(LogEvent::ptr event);
    void Error(LogEvent::ptr event);
    void Fatal(LogEvent::ptr event);

public:
    void AddAppender(LogAppender::ptr appender);
    void DelAppender(LogAppender::ptr appender);

public:
    LogLevel::Level GetLevel() const { return level_; };

    void SetLevel(LogLevel::Level level) { level_ = level; };

private:
    std::string name_;                       // 日志名称
    LogLevel::Level level_;                  // 日志级别
    std::list<LogAppender::ptr> appenders_;  // Appender 集合（链表）
};

};  // namespace eva
