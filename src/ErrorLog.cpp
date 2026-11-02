#include "ErrorLog.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <ostream>

namespace rfp {

void ErrorLog::add(const ParserException& ex, bool skipped) {
    entries_.push_back({ex.category(), ex.line(), ex.column(), ex.message(), skipped});
}

std::size_t ErrorLog::count(const std::string& category) const {
    return static_cast<std::size_t>(
        std::count_if(entries_.begin(), entries_.end(),
                      [&](const ErrorEntry& e) { return e.category == category; }));
}

std::size_t ErrorLog::skippedCount() const {
    return static_cast<std::size_t>(
        std::count_if(entries_.begin(), entries_.end(),
                      [](const ErrorEntry& e) { return e.skipped; }));
}

void ErrorLog::print(std::ostream& os, const std::string& sourceName) const {
    os << "==== Error Report: " << sourceName << " ====\n";
    if (entries_.empty()) {
        os << "No errors found.\n";
        return;
    }
    for (const ErrorEntry& e : entries_) {
        std::string where = "line " + std::to_string(e.line);
        if (e.column > 0) where += ", col " + std::to_string(e.column);
        os << std::left << std::setw(19) << ("[" + e.category + "]")
           << std::setw(16) << where
           << (e.skipped ? "(record skipped) " : "(logged)         ")
           << e.message << "\n";
    }
    os << "---- Summary ----\n"
       << "Total errors     : " << size() << "\n"
       << "  ParseError     : " << count("ParseError") << "\n"
       << "  ValidationError: " << count("ValidationError") << "\n"
       << "  FileError      : " << count("FileError") << "\n"
       << "Records skipped  : " << skippedCount() << "\n";
}

bool ErrorLog::writeToFile(const std::string& path, const std::string& sourceName) const {
    std::ofstream out(path);
    if (!out) return false;
    print(out, sourceName);
    return static_cast<bool>(out);
}

}  // namespace rfp
