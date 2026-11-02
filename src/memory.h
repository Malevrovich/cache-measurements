#pragma once

#include <cstddef>
#include <memory>

namespace cache_probe {

struct Node {
  Node* next = nullptr;
};

class AlignedBuffer {
 public:
  explicit AlignedBuffer(std::size_t byte_count);

  AlignedBuffer(const AlignedBuffer&) = delete;
  AlignedBuffer& operator=(const AlignedBuffer&) = delete;

  Node* data() const { return aligned_data_; }

 private:
  std::unique_ptr<Node[]> allocation_;
  Node* aligned_data_ = nullptr;
};

}
