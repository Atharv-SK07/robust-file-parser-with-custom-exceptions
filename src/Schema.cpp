#include "Schema.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

namespace rfp {

namespace {

std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

}  // namespace

const char* toString(FieldType t) {
    switch (t) {
        case FieldType::String: return "string";
        case FieldType::Integer: return "integer";
        case FieldType::Double: return "number";
        case FieldType::Boolean: return "boolean";
    }
    return "unknown";
}

bool isBlank(const std::string& s) { return trim(s).empty(); }

bool isValidInteger(const std::string& raw) {
    const std::string s = trim(raw);
    if (s.empty()) return false;
    std::size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    if (i >= s.size()) return false;
    for (std::size_t k = i; k < s.size(); ++k) {
        if (!std::isdigit(static_cast<unsigned char>(s[k]))) return false;
    }
    errno = 0;
    char* end = nullptr;
    std::strtoll(s.c_str(), &end, 10);
    return errno == 0;  // rejects values too large for 64 bits
}

bool isValidDouble(const std::string& raw) {
    const std::string s = trim(raw);
    if (s.empty()) return false;
    // reject things strtod would accept but we don't want: hex, inf, nan
    for (char c : s) {
        if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '+' || c == '-' ||
              c == '.' || c == 'e' || c == 'E')) {
            return false;
        }
    }
    errno = 0;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    return end == s.c_str() + s.size() && errno == 0 && std::isfinite(v);
}

bool isValidBoolean(const std::string& raw) {
    const std::string s = lower(trim(raw));
    return s == "true" || s == "false" || s == "yes" || s == "no" || s == "1" || s == "0";
}

bool conformsTo(const std::string& value, FieldType t) {
    switch (t) {
        case FieldType::String: return true;
        case FieldType::Integer: return isValidInteger(value);
        case FieldType::Double: return isValidDouble(value);
        case FieldType::Boolean: return isValidBoolean(value);
    }
    return false;
}

std::vector<FieldSpec> parseSchemaSpec(const std::string& text) {
    std::vector<FieldSpec> result;
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = trim(item);
        if (item.empty()) continue;
        const std::size_t colon = item.find(':');
        if (colon == std::string::npos || colon == 0) {
            throw std::invalid_argument("bad schema item '" + item + "' (expected name:type)");
        }
        FieldSpec spec;
        spec.name = trim(item.substr(0, colon));
        std::string type = lower(trim(item.substr(colon + 1)));
        if (!type.empty() && type.back() == '?') {
            spec.required = false;
            type.pop_back();
        }
        if (type == "string" || type == "str") spec.type = FieldType::String;
        else if (type == "int" || type == "integer") spec.type = FieldType::Integer;
        else if (type == "double" || type == "number" || type == "float") spec.type = FieldType::Double;
        else if (type == "bool" || type == "boolean") spec.type = FieldType::Boolean;
        else throw std::invalid_argument("unknown type '" + type + "' in schema item '" + item + "'");
        result.push_back(spec);
    }
    if (result.empty()) throw std::invalid_argument("schema is empty");
    return result;
}

}  // namespace rfp
