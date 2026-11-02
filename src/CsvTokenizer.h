// CsvTokenizer.h - Module 2: splits ONE physical CSV line into tokens.
//
// Design decision: quoted fields may not span lines. A quote that is still open
// at the end of the line is an "unclosed quote" error, which lets the parser
// skip just that row instead of swallowing the rest of the file.
#pragma once

#include <vector>

#include "Token.h"

namespace rfp {

class CsvTokenizer {
public:
    explicit CsvTokenizer(char delimiter = ',') : delimiter_(delimiter) {}

    // Throws ParseError (with line and column) on:
    //   - unclosed quote
    //   - characters after a closing quote
    //   - a quote in the middle of an unquoted field
    std::vector<Token> tokenizeLine(const std::string& line, std::size_t lineNo) const;

private:
    char delimiter_;
};

}  // namespace rfp
