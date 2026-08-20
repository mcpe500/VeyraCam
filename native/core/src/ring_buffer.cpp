#include "veyra/ring_buffer.h"

namespace veyra {

MemoryPool::MemoryPool(size_t poolSize) {
    storage_.resize(poolSize);
    freeList_.reserve(poolSize);
    for (size_t i = 0; i < poolSize; ++i) {
        freeList_.push_back(&storage_[i]);
    }
}

MemoryBlock* MemoryPool::Acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (freeList_.empty()) {
        return nullptr;
    }
    MemoryBlock* block = freeList_.back();
    freeList_.pop_back();
    block->size = 0;
    return block;
}

void MemoryPool::Release(MemoryBlock* block) {
    if (!block) return;
    std::lock_guard<std::mutex> lock(mutex_);
    freeList_.push_back(block);
}

} // namespace veyra
