
// main.cpp - Interactive menu + command-line demo
// Robust File Parser with Custom Exceptions

#include <algorithm>
#include <cctype>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <fstream>
#include <iomanip>

#include "CsvParser.h"
#include "Exceptions.h"
#include "JsonParser.h"

using namespace rfp;

namespace {

// Display the original input file with line numbers.
void displayInputFile(const std::string& path) {
    std::ifstream file(path);

    if (!file.is_open()) {
        std::cout << "Could not open input preview.\n";
        return;
    }

    std::cout << "\n========== ORIGINAL INPUT FILE ==========\n";
    std::cout << "File: " << path << "\n\n";

    std::string line;
    std::size_t lineNumber = 1;

    while (std::getline(file, line)) {
        std::cout << std::setw(3) << lineNumber
                  << " | " << line << "\n";
        ++lineNumber;
    }

    std::cout << "=========================================\n\n";
}

// Display the original source lines where errors occurred.
void displayErrorContext(
    const std::string& path,
    const ErrorLog& log
) {
    std::ifstream file(path);

    if (!file.is_open() || log.empty())
        return;

    std::vector<std::string> lines;
    std::string line;

    while (std::getline(file, line))
        lines.push_back(line);

    std::cout << "\n========== ERROR LOCATIONS ==========\n";

    for (const ErrorEntry& error : log.entries()) {

        if (error.line == 0 || error.line > lines.size())
            continue;

        const std::string& sourceLine = lines[error.line - 1];

        std::cout << "\n[" << error.category << "] "
                  << "Line " << error.line
                  << ", Column " << error.column << "\n";

        std::cout << sourceLine << "\n";

        // Keep the caret within the source line.
        const std::size_t position =
            std::min(error.column > 0 ? error.column - 1 : 0,
                     sourceLine.size());

        std::cout << std::string(position, ' ')
                  << "^\n";

        std::cout << "Reason: " << error.message << "\n";
    }

    std::cout << "=====================================\n";
}

std::string lowerCase(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
    return s;
}

std::string guessFormat(const std::string& path) {
    const std::size_t dot = path.find_last_of('.');

    if (dot != std::string::npos &&
        lowerCase(path.substr(dot + 1)) == "json")
        return "json";

    return "csv";
}

std::string valueToString(const JsonValue& v) {
    switch (v.type) {
        case JsonValue::Type::Null:
            return "null";

        case JsonValue::Type::Bool:
            return v.boolean ? "true" : "false";

        case JsonValue::Type::Number:
            return v.numberText;

        case JsonValue::Type::String:
            return "\"" + v.text + "\"";

        case JsonValue::Type::Array: {
            std::string s = "[";

            for (std::size_t i = 0; i < v.items.size(); ++i) {
                if (i) s += ", ";
                s += valueToString(v.items[i]);
            }

            return s + "]";
        }

        case JsonValue::Type::Object: {
            std::string s = "{";

            for (std::size_t i = 0; i < v.items.size(); ++i) {
                if (i) s += ", ";
                s += v.keys[i] + ": " + valueToString(v.items[i]);
            }

            return s + "}";
        }
    }

    return "?";
}

// --------------------------------------------------
// Core parser function
// --------------------------------------------------

int runParser(
    const std::string& path,
    const std::string& format,
    const std::string& schemaText,
    char delim,
    bool header,
    bool strict,
    std::size_t maxErrors,
    const std::string& logPath
) {
    std::vector<FieldSpec> schema;

    try {
        if (!schemaText.empty())
            schema = parseSchemaSpec(schemaText);
    }
    catch (const std::invalid_argument& e) {
        std::cerr << "Schema error: " << e.what() << "\n";
        return 2;
    }

    try {
        ErrorLog* logPtr = nullptr;

        std::size_t good = 0;
        std::size_t seen = 0;
        std::size_t skipped = 0;

        CsvDocument csv;
        JsonDocument js;

        if (format == "json") {
            JsonOptions o;

            o.schema = schema;
            o.stopOnFirstError = strict;
            o.maxErrors = maxErrors;

            js = JsonParser(o).parseFile(path);

            logPtr = &js.log;

            good = js.records.size();
            seen = js.recordsRead;
            skipped = js.recordsSkipped;

            for (const JsonValue& r : js.records) {
                std::cout << "  OK  line " << r.line
                          << ": " << valueToString(r) << "\n";
            }
        }
        else {
            CsvOptions o;

            o.delimiter = delim;
            o.hasHeader = header;
            o.schema = schema;
            o.stopOnFirstError = strict;
            o.maxErrors = maxErrors;

            csv = CsvParser(o).parseFile(path);

            logPtr = &csv.log;

            good = csv.records.size();
            seen = csv.rowsRead;
            skipped = csv.rowsSkipped;

            for (const CsvRecord& r : csv.records) {
                std::cout << "  OK  line " << r.line << ":";

                for (const std::string& f : r.fields)
                    std::cout << " [" << f << "]";

                std::cout << "\n";
            }
        }

        std::cout << "\nParsed " << good
                  << " valid record(s) out of " << seen
                  << " (" << skipped << " skipped)\n\n";

        logPtr->print(std::cout, path);

        // Show the original source lines responsible for errors.
        displayErrorContext(path, *logPtr);

        if (!logPath.empty()) {
            if (logPtr->writeToFile(logPath, path))
                std::cout << "\nReport written to " << logPath << "\n";
            else
                std::cerr << "\nCould not write report to "
                          << logPath << "\n";
        }

        return logPtr->empty() ? 0 : 1;
    }
    catch (const FileError& e) {
        std::cerr << e.what() << "\n";
        return 2;
    }
    catch (const ParserException& e) {
        std::cerr << e.what() << "\n";
        return 2;
    }
    catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << "\n";
        return 2;
    }
}

