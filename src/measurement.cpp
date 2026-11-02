#include "measurement.h"

#include <algorithm>
#include <chrono>
#include <random>
#include <stdexcept>
#include <vector>

namespace cache_probe {

Node* last_visited_node = nullptr;

namespace {

double MeasureRingLatency(Node* current_node, int steps, int repeats) {
  std::vector<double> latencies(repeats);

  for (int i = 0; i < steps; ++i) {
    current_node = current_node->next;
  }

  for (int repeat = 0; repeat < repeats; ++repeat) {
    const auto start_time = std::chrono::steady_clock::now();

    for (int i = 0; i < steps; ++i) {
      current_node = current_node->next;
    }

    last_visited_node = current_node;
    const auto finish_time = std::chrono::steady_clock::now();

    const double elapsed_ns =
        std::chrono::duration<double, std::nano>(finish_time - start_time)
            .count();
    latencies[repeat] = elapsed_ns / steps;
  }

  std::sort(latencies.begin(), latencies.end());

  return latencies[latencies.size() / 2];
}

}

void PrepareMeasurements(Node* memory) {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(300);

  while (std::chrono::steady_clock::now() < deadline) {
    MeasureOffsets(memory, MakeOffsets(128, sizeof(Node)));
  }
}

std::vector<std::size_t> MakeOffsets(std::size_t count, std::size_t stride) {
  std::vector<std::size_t> result(count);
  for (std::size_t i = 0; i < count; ++i) {
    result[i] = i * stride;
  }

  return result;
}

double MeasureOffsets(Node* memory, std::vector<std::size_t> offsets, int steps,
                      int repeats) {
  if (memory == nullptr || offsets.empty() || steps <= 0 || repeats <= 0) {
    throw std::invalid_argument(
        "Measurement requires a buffer, offsets, and positive step and repeat "
        "counts.");
  }

  std::mt19937 random_order(42);

  std::shuffle(offsets.begin(), offsets.end(), random_order);
  for (std::size_t i = 0; i < offsets.size(); ++i) {
    const std::size_t next_index = (i + 1) % offsets.size();
    memory[offsets[i] / sizeof(Node)].next =
        memory + (offsets[next_index] / sizeof(Node));
  }

  return MeasureRingLatency(memory + (offsets[0] / sizeof(Node)), steps,
                            repeats);
}

}
