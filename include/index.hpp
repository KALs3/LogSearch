#ifndef LOG_AGGREGATOR_INDEX_HPP
#define LOG_AGGREGATOR_INDEX_HPP

#include "types.hpp"
#include "tokenizer.hpp"
#include <mutex>
#include <memory>
#include <atomic>

namespace logagg {

class InvertedIndex {
public:
    InvertedIndex();
    ~InvertedIndex() = default;
    
    // Delete copy constructor and assignment (non-copyable)
    InvertedIndex(const InvertedIndex&) = delete;
    InvertedIndex& operator=(const InvertedIndex&) = delete;
    
    // Insert a new log document into the index
    // Returns the assigned DocID
    DocID insert(const LogDocument& doc);
    
    // Search for documents containing a specific term
    // Returns posting list for the term (empty if not found)
    PostingList search_term(const std::string& term) const;
    
    // Retrieve a document by its DocID
    std::optional<LogDocument> get_document(DocID doc_id) const;
    
    // Get current statistics
    IndexStats get_stats() const;
    
    // Check if memory threshold is exceeded
    bool needs_flush(size_t memory_threshold_bytes) const;
    
    // Clear all data (called after flushing to disk)
    void clear();
    
private:
    // Thread-safe insert without external locking
    void insert_internal(const LogDocument& doc, 
                         const std::vector<std::string>& tokens);
    
    // Core data structures
    InvertedIndexMap posting_lists_;  // term -> sorted list of DocIDs
    std::unordered_map<DocID, LogDocument> documents_;  // DocID -> document
    
    // Thread safety
    mutable std::mutex mutex_;  // Protects all data structures
    
    // Statistics
    std::atomic<uint64_t> next_doc_id_{1};  // Next available DocID
    std::atomic<size_t> memory_estimate_{0};  // Estimated memory usage
    
    // Tokenizer
    Tokenizer tokenizer_;
    
    // Helper to estimate memory usage
    size_t estimate_memory_usage() const;
};

// Shared pointer type for passing index around
using IndexPtr = std::shared_ptr<InvertedIndex>;

} // namespace logagg

#endif // LOG_AGGREGATOR_INDEX_HPP