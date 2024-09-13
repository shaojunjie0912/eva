#include <util/util.h>

namespace eva {

uint64_t GetElapsedMS() {
    struct timespec ts = {0};
    // CLOCK_MONOTONIC_RAW 时钟不受NTP(网络时间协议)或其他系统时间调整影响
    // 用于精确的时间测量
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

pid_t GetThreadId() { return syscall(SYS_gettid); }

uint64_t GetFiberId() { return Fiber::GetFiberId(); }

std::string GetThreadName() {
    char thread_name[16] = {0};
    pthread_getname_np(pthread_self(), thread_name, 16);
    return std::string(thread_name);
}

}  // namespace eva
