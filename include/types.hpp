#ifndef LOG_AGGREGATOR_TYPES_HPP
#define LOG_AGGREGATOR_TYPES_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <optional>

namespace logagg {

// Unique identifier for each log document
// uint64_t gives us up to 18.4 quintillion unique IDs
using DocID = uint64_t;

// Timestamp in microseconds since epoch
// This gives us microsecond precision from 1970 to year 2262
using Timestamp = int64_t;

struct LogDocument {
    DocID doc_id;          // Monotonically increasing unique ID
    Timestamp timestamp;    // When the log was generated
    std::string level;      // ERROR, WARN, INFO, DEBUG etc.
    std::string service;    // Which service generated this log
    std::string message;    // The actual log message
    
    // Default constructor
    LogDocument() : doc_id(0), timestamp(0) {}
    
    // Convenience constructor
    LogDocument(DocID id, Timestamp ts, std::string lvl, 
                std::string svc, std::string msg)
        : doc_id(id), timestamp(ts), level(std::move(lvl)),
          service(std::move(svc)), message(std::move(msg)) {}
};

// A posting list is simply a sorted vector of DocIDs
// We keep it sorted for O(A+B) merge intersection
using PostingList = std::vector<DocID>;

// The inverted index maps terms to their posting lists
// unordered_map for O(1) average case lookup
using InvertedIndexMap = std::unordered_map<std::string, PostingList>;

// Structure to hold search results
struct SearchResult {
    DocID doc_id;
    Timestamp timestamp;
    std::string level;
    std::string service;
    std::string message;
    
    // For sorting results by timestamp
    bool operator<(const SearchResult& other) const {
        return timestamp < other.timestamp;
    }
};

// Statistics for monitoring index health
struct IndexStats {
    size_t document_count;
    size_t unique_terms;
    size_t memory_estimate_bytes;
    
    IndexStats() : document_count(0), unique_terms(0), 
                   memory_estimate_bytes(0) {}
};

} 

#endif 