// --------------------------------------------------
// Interactive menu
// --------------------------------------------------

void displayMenu() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "       ROBUST FILE PARSER\n";
    std::cout << "       CSV & JSON VALIDATION SYSTEM\n";
    std::cout << "========================================\n\n";

    std::cout << "1. Parse Valid CSV File\n";
    std::cout << "2. Parse Corrupted CSV File\n";
    std::cout << "3. Parse Valid JSON File\n";
    std::cout << "4. Parse Corrupted JSON File\n";
    std::cout << "5. Parse Custom File\n";
    std::cout << "6. Demonstrate Strict Mode\n";
    std::cout << "7. Exit\n";

    std::cout << "\n========================================\n";
    std::cout << "Enter your choice: ";
}

// --------------------------------------------------
// Menu controller
// --------------------------------------------------

int runMenu() {
    std::string choice;

    const std::string csvSchema =
        "id:int,name:string,price:double,in_stock:bool";

    const std::string jsonSchema =
        "id:int,name:string,age:int,active:bool,email:string?";

    while (true) {
        displayMenu();

        if (!std::getline(std::cin, choice))
            return 0;

        std::cout << "\n";

        if (choice == "7") {
            std::cout << "Thank you for using Robust File Parser!\n";
            break;
        }

        std::string path;
        std::string format;
        std::string schema;
        std::string logPath;

        bool strict = false;

        if (choice == "1") {
            path = "testdata/valid_products.csv";
            format = "csv";
            schema = csvSchema;
        }
        else if (choice == "2") {
            path = "testdata/corrupt_products.csv";
            format = "csv";
            schema = csvSchema;
            logPath = "logs/corrupt_products.log";
        }
        else if (choice == "3") {
            path = "testdata/valid_users.json";
            format = "json";
            schema = jsonSchema;
        }
        else if (choice == "4") {
            path = "testdata/corrupt_users.json";
            format = "json";
            schema = jsonSchema;
            logPath = "logs/corrupt_users.log";
        }
        else if (choice == "5") {
            std::cout << "Enter file path: ";
            std::getline(std::cin, path);

            if (path.empty()) {
                std::cout << "File path cannot be empty.\n";
                continue;
            }

            format = guessFormat(path);

            std::cout << "Detected format: " << format << "\n";

            std::cout << "Enter schema (press Enter for none): ";
            std::getline(std::cin, schema);
        }
        else if (choice == "6") {
            path = "testdata/corrupt_products.csv";
            format = "csv";
            schema = csvSchema;
            strict = true;
        }
        else {
            std::cout << "Invalid choice. Please select 1 to 7.\n";
            continue;
        }

        std::cout << "----------------------------------------\n";
        std::cout << "Processing: " << path << "\n";
        std::cout << "Format: " << format << "\n";
        std::cout << "----------------------------------------\n";

        displayInputFile(path);

        runParser(path, format, schema, ',', true,
                  strict, 1000, logPath);
        std::cout << "\n\nPress Enter to return to the main menu...";

        std::string pause;
        std::getline(std::cin, pause);
    }

    return 0;
}

} // namespace

// --------------------------------------------------
// Main function
// --------------------------------------------------

int main(int argc, char** argv) {

    // No command-line arguments: display interactive menu.
    if (argc == 1)
        return runMenu();

    // Command-line mode is retained for advanced users.
    std::string path, format, schemaText, logPath;

    char delim = ',';
    bool header = true;
    bool strict = false;

    std::size_t maxErrors = 1000;

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string a = argv[i];

            auto next = [&](const char* what) -> std::string {
                if (i + 1 >= argc)
                    throw std::invalid_argument(
                        std::string("missing value for ") + what);

                return argv[++i];
            };

            if (a == "--format")
                format = lowerCase(next("--format"));

            else if (a == "--schema")
                schemaText = next("--schema");

            else if (a == "--delim") {
                const std::string d = next("--delim");
                delim = (d == "\\t" || d == "tab")
                    ? '\t' : (d.empty() ? ',' : d[0]);
            }

            else if (a == "--no-header")
                header = false;

            else if (a == "--strict")
                strict = true;

            else if (a == "--max-errors")
                maxErrors = static_cast<std::size_t>(
                    std::stoul(next("--max-errors")));

            else if (a == "--log")
                logPath = next("--log");

            else if (a == "--help" || a == "-h") {
                std::cout << "Usage: rfp_demo <file> [options]\n";
                return 0;
            }

            else if (!a.empty() && a[0] == '-')
                throw std::invalid_argument("unknown option " + a);

            else
                path = a;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Argument error: " << e.what() << "\n";
        return 2;
    }

    if (path.empty()) {
        std::cerr << "No input file specified.\n";
        return 2;
    }

    if (format.empty())
        format = guessFormat(path);

    return runParser(path, format, schemaText, delim,
                     header, strict, maxErrors, logPath);
}
