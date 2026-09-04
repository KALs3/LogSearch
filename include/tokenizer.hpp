#ifndef LOG_AGGREGATOR_TOKENIZER_HPP
#define LOG_AGGREGATOR_TOKENIZER_HPP

#include <string>
#include <vector>
#include <unordered_set>

namespace logagg {

class Tokenizer {
public:
    Tokenizer();  // Constructor - initializes stop words
    
    // Main tokenization method
    // Takes a string, returns vector of lowercase tokens
    std::vector<std::string> tokenize(const std::string& text) const;
    
    // Tokenize multiple fields at once
    std::vector<std::string> tokenize_fields(
        const std::string& level,
        const std::string& service,
        const std::string& message
    ) const;
    
    // Check if a word is a stop word
    bool is_stop_word(const std::string& token) const;
    
private:
    // Set of common words that don't help in search
    std::unordered_set<std::string> stop_words_;
    
    // Helper function to check if character is alphanumeric
    static bool is_alphanumeric(char c);
};

} 

#endif 