#pragma once

#include <cstddef>

#include "memory.h"

namespace cache_probe {

std::size_t RequiredMemoryBytes();

std::size_t MeasureCacheCapacity(Node* memory, std::size_t stride,
                                 bool debug = false);

std::size_t MeasureCacheAssociativity(Node* memory, std::size_t capacity_bytes,
                                      std::size_t line_size,
                                      bool debug = false);

std::size_t MeasureCacheLineSize(Node* memory, std::size_t estimated_capacity,
                                 bool debug = false);

}
