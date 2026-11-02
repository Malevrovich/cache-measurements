#include "experiments.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "measurement.h"

namespace cache_probe {
namespace {

constexpr double kTransitionThreshold = 1.5;
constexpr std::size_t kMaxConflictStride = 1024 * 1024;
constexpr std::size_t kMaxConflictNodes = 64;
constexpr int kConfirmingStrides = 3;
constexpr std::size_t kMinCacheStride = 4 * sizeof(Node);
constexpr std::size_t kMaxCacheStride = 1024;
constexpr std::size_t kMaxCacheNodes = 8192;

std::size_t FindWorkingSetBoundary(Node* memory, std::size_t stride,
                                   std::size_t max_nodes) {
  constexpr std::size_t kBaselineSamples = 5;
  std::vector<double> baseline;
  double threshold = 0;
  std::size_t previous_count = 0;
  std::size_t boundary = 0;
  int consecutive_slow = 0;

  for (std::size_t count = 4; count <= max_nodes;
       count += count < 256 ? 4 : (count < 1024 ? 16 : 64)) {
    const double latency =
        MeasureOffsets(memory, MakeOffsets(count, stride), 65536, 3);

    if (baseline.size() < kBaselineSamples) {
      baseline.push_back(latency);
      if (baseline.size() == kBaselineSamples) {
        std::sort(baseline.begin(), baseline.end());
        threshold = baseline[baseline.size() / 2] * 1.35;
      }

      previous_count = count;
      continue;
    }

    if (latency > threshold) {
      if (consecutive_slow == 0) {
        boundary = previous_count;
      }
      if (++consecutive_slow == 3) {
        return boundary;
      }
    } else {
      consecutive_slow = 0;
    }

    previous_count = count;
  }

  return 0;
}

}

std::size_t RequiredMemoryBytes() {
  const std::size_t conflict_bytes = kMaxConflictStride * kMaxConflictNodes;
  const std::size_t line_bytes =
      (kMaxCacheStride + kMaxCacheStride / 2) * kMaxCacheNodes;

  return std::max(conflict_bytes, line_bytes);
}

CacheGeometry MeasureCacheGeometry(Node* memory) {
  CacheGeometry candidate{};
  int confirming_strides = 0;

  for (std::size_t stride = sizeof(Node); stride <= kMaxConflictStride;
       stride *= 2) {
    std::size_t first_conflicting_count = 0;
    bool previous_was_slow = false;

    for (std::size_t count = 1; count <= kMaxConflictNodes; ++count) {
      const double control_latency =
          MeasureOffsets(memory, MakeOffsets(count, sizeof(Node)));
      const double strided_latency =
          MeasureOffsets(memory, MakeOffsets(count, stride));

      const bool is_slow =
          strided_latency / control_latency > kTransitionThreshold;
      if (is_slow && previous_was_slow) {
        first_conflicting_count = count - 1;
        break;
      }

      previous_was_slow = is_slow;
    }

    if (first_conflicting_count < 2) {
      candidate = {};
      confirming_strides = 0;
      continue;
    }

    const auto nodes_before_conflict = first_conflicting_count - 1;

    if (candidate.associativity == nodes_before_conflict) {
      ++confirming_strides;
    } else {
      candidate = {nodes_before_conflict, stride};
      confirming_strides = 1;
    }

    if (confirming_strides == kConfirmingStrides) {
      return candidate;
    }
  }

  throw std::runtime_error("Could not find a stable L1 conflict boundary.");
}

std::size_t MeasureCacheLineSize(Node* memory) {
  std::size_t block_size = 0;
  std::size_t candidate_block_size = 0;
  std::size_t last_decrease_stride = 0;

  for (std::size_t high_stride = kMinCacheStride;
       high_stride <= kMaxCacheStride; high_stride *= 2) {
    const auto original_boundary =
        FindWorkingSetBoundary(memory, high_stride, kMaxCacheNodes);
    const auto quarter_boundary = FindWorkingSetBoundary(
        memory, high_stride + high_stride / 4, kMaxCacheNodes);
    const auto half_boundary = FindWorkingSetBoundary(
        memory, high_stride + high_stride / 2, kMaxCacheNodes);

    if (original_boundary == 0 || quarter_boundary == 0 || half_boundary == 0) {
      candidate_block_size = 0;
      continue;
    }

    const bool decreased = quarter_boundary < original_boundary * 0.90 &&
                           half_boundary < original_boundary * 0.85;
    const bool increased = quarter_boundary > original_boundary * 1.25 &&
                           half_boundary > original_boundary * 1.25;

    if (decreased) {
      last_decrease_stride = high_stride;
      candidate_block_size = 0;
    } else if (increased && last_decrease_stride != 0) {
      if (high_stride == candidate_block_size * 4) {
        block_size = candidate_block_size;
        break;
      }

      const auto candidate = high_stride / 2;
      if (candidate >= last_decrease_stride &&
          candidate <= 2 * last_decrease_stride) {
        candidate_block_size = candidate;
      } else {
        candidate_block_size = 0;
      }
    } else {
      candidate_block_size = 0;
    }
  }

  if (block_size == 0) {
    throw std::runtime_error(
        "Could not determine the line size from boundary transitions.");
  }

  if (block_size < 32 || block_size > 256) {
    throw std::runtime_error("Line size is outside the experiment range.");
  }

  return block_size;
}

}
