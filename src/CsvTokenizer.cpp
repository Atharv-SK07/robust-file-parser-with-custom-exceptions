#include "CsvTokenizer.h"

#include "Exceptions.h"

namespace rfp {

std::vector<Token> CsvTokenizer::tokenizeLine(const std::string& line,
                                              std::size_t lineNo) const {
    std::vector<Token> tokens;
    const std::size_t n = line.size();
    std::size_t pos = 0;

    while (true) {
        const std::size_t startCol = pos + 1;

        if (pos < n && line[pos] == '"') {
            // ---- quoted field ----
            std::string value;
            std::size_t i = pos + 1;
            bool closed = false;
            while (i < n) {
                if (line[i] == '"') {
                    if (i + 1 < n && line[i + 1] == '"') {  // "" -> literal quote
                        value += '"';
                        i += 2;
                        continue;
                    }
                    closed = true;
                    ++i;
                    break;
                }
                value += line[i++];
            }
            if (!closed) {
                throw ParseError("unclosed quote (field opened here never ends on this line)",
                                 lineNo, startCol);
            }
            if (i < n && line[i] != delimiter_) {
                throw ParseError(std::string("unexpected character '") + line[i] +
                                     "' after closing quote",
                                 lineNo, i + 1);
            }
            tokens.push_back({TokenType::QuotedField, value, lineNo, startCol});
            pos = i;
        } else {
            // ---- unquoted field ----
            std::size_t i = pos;
            while (i < n && line[i] != delimiter_) {
                if (line[i] == '"') {
                    throw ParseError("unexpected quote inside an unquoted field",
                                     lineNo, i + 1);
                }
                ++i;
            }
            tokens.push_back({TokenType::Field, line.substr(pos, i - pos), lineNo, startCol});
            pos = i;
        }

        if (pos < n) {  // line[pos] is the delimiter
            tokens.push_back({TokenType::Delimiter, std::string(1, delimiter_), lineNo, pos + 1});
            ++pos;  // a trailing delimiter yields one more (empty) field next loop
        } else {
            break;
        }
    }

    tokens.push_back({TokenType::EndOfLine, "", lineNo, n + 1});
    return tokens;
}

}  // namespace rfp
