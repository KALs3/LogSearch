#include "worker_pool.hpp"
#include "log_parser.hpp"
#include <iostream>
#include <chrono>

namespace logagg {

WorkerPool::WorkerPool(size_t num_workers,
                       RingBuffer<std::string>& buffer,
                       InvertedIndex& index)
    : num_workers_(num_workers),
      buffer_(buffer),
      index_(index) {
    
    if (num_workers == 0) {
        throw std::invalid_argument("WorkerPool must have at least 1 worker");
    }
}

WorkerPool::~WorkerPool() {
    stop();
}

void WorkerPool::start() {
    if (running_) {
        return;
    }
    
    running_ = true;
    
    workers_.reserve(num_workers_);
    for (size_t i = 0; i < num_workers_; i++) {
        workers_.emplace_back(&WorkerPool::worker_loop, this);
    }
    
    std::cout << "WorkerPool started with " << num_workers_ << " workers\n";
}

void WorkerPool::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    workers_.clear();
    
    std::cout << "WorkerPool stopped. Total logs processed: " 
              << logs_processed_ << "\n";
}

void WorkerPool::worker_loop() {
    LogParser parser;
    
    // Continue while running OR buffer still has data
    while (running_ || !buffer_.empty()) {
        // Wait up to 10ms for data
        auto item = buffer_.pop_for(std::chrono::milliseconds(10));
        
        if (item.has_value()) {
            auto doc = parser.parse(*item);
            if (doc) {
                index_.insert(*doc);
                logs_processed_++;
            }
        }
    }
}

bool WorkerPool::is_running() const {
    return running_;
}

size_t WorkerPool::worker_count() const {
    return num_workers_;
}

size_t WorkerPool::logs_processed() const {
    return logs_processed_;
}

} // namespace logagg