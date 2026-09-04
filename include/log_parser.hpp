#ifndef LOG_AGGREGATOR_LOG_PARSER_HPP
#define LOG_AGGREGATOR_LOG_PARSER_HPP

#include <string>
#include <optional>
#include "types.hpp"

namespace logagg {

class LogParser {
public:
    // Parse JSON string to LogDocument
    // Returns nullopt if parsing fails
    std::optional<LogDocument> parse(const std::string& json) const;
    
private:
    // Extract a string value for a given key
    // e.g., extract_string(json, "level") → "ERROR"
    std::optional<std::string> extract_string(
        const std::string& json, 
        const std::string& key
    ) const;
    
    // Extract an integer value for a given key
    // e.g., extract_number(json, "timestamp") → 1637000000000000
    std::optional<int64_t> extract_number(
        const std::string& json, 
        const std::string& key
    ) const;
    
    // Find the position of value after a key
    // Returns position of ':' or npos if key not found
    size_t find_key(const std::string& json, const std::string& key) const;
    
    // Trim whitespace from both ends of a string
    std::string trim(const std::string& str) const;
};

} 

#endif 