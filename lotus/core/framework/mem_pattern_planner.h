#pragma once
#include "core/framework/mem_pattern.h"
#include "core/framework/allocation_planner.h"
#include <list>

namespace Lotus {
// MemPatternPlanner is used to trace allocation/free steps
// in a single iteration, record the pattern and cached for
// future request if they have the same input shape.
class MemPatternPlanner {
 public:
  MemPatternPlanner()  {}

  void TraceAllocation(int ml_value_idx, size_t size) {
    if (size == 0) {
      allocs_.emplace_back(ml_value_idx, MemoryBlock(0, 0));
      return;
    }

    size_t current = 0;
    size_t waste_bytes = std::numeric_limits<size_t>::max();
    size_t best_offset = 0;
    if (!blocks_.empty()) {
      auto last_block = allocs_[*blocks_.rbegin()];
      best_offset = last_block.block_.offset_ + last_block.block_.size_;
    }

    std::list<int>::iterator best_fit_it = blocks_.end();
    for (auto it = blocks_.begin(); it != blocks_.end(); it++) {
      if (allocs_[*it].block_.offset_ >= current) {
        auto gap = allocs_[*it].block_.offset_ - current;
        if (gap >= size && (gap - size) < waste_bytes) {
          best_fit_it = it;
          waste_bytes = gap - size;
          best_offset = current;
        }
      }
      current = allocs_[*it].block_.offset_ + allocs_[*it].block_.size_;
    }

    allocs_.emplace_back(ml_value_idx, MemoryBlock(best_offset, size));
    buffer_size = std::max(buffer_size, best_offset + size);
    blocks_.insert(best_fit_it, (static_cast<int>(allocs_.size()) - 1));
  }

  void TraceFree(int ml_value_index) {
    for (auto it = blocks_.begin(); it != blocks_.end(); it++) {
      if (allocs_[*it].index_ == ml_value_index) {
        blocks_.erase(it);
        break;
      }
    }
  }

  Status GenerateMemPattern(MemoryPattern* out) {
    if (!out)
      return Status(Lotus::Common::LOTUS, Lotus::Common::INVALID_ARGUMENT);
    out->peak_size_ = buffer_size;
    for (auto & alloc : allocs_) {
      out->patterns_[alloc.index_] = alloc.block_;
    }

    return Status::OK();
  }

 private:
  struct MLValueAllocationBlock {
    int index_{-1};
    MemoryBlock block_;

    MLValueAllocationBlock()  {}
    MLValueAllocationBlock(int index, MemoryBlock block) : index_(index), block_(block) {}
  };

  std::vector<MLValueAllocationBlock> allocs_;
  // blocks_ the list of currently allocated memory blocks, sorted in order of their offset
  std::list<int> blocks_;
  size_t buffer_size{0};
};

}  // namespace Lotus
