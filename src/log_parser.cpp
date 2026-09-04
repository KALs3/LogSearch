#include "log_parser.hpp"
#include <cctype>
#include <algorithm>

namespace logagg {

std::optional<LogDocument> LogParser::parse(const std::string& json) const {
    // Extract each field
    auto timestamp = extract_number(json, "timestamp");
    auto level = extract_string(json, "level");
    auto service = extract_string(json, "service");
    auto message = extract_string(json, "message");
    
    // Check all fields present
    if (!timestamp || !level || !service || !message) {
        return std::nullopt;
    }
    
    // Create LogDocument with DocID 0 (will be assigned by index)
    LogDocument doc;
    doc.doc_id = 0;  // Placeholder, index will assign
    doc.timestamp = *timestamp;
    doc.level = *level;
    doc.service = *service;
    doc.message = *message;
    
    return doc;
}

std::optional<std::string> LogParser::extract_string(
    const std::string& json, 
    const std::string& key
) const {
    size_t key_pos = find_key(json, key);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    
    // Find first quote after ':'
    size_t quote_start = json.find('"', key_pos);
    if (quote_start == std::string::npos) {
        return std::nullopt;
    }
    
    // Find closing quote
    size_t quote_end = json.find('"', quote_start + 1);
    if (quote_end == std::string::npos) {
        return std::nullopt;
    }
    
    // Extract string between quotes
    return json.substr(quote_start + 1, quote_end - quote_start - 1);
}

std::optional<int64_t> LogParser::extract_number(
    const std::string& json, 
    const std::string& key
) const {
    size_t key_pos = find_key(json, key);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    
    // Find first digit after ':'
    size_t num_start = key_pos + 1;
    while (num_start < json.size() && 
           !std::isdigit(static_cast<unsigned char>(json[num_start])) &&
           json[num_start] != '-') {
        num_start++;
    }
    
    if (num_start >= json.size()) {
        return std::nullopt;
    }
    
    // Parse the number
    try {
        return std::stoll(json.substr(num_start));
    } catch (...) {
        return std::nullopt;
    }
}

size_t LogParser::find_key(
    const std::string& json, 
    const std::string& key
) const {
    // Search for "key":
    std::string search_pattern = "\"" + key + "\"";
    size_t key_pos = json.find(search_pattern);
    
    if (key_pos == std::string::npos) {
        return std::string::npos;
    }
    
    // Find ':' after the key
    size_t colon_pos = json.find(':', key_pos + search_pattern.size());
    return colon_pos;
}

std::string LogParser::trim(const std::string& str) const {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

} // namespace logagg