// CsvParser.h - Module 3 (CSV): builds records from tokens and validates them.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "ErrorLog.h"
#include "Schema.h"

namespace rfp {

struct CsvOptions {
    char delimiter = ',';
    bool hasHeader = true;
    // Optional. If empty, the expected field count comes from the header
    // (or from the first data row when there is no header) and every field is a string.
    std::vector<FieldSpec> schema;
    bool stopOnFirstError = false;  // strict mode: rethrow instead of recovering
    std::size_t maxErrors = 1000;   // 0 = unlimited; abort with ParseError when exceeded
};

struct CsvRecord {
    std::size_t line;                 // source line number
    std::vector<std::string> fields;  // already unquoted
};

struct CsvDocument {
    std::vector<std::string> headers;
    std::vector<CsvRecord> records;  // only the valid rows
    ErrorLog log;
    std::size_t rowsRead = 0;        // data rows seen (valid + invalid)
    std::size_t rowsSkipped = 0;
};

class CsvParser {
public:
    explicit CsvParser(CsvOptions options = CsvOptions()) : opt_(std::move(options)) {}

    // Throws FileError if the file can't be opened/read or is empty.
    // ParseError / ValidationError are caught per row, logged, and the row skipped.
    CsvDocument parseFile(const std::string& path) const;

private:
    CsvOptions opt_;
};

}  // namespace rfp
