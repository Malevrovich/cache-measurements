#include "experiments.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

#include "measurement.h"

namespace cache_probe {
namespace {

constexpr double kConflictOnsetThreshold = 1.2;
constexpr double kConflictConfirmationThreshold = 1.5;
constexpr std::size_t kControlRegionBytes = 4096;
constexpr std::size_t kCapacityStepBytes = 4096;
constexpr std::size_t kCapacityResolutionBytes = 1024;
constexpr double kCapacityThreshold = 1.2;
constexpr std::size_t kMaxCapacityBytes = 1024 * 1024;
constexpr std::size_t kMaxConflictNodes = 64;
constexpr std::size_t kPatternTrials = 5;
constexpr int kProbeSteps = 2097152;
constexpr int kProbeRepeats = 3;
constexpr std::size_t kLineTileBytes = 512;
constexpr std::size_t kMinLineGroups = 1024;
constexpr std::size_t kMaxLineRegionBytes =
    kMaxCapacityBytes / (2 * sizeof(Node)) * kLineTileBytes;

struct LatencyPair {
  double control;
  double measured;
};

LatencyPair MeasurePatternPair(Node* memory,
                               const std::vector<std::size_t>& control_offsets,
                               const std::vector<std::size_t>& measured_offsets,
                               std::uint32_t seed, bool control_first) {
  LatencyPair result{};

  if (control_first) {
    result.control = MeasureOffsets(memory, control_offsets, kProbeSteps,
                                    kProbeRepeats, seed);
    result.measured = MeasureOffsets(memory, measured_offsets, kProbeSteps,
                                     kProbeRepeats, seed);
  } else {
    result.measured = MeasureOffsets(memory, measured_offsets, kProbeSteps,
                                     kProbeRepeats, seed);
    result.control = MeasureOffsets(memory, control_offsets, kProbeSteps,
                                    kProbeRepeats, seed);
  }

  return result;
}

double MeasureCapacityRatio(Node* memory, std::size_t bytes, std::size_t stride,
                            bool debug) {
  const auto control_offsets =
      MakeOffsets(kControlRegionBytes / stride, stride);
  const auto offsets = MakeOffsets(bytes / stride, stride);
  std::array<LatencyPair, kPatternTrials> timings{};
  std::array<double, kPatternTrials> ratios{};

  for (std::size_t trial = 0; trial < kPatternTrials; ++trial) {
    const auto seed = static_cast<std::uint32_t>(trial);
    timings[trial] = MeasurePatternPair(memory, control_offsets, offsets, seed,
                                        trial % 2 == 0);
    ratios[trial] = timings[trial].measured / timings[trial].control;
  }

  if (debug) {
    const char* experiment =
        stride == sizeof(Node) ? "capacity-estimate" : "capacity";
    for (std::size_t trial = 0; trial < kPatternTrials; ++trial) {
      std::cerr << experiment << ',' << bytes << ',' << offsets.size() << ','
                << trial + 1 << ',' << timings[trial].control << ','
                << timings[trial].control << ',' << timings[trial].measured
                << ',' << ratios[trial] << '\n';
    }
  }

  std::sort(ratios.begin(), ratios.end());
  return ratios[ratios.size() / 2];
}

std::size_t FindConflictBoundary(Node* memory, std::size_t stride,
                                 std::size_t max_nodes, std::size_t line_size,
                                 std::size_t trial_group, bool debug) {
  const std::size_t control_region = std::min(stride, kControlRegionBytes);
  const auto baseline_offsets =
      MakeOffsets(kControlRegionBytes / line_size, line_size);
  std::array<double, kPatternTrials> baseline_latencies{};
  for (std::size_t trial = 0; trial < kPatternTrials; ++trial) {
    const auto seed =
        static_cast<std::uint32_t>(trial_group * kPatternTrials + trial);
    baseline_latencies[trial] = MeasureOffsets(
        memory, baseline_offsets, kProbeSteps, kProbeRepeats, seed);
  }

  std::size_t transition_start = 0;
  bool previous_was_slow = false;

  for (std::size_t count = 1; count <= max_nodes; ++count) {
    std::array<LatencyPair, kPatternTrials> timings{};
    std::array<double, kPatternTrials> relative_costs{};

    for (std::size_t trial = 0; trial < kPatternTrials; ++trial) {
      const std::size_t common_offset = (trial * 7 + trial_group * 11) %
                                        (control_region / line_size) *
                                        line_size;
      auto conflict_offsets = MakeOffsets(count, stride);
      auto control_offsets = conflict_offsets;

      for (std::size_t i = 0; i < count; ++i) {
        conflict_offsets[i] += common_offset;
        control_offsets[i] += (common_offset + i * line_size) % control_region;
      }

      const auto seed =
          static_cast<std::uint32_t>(trial_group * kPatternTrials + trial);
      timings[trial] =
          MeasurePatternPair(memory, control_offsets, conflict_offsets, seed,
                             (trial + trial_group) % 2 == 0);
      relative_costs[trial] =
          1.0 + (timings[trial].measured - timings[trial].control) /
                    baseline_latencies[trial];
    }

    if (debug) {
      for (std::size_t trial = 0; trial < kPatternTrials; ++trial) {
        std::cerr << "associativity," << stride << ',' << count << ','
                  << trial_group * kPatternTrials + trial + 1 << ','
                  << baseline_latencies[trial] << ',' << timings[trial].control
                  << ',' << timings[trial].measured << ','
                  << relative_costs[trial] << '\n';
      }
    }

    std::sort(relative_costs.begin(), relative_costs.end());
    const double relative_cost = relative_costs[relative_costs.size() / 2];
    if (relative_cost > kConflictOnsetThreshold) {
      if (transition_start == 0) {
        transition_start = count;
      }
    } else {
      transition_start = 0;
    }

    const bool is_slow = relative_cost > kConflictConfirmationThreshold;
    if (is_slow && previous_was_slow) {
      return transition_start - 1;
    }

    previous_was_slow = is_slow;
  }

  return 0;
}

}

std::size_t RequiredMemoryBytes() {
  const std::size_t conflict_bytes = kMaxCapacityBytes * kMaxConflictNodes;
  return std::max(conflict_bytes, kMaxLineRegionBytes);
}

std::size_t MeasureCacheCapacity(Node* memory, std::size_t stride, bool debug) {
  if (stride < sizeof(Node) || stride > kCapacityResolutionBytes ||
      kCapacityResolutionBytes % stride != 0) {
    throw std::invalid_argument(
        "Invalid sampling stride for the capacity scan.");
  }

  std::size_t boundary_bytes = 0;
  int consecutive_slow = 0;

  for (std::size_t bytes = kCapacityStepBytes; bytes <= kMaxCapacityBytes;
       bytes += kCapacityStepBytes) {
    if (MeasureCapacityRatio(memory, bytes, stride, debug) >
        kCapacityThreshold) {
      if (consecutive_slow == 0) {
        boundary_bytes = bytes - kCapacityStepBytes;
      }
      if (++consecutive_slow == 3 && boundary_bytes != 0) {
        std::size_t fast_bytes = boundary_bytes;
        std::size_t slow_bytes = boundary_bytes + kCapacityStepBytes;

        while (slow_bytes - fast_bytes > kCapacityResolutionBytes) {
          const std::size_t middle_bytes =
              fast_bytes + (slow_bytes - fast_bytes) / 2;
          if (MeasureCapacityRatio(memory, middle_bytes, stride, debug) >
              kCapacityThreshold) {
            slow_bytes = middle_bytes;
          } else {
            fast_bytes = middle_bytes;
          }
        }

        return fast_bytes;
      }
    } else {
      consecutive_slow = 0;
    }
  }

  throw std::runtime_error("Could not find a stable L1 capacity boundary.");
}

std::size_t MeasureCacheAssociativity(Node* memory, std::size_t capacity_bytes,
                                      std::size_t line_size, bool debug) {
  if (capacity_bytes < kCapacityStepBytes ||
      capacity_bytes > kMaxCapacityBytes ||
      capacity_bytes % kCapacityResolutionBytes != 0 ||
      line_size < sizeof(Node) || line_size > kCapacityResolutionBytes ||
      kCapacityResolutionBytes % line_size != 0) {
    throw std::invalid_argument(
        "Invalid cache dimensions for the associativity scan.");
  }

  std::array<std::size_t, 3> candidates{};

  for (std::size_t attempt = 0; attempt < candidates.size(); ++attempt) {
    for (std::size_t ways = 1; ways + 3 <= kMaxConflictNodes; ++ways) {
      if (capacity_bytes % (ways * line_size) != 0) {
        continue;
      }

      const std::size_t stride = capacity_bytes / ways;
      const std::size_t control_region = std::min(stride, kControlRegionBytes);
      if (stride % control_region != 0 ||
          kControlRegionBytes % control_region != 0) {
        continue;
      }

      const std::size_t trial_group = attempt * 2;
      if (FindConflictBoundary(memory, stride, ways + 3, line_size, trial_group,
                               debug) != ways ||
          FindConflictBoundary(memory, stride * 2, ways + 3, line_size,
                               trial_group + 1, debug) != ways) {
        continue;
      }

      candidates[attempt] = ways;
      if (std::count(candidates.begin(), candidates.end(), ways) >= 2) {
        return ways;
      }
      break;
    }
  }

  throw std::runtime_error(
      "Could not reproduce the L1 associativity boundary.");
}

std::size_t MeasureCacheLineSize(Node* memory, std::size_t estimated_capacity,
                                 bool debug) {
  if (estimated_capacity < kControlRegionBytes ||
      estimated_capacity > kMaxCapacityBytes) {
    throw std::invalid_argument(
        "Invalid capacity estimate for the line-size scan.");
  }

  constexpr std::array<std::size_t, 6> kOffsets = {8, 16, 32, 64, 128, 256};
  const std::size_t group_count =
      std::max(kMinLineGroups, estimated_capacity / (2 * sizeof(Node)));

  std::vector<std::size_t> groups(group_count);
  std::iota(groups.begin(), groups.end(), 0);
  std::array<std::size_t, kOffsets.size()> tests{};
  std::iota(tests.begin(), tests.end(), 0);
  std::array<std::array<double, kPatternTrials>, kOffsets.size()> timings{};

  for (std::size_t trial = 0; trial < kPatternTrials; ++trial) {
    std::mt19937 random_order(static_cast<std::uint32_t>(trial));
    std::shuffle(groups.begin(), groups.end(), random_order);
    std::shuffle(tests.begin(), tests.end(), random_order);

    for (const auto test : tests) {
      std::vector<std::size_t> offsets;
      offsets.reserve(group_count * 2);
      for (const auto group : groups) {
        offsets.push_back(group * kLineTileBytes);
        offsets.push_back(group * kLineTileBytes + kOffsets[test]);
      }

      timings[test][trial] =
          MeasureOrderedOffsets(memory, offsets, kProbeSteps, kProbeRepeats);
    }
  }

  std::array<double, kOffsets.size()> latencies{};
  for (std::size_t test = 0; test < kOffsets.size(); ++test) {
    if (debug) {
      for (std::size_t trial = 0; trial < kPatternTrials; ++trial) {
        const double baseline = timings[0][trial];
        std::cerr << "line," << kOffsets[test] << ',' << group_count * 2 << ','
                  << trial + 1 << ',' << baseline << ',' << baseline << ','
                  << timings[test][trial] << ','
                  << timings[test][trial] / baseline << '\n';
      }
    }

    auto samples = timings[test];
    std::sort(samples.begin(), samples.end());
    latencies[test] = samples[samples.size() / 2];
  }

  std::size_t candidate = 1;
  for (std::size_t test = 2; test < kOffsets.size(); ++test) {
    if (latencies[test] / latencies[test - 1] >
        latencies[candidate] / latencies[candidate - 1]) {
      candidate = test;
    }
  }

  const double before_jump = latencies[candidate - 1];
  if (latencies[candidate] < before_jump * 1.15 ||
      !std::all_of(
          latencies.begin() + candidate, latencies.end(),
          [before_jump](double latency) {
            return latency > before_jump * 1.10;
          })) {
    throw std::runtime_error(
        "Could not find a stable L1 line-size transition.");
  }

  int confirming_trials = 0;
  for (std::size_t trial = 0; trial < kPatternTrials; ++trial) {
    if (timings[candidate][trial] > timings[candidate - 1][trial] * 1.10) {
      ++confirming_trials;
    }
  }
  if (confirming_trials < 4) {
    throw std::runtime_error(
        "Could not reproduce the L1 line-size transition.");
  }

  return kOffsets[candidate];
}

}
