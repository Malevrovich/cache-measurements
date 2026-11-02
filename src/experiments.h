#pragma once

#include <cstddef>

#include "memory.h"

namespace cache_probe {

struct CacheGeometry {
  std::size_t associativity;
  std::size_t conflict_stride;
};

std::size_t RequiredMemoryBytes();

CacheGeometry MeasureCacheGeometry(Node* memory, bool debug = false);

std::size_t MeasureCacheLineSize(Node* memory);

}
