// FileReader.h - Module 1: RAII file reader.
// The file is opened in the constructor and closed in the destructor, so it is
// released correctly even when an exception unwinds the stack.
#pragma once

#include <cstddef>
#include <fstream>
#include <string>

namespace rfp {

class FileReader {
public:
    // Throws FileError if the file cannot be opened.
    explicit FileReader(const std::string& path);
    ~FileReader();

    FileReader(const FileReader&) = delete;
    FileReader& operator=(const FileReader&) = delete;

    // Reads the next line (without '\n' or '\r'). Strips a UTF-8 BOM on line 1.
    // Returns false at end of file. Throws FileError on a read failure.
    bool readLine(std::string& line);

    std::size_t lineNumber() const noexcept { return lineNo_; }
    const std::string& path() const noexcept { return path_; }

    // Number of FileReader objects currently holding an open file.
    // Used by the tests to prove nothing leaks when exceptions are thrown.
    static int openCount() noexcept { return openCount_; }

private:
    std::ifstream in_;
    std::string path_;
    std::size_t lineNo_ = 0;
    static int openCount_;
};

}  // namespace rfp
