// JsonParser.h - Module 3 (JSON-like): recursive-descent parser + validation.
//
// Accepted input: a top-level array of objects  [ {...}, {...} ]  or one object.
// Each object is a "record" and is checked against an optional schema.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "ErrorLog.h"
#include "JsonValue.h"
#include "Schema.h"

namespace rfp {

struct JsonOptions {
    std::vector<FieldSpec> schema;  // optional: required keys + value types
    bool stopOnFirstError = false;
    std::size_t maxErrors = 1000;   // 0 = unlimited
};

struct JsonDocument {
    std::vector<JsonValue> records;  // only the valid records
    ErrorLog log;
    std::size_t recordsRead = 0;
    std::size_t recordsSkipped = 0;
};

class JsonParser {
public:
    explicit JsonParser(JsonOptions options = JsonOptions()) : opt_(std::move(options)) {}

    // Throws FileError if the file can't be opened/read or is empty.
    JsonDocument parseFile(const std::string& path) const;

private:
    JsonOptions opt_;
};

}  // namespace rfp
