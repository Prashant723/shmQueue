#pragma once

#include <atomic>

namespace turbo {

struct ShmHeader {
  std::atomic<int64_t> m_tail;
};

}
