#include "JsonParser.h"

#include <cstdlib>

#include "Exceptions.h"
#include "FileReader.h"
#include "JsonTokenizer.h"

namespace rfp {

namespace {

constexpr int kMaxDepth = 64;

class Impl {
public:
    Impl(const std::vector<Token>& tokens, const JsonOptions& opt, JsonDocument& doc)
        : t_(tokens), opt_(opt), doc_(doc) {}

    void run() {
        const Token& first = cur();
        if (first.type == TokenType::LBracket) {
            parseArrayOfRecords();
        } else if (first.type == TokenType::LBrace) {
            handleRecord();  // a single object is one record
            if (cur().type != TokenType::EndOfFile) {
                report(ParseError("unexpected content after end of document", cur().line, cur().column),
                       false);
            }
        } else {
            report(unexpected(first, "'[' or '{' at the start of the document"), false);
        }
    }

private:
    const std::vector<Token>& t_;
    const JsonOptions& opt_;
    JsonDocument& doc_;
    std::size_t pos_ = 0;
    bool atRecordStart_ = false;  // set by resync() when pos_ already sits on a record's '{'

    const Token& cur() const { return t_[pos_]; }
    const Token& advance() {
        const Token& t = t_[pos_];
        if (t.type != TokenType::EndOfFile) ++pos_;
        return t;
    }

    static ParseError unexpected(const Token& tok, const std::string& expected) {
        if (tok.type == TokenType::Invalid) return ParseError(tok.text, tok.line, tok.column);
        if (tok.type == TokenType::EndOfFile) {
            return ParseError("unexpected end of file, expected " + expected, tok.line, tok.column);
        }
        return ParseError("unexpected '" + tok.text + "', expected " + expected, tok.line, tok.column);
    }

    void report(const ParserException& ex, bool skipped) {
        if (opt_.stopOnFirstError) ex.raise();  // keeps the derived type
        if (skipped) ++doc_.recordsSkipped;
        doc_.log.add(ex, skipped);
        if (opt_.maxErrors != 0 && doc_.log.size() >= opt_.maxErrors) {
            throw ParseError("too many errors (" + std::to_string(doc_.log.size()) + "), giving up",
                             ex.line(), 0);
        }
    }

    // ---- grammar ----
    JsonValue parseValue(int depth) {
        if (depth > kMaxDepth) {
            throw ParseError("nesting is too deep (limit " + std::to_string(kMaxDepth) + ")",
                             cur().line, cur().column);
        }
        const Token tok = cur();
        JsonValue v;
        v.line = tok.line;
        v.column = tok.column;
        switch (tok.type) {
            case TokenType::LBrace: return parseObject(depth);
            case TokenType::LBracket: return parseArray(depth);
            case TokenType::String:
                advance();
                v.type = JsonValue::Type::String;
                v.text = tok.text;
                return v;
            case TokenType::Number:
                advance();
                v.type = JsonValue::Type::Number;
                v.numberText = tok.text;
                v.number = std::strtod(tok.text.c_str(), nullptr);
                return v;
            case TokenType::True:
            case TokenType::False:
                advance();
                v.type = JsonValue::Type::Bool;
                v.boolean = (tok.type == TokenType::True);
                return v;
            case TokenType::Null:
                advance();
                v.type = JsonValue::Type::Null;
                return v;
            default:
                throw unexpected(tok, "a value");
        }
    }

    JsonValue parseObject(int depth) {
        JsonValue obj;
        obj.type = JsonValue::Type::Object;
        obj.line = cur().line;
        obj.column = cur().column;
        advance();  // '{'

        if (cur().type == TokenType::RBrace) { advance(); return obj; }

        while (true) {
            const Token keyTok = cur();
            if (keyTok.type != TokenType::String) throw unexpected(keyTok, "a string key");
            advance();
            if (cur().type != TokenType::Colon) throw unexpected(cur(), "':' after key");
            advance();
            for (const std::string& k : obj.keys) {
                if (k == keyTok.text) {
                    throw ParseError("duplicate key '" + keyTok.text + "'", keyTok.line, keyTok.column);
                }
            }
            JsonValue val = parseValue(depth + 1);
            obj.keys.push_back(keyTok.text);
            obj.items.push_back(std::move(val));

            if (cur().type == TokenType::Comma) {
                advance();
                if (cur().type == TokenType::RBrace) {
                    throw ParseError("trailing comma before '}'", cur().line, cur().column);
                }
                continue;
            }
            if (cur().type == TokenType::RBrace) { advance(); return obj; }
            throw unexpected(cur(), "',' or '}'");
        }
    }

    JsonValue parseArray(int depth) {
        JsonValue arr;
        arr.type = JsonValue::Type::Array;
        arr.line = cur().line;
        arr.column = cur().column;
        advance();  // '['

        if (cur().type == TokenType::RBracket) { advance(); return arr; }

        while (true) {
            arr.items.push_back(parseValue(depth + 1));
            if (cur().type == TokenType::Comma) {
                advance();
                if (cur().type == TokenType::RBracket) {
                    throw ParseError("trailing comma before ']'", cur().line, cur().column);
                }
                continue;
            }
            if (cur().type == TokenType::RBracket) { advance(); return arr; }
            throw unexpected(cur(), "',' or ']'");
        }
    }

