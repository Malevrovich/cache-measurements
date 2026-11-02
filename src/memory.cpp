#include "memory.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>

namespace cache_probe {

AlignedBuffer::AlignedBuffer(std::size_t byte_count) {
  constexpr std::size_t kAlignment = 1024 * 1024;
  static_assert(kAlignment % sizeof(Node) == 0);

  constexpr std::size_t kExtraNodes = kAlignment / sizeof(Node);
  constexpr std::size_t kMaxNodes =
      std::numeric_limits<std::size_t>::max() / sizeof(Node);
  if (byte_count > (kMaxNodes - kExtraNodes) * sizeof(Node)) {
    throw std::length_error("Buffer size is too large.");
  }

  const std::size_t node_count = (byte_count + sizeof(Node) - 1) / sizeof(Node);
  allocation_ = std::make_unique<Node[]>(node_count + kExtraNodes);

  const auto address = reinterpret_cast<std::uintptr_t>(allocation_.get());
  const std::size_t remainder = address % kAlignment;
  const std::size_t padding_bytes = (kAlignment - remainder) % kAlignment;

  aligned_data_ = allocation_.get() + padding_bytes / sizeof(Node);
}

}
