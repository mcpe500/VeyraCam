#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <optional>

namespace veyra {

template <typename T, size_t Capacity>
class LockFreeRingBuffer {
public:
    LockFreeRingBuffer() : head_(0), tail_(0), droppedCount_(0) {
        buffer_.resize(Capacity);
    }

    ~LockFreeRingBuffer() = default;

    // Push item into ring buffer. If full, drops oldest item (stale drop policy)
    bool Push(const T& item) {
        size_t currentTail = tail_.load(std::memory_order_relaxed);
        size_t currentHead = head_.load(std::memory_order_acquire);
        size_t nextTail = (currentTail + 1) % Capacity;

        if (nextTail == currentHead) {
            // Buffer is full -> Drop oldest item (advance head)
            head_.store((currentHead + 1) % Capacity, std::memory_order_release);
            droppedCount_.fetch_add(1, std::memory_order_relaxed);
        }

        buffer_[currentTail] = item;
        tail_.store(nextTail, std::memory_order_release);
        return true;
    }

    // Move push
    bool Push(T&& item) {
        size_t currentTail = tail_.load(std::memory_order_relaxed);
        size_t currentHead = head_.load(std::memory_order_acquire);
        size_t nextTail = (currentTail + 1) % Capacity;

        if (nextTail == currentHead) {
            head_.store((currentHead + 1) % Capacity, std::memory_order_release);
            droppedCount_.fetch_add(1, std::memory_order_relaxed);
        }

        buffer_[currentTail] = std::move(item);
        tail_.store(nextTail, std::memory_order_release);
        return true;
    }

    // Pop item from ring buffer
    std::optional<T> Pop() {
        size_t currentHead = head_.load(std::memory_order_relaxed);
        size_t currentTail = tail_.load(std::memory_order_acquire);

        if (currentHead == currentTail) {
            return std::nullopt; // Buffer empty
        }

        T item = std::move(buffer_[currentHead]);
        head_.store((currentHead + 1) % Capacity, std::memory_order_release);
        return item;
    }

    bool IsEmpty() const {
        return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed);
    }

    size_t Size() const {
        size_t currentHead = head_.load(std::memory_order_relaxed);
        size_t currentTail = tail_.load(std::memory_order_relaxed);
        if (currentTail >= currentHead) {
            return currentTail - currentHead;
        }
        return Capacity - (currentHead - currentTail);
    }

    uint64_t GetDroppedCount() const {
        return droppedCount_.load(std::memory_order_relaxed);
    }

    void Clear() {
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

private:
    std::vector<T> buffer_;
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
    std::atomic<uint64_t> droppedCount_;
};

// Fixed-size reusable raw memory block
struct MemoryBlock {
    static constexpr size_t MAX_BLOCK_SIZE = 2048;
    uint8_t data[MAX_BLOCK_SIZE];
    size_t size{0};
};

class MemoryPool {
public:
    explicit MemoryPool(size_t poolSize = 256);
    ~MemoryPool() = default;

    MemoryBlock* Acquire();
    void Release(MemoryBlock* block);

private:
    std::vector<MemoryBlock> storage_;
    std::vector<MemoryBlock*> freeList_;
    std::mutex mutex_;
};

} // namespace veyra
