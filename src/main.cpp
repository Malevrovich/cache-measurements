#include <exception>
#include <iostream>
#include <string_view>

#include "experiments.h"
#include "measurement.h"
#include "memory.h"

int main(int argc, char* argv[]) {
  if (argc > 2 || (argc == 2 && std::string_view(argv[1]) != "--debug")) {
    std::cerr << "Usage: " << argv[0] << " [--debug]\n";
    return 1;
  }

  const bool debug = argc == 2;

  try {
    cache_probe::AlignedBuffer buffer(cache_probe::RequiredMemoryBytes());
    cache_probe::Node* memory = buffer.data();
    cache_probe::PrepareMeasurements(memory);

    if (debug) {
      std::cerr << "experiment,bytes,n,trial,baseline_ns,control_ns,test_ns,"
                   "relative_cost\n";
    }

    const auto estimated_capacity = cache_probe::MeasureCacheCapacity(
        memory, sizeof(cache_probe::Node), debug);
    const auto line_size =
        cache_probe::MeasureCacheLineSize(memory, estimated_capacity, debug);
    const auto capacity_bytes =
        cache_probe::MeasureCacheCapacity(memory, line_size, debug);
    const auto associativity = cache_probe::MeasureCacheAssociativity(
        memory, capacity_bytes, line_size, debug);

    std::cout << "L1D capacity: " << capacity_bytes / 1024 << " KiB\n"
              << "L1 line size: " << line_size << " bytes\n"
              << "Associativity: " << associativity << '\n';

    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
