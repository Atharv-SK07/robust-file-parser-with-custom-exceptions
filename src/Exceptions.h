// Exceptions.h - Module 4: custom exception hierarchy.
//
//   std::exception
//     +-- ParserException      (base: category, message, line, column)
//           +-- FileError        (cannot open / read / empty file)
//           +-- ParseError       (syntax problems: unclosed quote, bad token)
//           +-- ValidationError  (well-formed but breaks rules: field count, type)
#pragma once

#include <cstddef>
#include <exception>
#include <string>

namespace rfp {

class ParserException : public std::exception {
public:
    ParserException(std::string category, std::string message,
                    std::size_t line = 0, std::size_t column = 0)
        : category_(std::move(category)),
          message_(std::move(message)),
          line_(line),
          column_(column) {
        full_ = "[" + category_ + "] ";
        if (line_ > 0) {
            full_ += "line " + std::to_string(line_);
            if (column_ > 0) full_ += ", col " + std::to_string(column_);
            full_ += ": ";
        }
        full_ += message_;
    }

    const char* what() const noexcept override { return full_.c_str(); }

    // Re-throws this object with its real (derived) type. Writing `throw ex;`
    // through a base-class reference would slice it; this avoids that.
    [[noreturn]] virtual void raise() const { throw *this; }

    const std::string& category() const noexcept { return category_; }
    const std::string& message() const noexcept { return message_; }
    std::size_t line() const noexcept { return line_; }
    std::size_t column() const noexcept { return column_; }

private:
    std::string category_;
    std::string message_;
    std::size_t line_;
    std::size_t column_;
    std::string full_;
};

// Problems with the file itself. Always fatal: there is nothing to recover.
class FileError : public ParserException {
public:
    explicit FileError(const std::string& message)
        : ParserException("FileError", message) {}
    [[noreturn]] void raise() const override { throw *this; }
};

// Syntax errors found while tokenizing or building records.
class ParseError : public ParserException {
public:
    ParseError(const std::string& message, std::size_t line, std::size_t column)
        : ParserException("ParseError", message, line, column) {}
    [[noreturn]] void raise() const override { throw *this; }
};

// Data is syntactically fine but violates a rule (field count, type, required).
class ValidationError : public ParserException {
public:
    ValidationError(const std::string& message, std::size_t line, std::size_t column)
        : ParserException("ValidationError", message, line, column) {}
    [[noreturn]] void raise() const override { throw *this; }
};

}  // namespace rfp
