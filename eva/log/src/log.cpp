#include <log/log.h>

#include <cstdint>
#include <fstream>
#include <iostream>

namespace eva {

// ---------------- LogEvent 类 ----------------
LogEvent::LogEvent(const std::string& logger_name, LogLevel::Level level, const char* file,
                   int32_t line, int64_t elapse, uint32_t thread_id, uint64_t fiber_id,
                   uint64_t time, const std::string& thread_name)
    : level_(level),
      file_(file),
      line_(line),
      elapse_(elapse),
      thread_id_(thread_id),
      fiber_id_(fiber_id),
      time_(time),
      thread_name_(thread_name),
      logger_name_(logger_name) {}

void LogEvent::Printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

// ---------------- LogFormatter 类 ----------------

LogFormatter::LogFormatter(std::string const& pattern) : pattern_(pattern) { Init(); }

void LogFormatter::Init() {
    // 按顺序存储解析到的pattern项
    // 每个pattern包括一个整数类型和一个字符串，类型为0表示该pattern是常规字符串，为1表示该pattern需要转义
    // 日期格式单独用下面的dataformat存储
    std::vector<std::pair<int, std::string>> patterns;
    // 临时存储常规字符串
    std::string tmp;
    // 日期格式字符串，默认把位于%d后面的大括号对里的全部字符都当作格式字符，不校验格式是否合法
    std::string dateformat;
    // 是否解析出错
    bool error = false;

    // 是否正在解析常规字符，初始时为true
    bool parsing_string = true;

    size_t i = 0;
    while (i < pattern_.size()) {
        std::string c = std::string(1, pattern_[i]);
        if (c == "%") {
            if (parsing_string) {
                if (!tmp.empty()) {
                    patterns.push_back(std::make_pair(0, tmp));
                }
                tmp.clear();
                parsing_string = false;  // 在解析常规字符时遇到%，表示开始解析模板字符
                i++;
                continue;
            } else {
                patterns.push_back(std::make_pair(1, c));
                parsing_string = true;  // 在解析模板字符时遇到%，表示这里是一个%转义
                i++;
                continue;
            }
        } else {                   // not %
            if (parsing_string) {  // 持续解析常规字符直到遇到%，解析出的字符串作为一个常规字符串加入patterns
                tmp += c;
                i++;
                continue;
            } else {  // 模板字符，直接添加到patterns中，添加完成后，状态变为解析常规字符，%d特殊处理
                patterns.push_back(std::make_pair(1, c));
                parsing_string = true;

                // 后面是对%d的特殊处理，如果%d后面直接跟了一对大括号，那么把大括号里面的内容提取出来作为dateformat
                if (c != "d") {
                    i++;
                    continue;
                }
                i++;
                if (i < pattern_.size() && pattern_[i] != '{') {
                    continue;
                }
                i++;
                while (i < pattern_.size() && pattern_[i] != '}') {
                    dateformat.push_back(pattern_[i]);
                    i++;
                }
                if (pattern_[i] != '}') {
                    // %d后面的大括号没有闭合，直接报错
                    std::cout << "[ERROR] LogFormatter::init() " << "pattern: [" << pattern_
                              << "] '{' not closed" << std::endl;
                    error = true;
                    break;
                }
                i++;
                continue;
            }
        }
    }

    if (error) {
        error_ = true;
        return;
    }

    // 模板解析结束之后剩余的常规字符也要算进去
    if (!tmp.empty()) {
        patterns.push_back(std::make_pair(0, tmp));
        tmp.clear();
    }

    static std::map<std::string, std::function<FormatItem::ptr(std::string const& str)>>
        s_format_items = {
#define XX(str, C) {#str, [](std::string const& fmt) { return FormatItem::ptr(new C(fmt)); }}
            XX(m, MessageFormatItem),      // m:消息
            XX(p, LevelFormatItem),        // p:日志级别
            XX(c, LoggerNameFormatItem),   // c:日志器名称
            XX(r, ElapseFormatItem),       // r:累计毫秒数
            XX(f, FileNameFormatItem),     // f:文件名
            XX(l, LineFormatItem),         // l:行号
            XX(t, ThreadIdFormatItem),     // t:编程号
            XX(F, FiberIdFormatItem),      // F:协程号
            XX(N, ThreadNameFormatItem),   // N:线程名称
            XX(%, PercentSignFormatItem),  // %:百分号
            XX(T, TabFormatItem),          // T:制表符
            XX(n, NewLineFormatItem),      // n:换行符
#undef XX
        };

    for (auto& v : patterns) {
        if (v.first == 0) {
            items_.push_back(FormatItem::ptr(new StringFormatItem(v.second)));
        } else if (v.second == "d") {
            // d: 日期时间
            items_.push_back(FormatItem::ptr(new DateTimeFormatItem(dateformat)));
        } else {
            auto it = s_format_items.find(v.second);
            if (it == s_format_items.end()) {
                std::cout << "[ERROR] LogFormatter::init() " << "pattern: [" << pattern_ << "] "
                          << "unknown format item: " << v.second << std::endl;
                error = true;
                break;
            } else {
                items_.push_back(it->second(v.second));
            }
        }
    }

    if (error) {
        error_ = true;
        return;
    }
}

std::string LogFormatter::Format(LogEvent::ptr event) {
    std::stringstream ss;
    for (auto const& item : items_) {
        item->Format(ss, event);
    }
    return ss.str();
}

std::ostream& LogFormatter::Format(std::ostream& os, LogEvent::ptr event) {
    for (auto const& item : items_) {
        item->Format(os, event);
    }
    return os;
}

// ---------------- LogAppender 类 ----------------

LogAppender::LogAppender(LogFormatter::ptr default_formatter)
    : default_formatter_(default_formatter) {}

// ---------------- StdoutLogAppender 类 ----------------

// 基类 LogAppender 构造 -> 派生类 StdoutLogAppender 构造
// 因为基类没有默认构造函数，必须显示构造
// 用一个默认构造函数的 LogFormatter 智能指针构造一个 LogAppender 基类对象
//  Constructor for 'eva::StdoutLogAppender' must explicitly initialize the base class 'LogAppender'
//  which does not have a default constructor
StdoutLogAppender::StdoutLogAppender() : LogAppender(LogFormatter::ptr{new LogFormatter}) {}

void StdoutLogAppender::Log(LogEvent::ptr event) {
    if (formatter_) {
        formatter_->Format(std::cout, event);
    } else {
        default_formatter_->Format(std::cout, event);
    }
}

// ---------------- FileLogAppender 类 ----------------

// LogFormatter 空构造函数意味着 default_formatter
FileLogAppender::FileLogAppender(std::string const& filename)
    : LogAppender(LogFormatter::ptr{new LogFormatter}), filename_(filename) {
    Reopen();  // 重新打开？
    if (reopen_error_) {
        std::cout << "reopen file " << filename_ << " error" << std::endl;
    }
}

void FileLogAppender::Log(LogEvent::ptr event) {
    uint64_t now = event->GetTime();
    // 如果一个日志事件距离上次写日志超过3秒，那就重新打开一次日志文件
    if (now >= last_time_ + 3) {
        Reopen();
        if (reopen_error_) {
            std::cout << "reopen file " << filename_ << " error" << std::endl;
        }
        last_time_ = now;
    }
    if (reopen_error_) {
        return;
    }
    // 这里的🔒不确定
    std::lock_guard lk{mtx_};

    // 如果有自定义的 formatter
    if (formatter_) {
        if (!formatter_->Format(filestream_, event)) {
            std::cout << "[ERROR] FileLogAppender::log() format error" << std::endl;
        }
    }
    // 默认 default_formatter
    else {
        if (!default_formatter_->Format(filestream_, event)) {
            std::cout << "[ERROR] FileLogAppender::log() format error" << std::endl;
        }
    }
}

bool FileLogAppender::Reopen() {
    std::lock_guard lk{mtx_};
    if (filestream_) {
        filestream_.close();
    }
    filestream_.open(filename_, std::ios::app);
    reopen_error_ = !filestream_;
    return !reopen_error_;
}

// ---------------- Logger 类 ----------------

// TODO: 这里 create_time 后续再添加
// 日志器的默认等级 INFO
Logger::Logger(std::string const& name)
    : name_(name), level_(LogLevel::Level::INFO), create_time_(GetElapsedMS()) {}

void Logger::AddAppender(LogAppender::ptr appender) {
    std::lock_guard lk{mtx_};  // NOTE: 加锁
    appenders_.push_back(appender);
}

void Logger::DelAppender(LogAppender::ptr appender) {
    std::lock_guard lk{mtx_};  // NOTE: 加锁
    if (auto it{std::find(appenders_.begin(), appenders_.end(), appender)};
        it != appenders_.end()) {
        appenders_.erase(it);
    }
}

void Logger::ClearAppenders() {
    std::lock_guard lk{mtx_};  // NOTE: 加锁
    appenders_.clear();
}

/**
 * 调用Logger的所有appenders将日志写一遍，
 * Logger至少要有一个appender，否则没有输出
 */
void Logger::Log(LogEvent::ptr event) {
    if (event->GetLevel() >= level_) {
        for (auto const& appender : appenders_) {
            appender->Log(event);
        }
    }
}

// ---------------- LogEventWrap 类 ----------------

LogEventWrap::LogEventWrap(Logger::ptr logger, LogEvent::ptr event)
    : logger_(logger), event_(event) {}

// NOTE: LogEventWrap 在析构时写日志
LogEventWrap::~LogEventWrap() { logger_->Log(event_); }

// ---------------- LoggerManager 类 ----------------
LoggerManager::LoggerManager() {
    // 默认创建一个 root 日志器
    root_.reset(new Logger{"root"});
    // 为默认日志器创建一个 StdoutLogAppender
    root_->AddAppender(LogAppender::ptr{new StdoutLogAppender});
    loggers_[root_->GetName()] = root_;
    Init();
}

// TODO: 没写 LoggerManager::Init
void LoggerManager::Init() {}

/**
 * 如果指定名称的日志器未找到，那会就新创建一个，但是新创建的Logger是不带Appender的，
 * 需要手动添加Appender
 */
Logger::ptr LoggerManager::GetLogger(std::string const& name) {
    std::lock_guard lk{mtx_};  // NOTE: 加锁
    if (loggers_.count(name)) {
        return loggers_[name];
    }
    Logger::ptr logger{new Logger{name}};
    loggers_[name] = logger;
    return logger;
}

}  // namespace eva
