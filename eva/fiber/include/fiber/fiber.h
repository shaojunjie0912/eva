#pragma once

#include <cstdint>

namespace eva {

class Fiber {
public:
    static uint64_t GetFiberId();
};

}  // namespace eva
