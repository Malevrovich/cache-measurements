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

    const auto geometry = cache_probe::MeasureCacheGeometry(memory, debug);
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
