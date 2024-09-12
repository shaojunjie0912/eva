#pragma once

#include <iostream>
#include <memory>
#include <mutex>

namespace eva {

template <typename T>
class Singleton {
    // protected:
private:
    Singleton() = default;
    Singleton(Singleton<T> const&) = delete;
    Singleton& operator=(Singleton<T> const&) = delete;

    inline static std::shared_ptr<T> instance_{nullptr};

public:
    ~Singleton() = default;
    static std::shared_ptr<T> GetInstance() {
        instance_ = std::shared_ptr<T>{new T};
        return instance_;
    }
};
}  // namespace eva
