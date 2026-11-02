// JsonTokenizer.h - Module 2: tokenizer for the JSON-like format.
//
// Lexical errors do not throw. They become TokenType::Invalid tokens whose text
// is the error message, so the tokenizer always reaches the end of the line and
// the parser can decide where to report and how to recover.
// Strings may not span lines (same reasoning as the CSV tokenizer).
#pragma once

#include <vector>

#include "Token.h"

namespace rfp {

class JsonTokenizer {
public:
    // Appends the tokens of one line to `out`.
    void tokenizeLine(const std::string& line, std::size_t lineNo,
                      std::vector<Token>& out) const;
};

}  // namespace rfp
