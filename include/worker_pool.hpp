#ifndef LOG_AGGREGATOR_WORKER_POOL_HPP
#define LOG_AGGREGATOR_WORKER_POOL_HPP

#include <vector>
#include <thread>
#include <atomic>
#include <string>
#include "ring_buffer.hpp"
#include "index.hpp"
#include "log_parser.hpp"

namespace logagg {

class WorkerPool {
public:
    // Constructor
    // num_workers: How many threads to spawn
    // buffer: Shared ring buffer to pull raw logs from
    // index: Shared inverted index to insert parsed logs into
    WorkerPool(size_t num_workers,
               RingBuffer<std::string>& buffer,
               InvertedIndex& index);
    
    // Delete copy (can't copy threads)
    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;
    
    // Destructor - ensures workers are stopped
    ~WorkerPool();
    
    // Start all worker threads
    void start();
    
    // Signal workers to stop and join them
    void stop();
    
    // Check if workers are running
    bool is_running() const;
    
    // Get number of workers
    size_t worker_count() const;
    
    // Get number of logs processed
    size_t logs_processed() const;
    
private:
    // The main worker loop: pop → parse → index
    void worker_loop();
    
    // Configuration
    size_t num_workers_;
    
    // Shared resources (references, not owned)
    RingBuffer<std::string>& buffer_;
    InvertedIndex& index_;
    
    // Worker threads
    std::vector<std::thread> workers_;
    
    // State
    std::atomic<bool> running_{false};
    std::atomic<size_t> logs_processed_{0};
};

} 

#endif 