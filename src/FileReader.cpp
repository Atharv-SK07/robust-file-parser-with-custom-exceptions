#include "FileReader.h"

#include "Exceptions.h"

namespace rfp {

int FileReader::openCount_ = 0;

FileReader::FileReader(const std::string& path) : path_(path) {
    in_.open(path, std::ios::in | std::ios::binary);
    if (!in_.is_open()) {
        throw FileError("cannot open file '" + path + "'");
    }
    ++openCount_;  // counted only after a successful open
}

FileReader::~FileReader() {
    if (in_.is_open()) {
        in_.close();
        --openCount_;
    }
}

bool FileReader::readLine(std::string& line) {
    if (!std::getline(in_, line)) {
        if (in_.bad()) {
            throw FileError("read error while reading '" + path_ + "'");
        }
        return false;
    }
    ++lineNo_;
    if (!line.empty() && line.back() == '\r') line.pop_back();  // CRLF files
    if (lineNo_ == 1 && line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) {
        line.erase(0, 3);  // UTF-8 byte order mark
    }
    return true;
}

}  // namespace rfp
