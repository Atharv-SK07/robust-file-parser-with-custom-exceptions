// ErrorLog.h - Module 5: collects every error and prints a summary report.
#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

#include "Exceptions.h"

namespace rfp {

struct ErrorEntry {
    std::string category;  // "ParseError", "ValidationError", "FileError"
    std::size_t line;
    std::size_t column;
    std::string message;
    bool skipped;  // true if the record was dropped and parsing carried on
};

class ErrorLog {
public:
    void add(const ParserException& ex, bool skipped);

    const std::vector<ErrorEntry>& entries() const noexcept { return entries_; }
    std::size_t size() const noexcept { return entries_.size(); }
    bool empty() const noexcept { return entries_.empty(); }
    std::size_t count(const std::string& category) const;
    std::size_t skippedCount() const;

    // Human-readable report.
    void print(std::ostream& os, const std::string& sourceName) const;
    // Writes the same report to a file. Returns false if the file can't be written.
    bool writeToFile(const std::string& path, const std::string& sourceName) const;

private:
    std::vector<ErrorEntry> entries_;
};

}  // namespace rfp
