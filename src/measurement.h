#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "memory.h"

namespace cache_probe {

inline constexpr int kTraversalSteps = 524288;
inline constexpr int kMeasurementRepeats = 7;

void PrepareMeasurements(Node* memory);

std::vector<std::size_t> MakeOffsets(std::size_t count, std::size_t stride);

double MeasureOrderedOffsets(Node* memory,
                             const std::vector<std::size_t>& offsets,
                             int steps = kTraversalSteps,
                             int repeats = kMeasurementRepeats);

double MeasureOffsets(Node* memory, std::vector<std::size_t> offsets,
                      int steps = kTraversalSteps,
                      int repeats = kMeasurementRepeats,
                      std::uint32_t seed = 42);

}
