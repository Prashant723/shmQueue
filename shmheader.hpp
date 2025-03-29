#pragma once

#include <atomic>


struct ShmHeader {
  std::atomic<int64_t> m_tail;
};

