// Token.h - Module 2: token type shared by the CSV and JSON tokenizers.
// Every token remembers where it started (1-based line and column).
#pragma once

#include <cstddef>
#include <string>

namespace rfp {

enum class TokenType {
    // CSV
    Field,        // unquoted field
    QuotedField,  // "quoted" field (text already unescaped)
    Delimiter,    // , ; \t ...
    EndOfLine,
    // JSON-like
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Colon,
    Comma,
    String,
    Number,
    True,
    False,
    Null,
    Invalid,  // bad lexeme; text holds the error message
    EndOfFile
};

struct Token {
    TokenType type;
    std::string text;
    std::size_t line;
    std::size_t column;
};

}  // namespace rfp
