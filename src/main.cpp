#include <exception>
#include <iostream>

#include "experiments.h"
#include "measurement.h"
#include "memory.h"

int main() {
  try {
    cache_probe::AlignedBuffer buffer(cache_probe::RequiredMemoryBytes());
    cache_probe::Node* memory = buffer.data();
    cache_probe::PrepareMeasurements(memory);

    const auto geometry = cache_probe::MeasureCacheGeometry(memory);
    const auto capacity_bytes =
        geometry.associativity * geometry.conflict_stride;
    const auto line_size = cache_probe::MeasureCacheLineSize(memory);

    std::cout << "L1D capacity: " << capacity_bytes / 1024 << " KiB\n"
              << "L1 line size: " << line_size << " bytes\n"
              << "Associativity: " << geometry.associativity << '\n';

    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
