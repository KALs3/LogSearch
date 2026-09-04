#ifndef LOG_AGGREGATOR_RING_BUFFER_HPP
#define LOG_AGGREGATOR_RING_BUFFER_HPP

#include <mutex>
#include <condition_variable>
#include <vector>
#include <optional>
#include <stdexcept>
#include <cstddef>

namespace logagg {

template<typename T>
class RingBuffer {
public:
    // Constructor - specify maximum capacity
    explicit RingBuffer(size_t capacity);
    
    // Delete copy (can't copy a buffer with mutexes)
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    
    // Producer side: Add item to buffer
    // Blocks if buffer is full
    void push(T item);
    
    // Consumer side: Remove item from buffer
    // Blocks if buffer is empty
    T pop();
    
    // Non-blocking versions
    // Returns false if operation would block
    bool try_push(T item);
    std::optional<T> try_pop();
    // Timed pop - waits up to timeout for an item
    std::optional<T> pop_for(std::chrono::milliseconds timeout);

    // Status
    size_t size() const;
    bool empty() const;
    bool full() const;
    size_t capacity() const;
    
private:
    // Internal, non-locking helpers.
    // PRECONDITION: caller already holds mutex_.
    // These exist so that push/pop/try_push/try_pop/empty/full can share
    // the emptiness/fullness check without re-locking a mutex the thread
    // already owns (std::mutex is not recursive - double-locking it is UB
    // and will deadlock in practice).
    bool empty_unlocked() const { return count_ == 0; }
    bool full_unlocked() const { return count_ == capacity_; }

    // Internal circular buffer storage
    std::vector<T> buffer_;
    
    // Head and tail indices
    size_t head_;  // Where to read from
    size_t tail_;  // Where to write to
    size_t count_; // Number of items currently in buffer
    
    // Capacity
    const size_t capacity_;
    
    // Synchronization
    mutable std::mutex mutex_;
    std::condition_variable not_full_;   // Producers wait on this
    std::condition_variable not_empty_;  // Consumers wait on this
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================


template<typename T>
RingBuffer<T>::RingBuffer(size_t capacity)
    : buffer_(capacity),  // Creates 'capacity' default elements
      head_(0),
      tail_(0),
      count_(0),
      capacity_(capacity) {
    if (capacity == 0) {
        throw std::invalid_argument("RingBuffer capacity must be > 0");
    }
}

template<typename T>
void RingBuffer<T>::push(T item) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // Wait while buffer is full
    not_full_.wait(lock, [this]() { return !full_unlocked(); });
    
    // Add item at tail position
    buffer_[tail_] = std::move(item);
    tail_ = (tail_ + 1) % capacity_;
    count_++;
    
    // Notify one waiting consumer
    not_empty_.notify_one();
}

template<typename T>
T RingBuffer<T>::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // Wait while buffer is empty
    not_empty_.wait(lock, [this]() { return !empty_unlocked(); });
    
    // Extract item from head position
    T item = std::move(buffer_[head_]);
    head_ = (head_ + 1) % capacity_;
    count_--;
    
    // Notify one waiting producer
    not_full_.notify_one();
    
    return item;
}
template<typename T>
std::optional<T> RingBuffer<T>::pop_for(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (!not_empty_.wait_for(lock, timeout, [this]() { return !empty_unlocked(); })) {
        return std::nullopt;
    }
    
    T item = std::move(buffer_[head_]);
    head_ = (head_ + 1) % capacity_;
    count_--;
    
    not_full_.notify_one();
    return item;
}
template<typename T>
bool RingBuffer<T>::try_push(T item) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if full - don't wait
    if (full_unlocked()) {
        return false;
    }
    
    // Add item at tail position
    buffer_[tail_] = std::move(item);
    tail_ = (tail_ + 1) % capacity_;
    count_++;
    
    // Notify one waiting consumer
    not_empty_.notify_one();
    
    return true;
}

template<typename T>
std::optional<T> RingBuffer<T>::try_pop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if empty - don't wait
    if (empty_unlocked()) {
        return std::nullopt;
    }
    
    // Extract item from head position
    T item = std::move(buffer_[head_]);
    head_ = (head_ + 1) % capacity_;
    count_--;
    
    // Notify one waiting producer
    not_full_.notify_one();
    
    return item;
}

template<typename T>
size_t RingBuffer<T>::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
}

template<typename T>
bool RingBuffer<T>::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return empty_unlocked();
}

template<typename T>
bool RingBuffer<T>::full() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return full_unlocked();
}

template<typename T>
size_t RingBuffer<T>::capacity() const {
    return capacity_;  
}

} 

#endif 