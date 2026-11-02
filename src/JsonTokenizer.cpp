#include "JsonTokenizer.h"

#include <cctype>

namespace rfp {

namespace {

bool isDigit(char c) { return c >= '0' && c <= '9'; }

bool isHex(char c) {
    return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

void appendUtf8(std::string& out, unsigned cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// Strict JSON number grammar: -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?
bool isValidNumber(const std::string& s) {
    std::size_t i = 0;
    const std::size_t n = s.size();
    if (i < n && s[i] == '-') ++i;
    if (i >= n || !isDigit(s[i])) return false;
    if (s[i] == '0') {
        ++i;
    } else {
        while (i < n && isDigit(s[i])) ++i;
    }
    if (i < n && s[i] == '.') {
        ++i;
        if (i >= n || !isDigit(s[i])) return false;
        while (i < n && isDigit(s[i])) ++i;
    }
    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < n && (s[i] == '+' || s[i] == '-')) ++i;
        if (i >= n || !isDigit(s[i])) return false;
        while (i < n && isDigit(s[i])) ++i;
    }
    return i == n;
}

}  // namespace

void JsonTokenizer::tokenizeLine(const std::string& line, std::size_t lineNo,
                                 std::vector<Token>& out) const {
    const std::size_t n = line.size();
    std::size_t i = 0;

    while (i < n) {
        const char c = line[i];
        const std::size_t col = i + 1;

        if (c == ' ' || c == '\t') {
            ++i;
        } else if (c == '{') {
            out.push_back({TokenType::LBrace, "{", lineNo, col}); ++i;
        } else if (c == '}') {
            out.push_back({TokenType::RBrace, "}", lineNo, col}); ++i;
        } else if (c == '[') {
            out.push_back({TokenType::LBracket, "[", lineNo, col}); ++i;
        } else if (c == ']') {
            out.push_back({TokenType::RBracket, "]", lineNo, col}); ++i;
        } else if (c == ':') {
            out.push_back({TokenType::Colon, ":", lineNo, col}); ++i;
        } else if (c == ',') {
            out.push_back({TokenType::Comma, ",", lineNo, col}); ++i;
        } else if (c == '"') {
            // ---- string ----
            std::string value;
            std::string problem;  // first escape problem found, if any
            std::size_t j = i + 1;
            bool closed = false;
            while (j < n) {
                const char d = line[j];
                if (d == '"') { closed = true; ++j; break; }
                if (d == '\\') {
                    if (j + 1 >= n) { ++j; break; }
                    const char e = line[j + 1];
                    switch (e) {
                        case '"': value += '"'; j += 2; break;
                        case '\\': value += '\\'; j += 2; break;
                        case '/': value += '/'; j += 2; break;
                        case 'b': value += '\b'; j += 2; break;
                        case 'f': value += '\f'; j += 2; break;
                        case 'n': value += '\n'; j += 2; break;
                        case 'r': value += '\r'; j += 2; break;
                        case 't': value += '\t'; j += 2; break;
                        case 'u': {
                            // needs 4 hex digits at line[j+2] .. line[j+5]
                            bool ok = (j + 5 < n);
                            unsigned cp = 0;
                            for (std::size_t k = 2; ok && k <= 5; ++k) {
                                const char h = line[j + k];
                                if (!isHex(h)) { ok = false; break; }
                                cp = cp * 16 + static_cast<unsigned>(
                                    isDigit(h) ? h - '0' : (std::tolower(h) - 'a' + 10));
                            }
                            if (ok) {
                                appendUtf8(value, cp);
                                j += 6;
                            } else {
                                if (problem.empty()) problem = "invalid \\u escape (needs 4 hex digits)";
                                j += 2;
                            }
                            break;
                        }
                        default:
                            if (problem.empty()) {
                                problem = std::string("invalid escape sequence '\\") + e + "'";
                            }
                            j += 2;
                            break;
                    }
                    continue;
                }
                value += d;
                ++j;
            }
            if (!closed) {
                out.push_back({TokenType::Invalid, "unterminated string (missing closing quote)",
                               lineNo, col});
                i = n;  // the rest of the line belonged to the broken string
            } else if (!problem.empty()) {
                out.push_back({TokenType::Invalid, problem, lineNo, col});
                i = j;
            } else {
                out.push_back({TokenType::String, value, lineNo, col});
                i = j;
            }
        } else if (c == '-' || isDigit(c)) {
            // ---- number ----
            std::size_t j = i;
            while (j < n && (isDigit(line[j]) || line[j] == '-' || line[j] == '+' ||
                             line[j] == '.' || line[j] == 'e' || line[j] == 'E')) {
                ++j;
            }
            const std::string text = line.substr(i, j - i);
            if (isValidNumber(text)) {
                out.push_back({TokenType::Number, text, lineNo, col});
            } else {
                out.push_back({TokenType::Invalid, "invalid number '" + text + "'", lineNo, col});
            }
            i = j;
        } else if (std::isalpha(static_cast<unsigned char>(c))) {
            // ---- true / false / null ----
            std::size_t j = i;
            while (j < n && std::isalpha(static_cast<unsigned char>(line[j]))) ++j;
            const std::string word = line.substr(i, j - i);
            if (word == "true") out.push_back({TokenType::True, word, lineNo, col});
            else if (word == "false") out.push_back({TokenType::False, word, lineNo, col});
            else if (word == "null") out.push_back({TokenType::Null, word, lineNo, col});
            else out.push_back({TokenType::Invalid, "unknown literal '" + word + "'", lineNo, col});
            i = j;
        } else {
            out.push_back({TokenType::Invalid,
                           std::string("unexpected character '") + c + "'", lineNo, col});
            ++i;
        }
    }
}

}  // namespace rfp
