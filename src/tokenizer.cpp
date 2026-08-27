#include "tokenizer.hpp"
#include <cctype>  // for std::tolower, std::isalnum
#include <utility>  // for std::move

namespace logagg {

Tokenizer::Tokenizer() {
    // Initialize stop words - common English words that don't add search value
    const char* common_stop_words[] = {
        // Articles
        "the", "a", "an",
        
        // Conjunctions  
        "and", "or", "but", "if", "then", "else",
        
        // Prepositions
        "of", "to", "in", "on", "at", "by", "for", "with", "about",
        "from", "into", "through", "during", "before", "after",
        
        // Helping verbs
        "is", "are", "was", "were", "be", "been", "being",
        "have", "has", "had", "do", "does", "did",
        
        // Modal verbs
        "will", "would", "can", "could", "should", "may", "might", "must",
        
        // Pronouns
        "this", "that", "these", "those", "it", "its",
        
        // Other common words
        "as", "not", "no", "yes", "so", "than", "too", "very"
    };
    
    // Insert all stop words into the set
    for (const auto* word : common_stop_words) {
        stop_words_.insert(word);
    }
}

std::vector<std::string> Tokenizer::tokenize(const std::string& text) const {
    std::vector<std::string> tokens;
    std::string current_token;
    
    // Reserve space to avoid frequent reallocations
    // Most tokens are 5-20 characters
    current_token.reserve(32);
    
    for (char c : text) {
        if (is_alphanumeric(c)) {
            // This character is part of a word
            // Convert to lowercase and append
            current_token.push_back(std::tolower(c));
        } else {
            // We hit a delimiter (space, punctuation, etc.)
            // Process the current token if it exists
            if (!current_token.empty()) {
                // Only add if it's not a stop word
                if (!is_stop_word(current_token)) {
                    tokens.push_back(std::move(current_token));
                }
                // Reset for next token
                current_token.clear();
                current_token.reserve(32);
            }
        }
    }
    
    // Handle the last token if string doesn't end with delimiter
    if (!current_token.empty() && !is_stop_word(current_token)) {
        tokens.push_back(std::move(current_token));
    }
    
    return tokens;
}

std::vector<std::string> Tokenizer::tokenize_fields(
    const std::string& level,
    const std::string& service,
    const std::string& message
) const {
    // Combine all fields with spaces
    std::string combined;
      
    combined = level + " " + service + " " + message;
    
    // Tokenize the combined string
    return tokenize(combined);
}

bool Tokenizer::is_stop_word(const std::string& token) const {
    return stop_words_.find(token) != stop_words_.end();
}

bool Tokenizer::is_alphanumeric(char c) {
    // std::isalnum expects unsigned char to avoid undefined behavior
    return std::isalnum(static_cast<unsigned char>(c));
}

} 