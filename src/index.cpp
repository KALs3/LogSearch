#include "index.hpp"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iostream>
namespace logagg {

InvertedIndex::InvertedIndex() {
    // Reserve space to avoid rehashing early
    posting_lists_.reserve(10000);
    documents_.reserve(100000);
}

DocID InvertedIndex::insert(const LogDocument& doc) {
    // Tokenize the document fields
    auto tokens = tokenizer_.tokenize_fields(doc.level, doc.service, doc.message);
    // Lock for the entire insert operation
    std::lock_guard<std::mutex> lock(mutex_);

        // Assign DocID (thread-safe)
    DocID doc_id = next_doc_id_.fetch_add(1);
    
    // Create a copy with the assigned DocID
    LogDocument new_doc = doc;
    new_doc.doc_id = doc_id;
    
    // Insert into document store
    documents_[doc_id] = new_doc;
    
    // Update posting lists for each token
    for (const auto& token : tokens) {
        auto& posting_list = posting_lists_[token];
        
        // Posting lists must remain strictly sorted
        // Since DocIDs are monotonically increasing, we can just push_back

      if (posting_list.empty() || posting_list.back() != doc_id) {
        posting_list.push_back(doc_id);
      }
        
        
        // Update memory estimate
        memory_estimate_ += sizeof(DocID) + token.size();
    }

    // Update document memory estimate
    memory_estimate_ += sizeof(LogDocument) + 
                        new_doc.message.size() + 
                        new_doc.service.size() + 
                        new_doc.level.size();
    
                    
    return doc_id;
}

PostingList InvertedIndex::search_term(const std::string& term) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = posting_lists_.find(term);
    if (it != posting_lists_.end()) {
        return it->second;  // Return copy
    }
    
    
    return PostingList();  // Empty list
}

std::optional<LogDocument> InvertedIndex::get_document(DocID doc_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = documents_.find(doc_id);
    if (it != documents_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

IndexStats InvertedIndex::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    IndexStats stats;
    stats.document_count = documents_.size();
    stats.unique_terms = posting_lists_.size();
    stats.memory_estimate_bytes = memory_estimate_.load();
    
    return stats;
}

bool InvertedIndex::needs_flush(size_t memory_threshold_bytes) const {
    return memory_estimate_.load() >= memory_threshold_bytes;
}

void InvertedIndex::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    posting_lists_.clear();
    documents_.clear();
    memory_estimate_ = 0;
    // Don't reset next_doc_id_ - DocIDs must remain unique across segments
}

size_t InvertedIndex::estimate_memory_usage() const {
    size_t estimate = 0;
    
    // Estimate posting lists memory
    for (const auto& [term, postings] : posting_lists_) {
        estimate += term.size();
        estimate += postings.size() * sizeof(DocID);
        estimate += sizeof(PostingList);  // Vector overhead
    }
    
    // Estimate documents memory
    for (const auto& [doc_id, doc] : documents_) {
        estimate += sizeof(LogDocument);
        estimate += doc.message.size();
        estimate += doc.service.size();
        estimate += doc.level.size();
    }
    
    return estimate;
}

} // namespace logagg