    // ---- validation ----
    void validateRecord(const JsonValue& rec) const {
        if (rec.type != JsonValue::Type::Object) {
            throw ValidationError("record must be an object, found " + typeName(rec),
                                  rec.line, rec.column);
        }
        for (const FieldSpec& spec : opt_.schema) {
            const JsonValue* v = rec.find(spec.name);
            if (v == nullptr || v->type == JsonValue::Type::Null) {
                if (spec.required) {
                    throw ValidationError("missing required field '" + spec.name + "'",
                                          rec.line, rec.column);
                }
                continue;
            }
            if (!matches(*v, spec.type)) {
                throw ValidationError("field '" + spec.name + "' expected " + toString(spec.type) +
                                          " but found " + typeName(*v),
                                      v->line, v->column);
            }
        }
    }

    static bool matches(const JsonValue& v, FieldType t) {
        switch (t) {
            case FieldType::String: return v.type == JsonValue::Type::String;
            case FieldType::Boolean: return v.type == JsonValue::Type::Bool;
            case FieldType::Double: return v.type == JsonValue::Type::Number;
            case FieldType::Integer:
                return v.type == JsonValue::Type::Number &&
                       v.numberText.find_first_of(".eE") == std::string::npos;
        }
        return false;
    }

    static std::string typeName(const JsonValue& v) {
        switch (v.type) {
            case JsonValue::Type::Null: return "null";
            case JsonValue::Type::Bool: return "boolean";
            case JsonValue::Type::Number: return "number '" + v.numberText + "'";
            case JsonValue::Type::String: return "string '" + v.text + "'";
            case JsonValue::Type::Array: return "array";
            case JsonValue::Type::Object: return "object";
        }
        return "unknown";
    }

    // ---- records + recovery ----
    // Parse and validate one record starting at pos_. On failure: log it, skip it.
    // Returns normally in both cases; pos_ is left at a safe resume point.
    void handleRecord() {
        const std::size_t start = pos_;
        ++doc_.recordsRead;
        try {
            JsonValue rec = parseValue(0);
            validateRecord(rec);
            doc_.records.push_back(std::move(rec));
        } catch (const ParserException& ex) {
            report(ex, true);
            resync(start);
        }
    }

    // Move pos_ to the next likely record boundary: a comma followed by '{',
    // or the closing ']' of the top-level array. Always makes progress.
    void resync(std::size_t start) {
        std::size_t j = pos_ > start + 1 ? pos_ - 1 : start + 1;
        for (; j + 1 < t_.size(); ++j) {
            if (t_[j].type == TokenType::Comma && t_[j + 1].type == TokenType::LBrace) {
                pos_ = j;  // leave the comma; the array loop consumes it
                return;
            }
            // A bad token (e.g. an unterminated string swallowed the rest of its line,
            // comma included) followed by '{' on a later line: that '{' starts a new record.
            if (t_[j].type == TokenType::Invalid && t_[j + 1].type == TokenType::LBrace &&
                t_[j + 1].line > t_[j].line) {
                pos_ = j + 1;
                atRecordStart_ = true;  // no comma to consume before it
                return;
            }
            if (t_[j].type == TokenType::RBracket && t_[j + 1].type == TokenType::EndOfFile) {
                pos_ = j;
                return;
            }
        }
        pos_ = t_.size() - 1;  // EndOfFile
    }

    void parseArrayOfRecords() {
        advance();  // '['
        if (cur().type == TokenType::RBracket) { advance(); finish(); return; }

        while (true) {
            handleRecord();

            if (atRecordStart_) {  // recovery already positioned us on the next record
                atRecordStart_ = false;
                continue;
            }
            if (cur().type == TokenType::Comma) {
                advance();
                if (cur().type == TokenType::RBracket) {
                    report(ParseError("trailing comma before ']'", cur().line, cur().column), false);
                    advance();
                    break;
                }
                continue;
            }
            if (cur().type == TokenType::RBracket) { advance(); break; }

            // Something other than ',' or ']' follows a finished record.
            if (cur().type == TokenType::EndOfFile) {
                report(unexpected(cur(), "',' or ']' (is the closing ']' missing?)"), false);
                return;
            }
            report(unexpected(cur(), "',' or ']'"), false);
            if (cur().type == TokenType::LBrace) continue;  // missing comma: next record is intact
            // Otherwise skip to the next boundary and keep going.
            const std::size_t before = pos_;
            resync(before);
            if (atRecordStart_) { atRecordStart_ = false; continue; }
            if (cur().type == TokenType::EndOfFile) return;
            if (cur().type == TokenType::RBracket) { advance(); break; }
            advance();  // cur() is the comma before the next record: consume it
        }
        finish();
    }

    void finish() {
        if (cur().type != TokenType::EndOfFile) {
            report(ParseError("unexpected content after end of document", cur().line, cur().column),
                   false);
        }
    }
};

}  // namespace

JsonDocument JsonParser::parseFile(const std::string& path) const {
    JsonDocument doc;
    std::vector<Token> tokens;
    {
        FileReader reader(path);  // RAII: closed at the end of this block, or on a throw
        JsonTokenizer tokenizer;
        std::string line;
        while (reader.readLine(line)) {
            tokenizer.tokenizeLine(line, reader.lineNumber(), tokens);
        }
        if (reader.lineNumber() == 0) throw FileError("file is empty: '" + path + "'");
        const std::size_t lastLine = reader.lineNumber();
        tokens.push_back({TokenType::EndOfFile, "", lastLine, line.size() + 1});
    }

    if (tokens.size() == 1) {  // only EndOfFile: file had nothing but whitespace
        throw FileError("file contains no data: '" + path + "'");
    }

    Impl(tokens, opt_, doc).run();
    return doc;
}

}  // namespace rfp
