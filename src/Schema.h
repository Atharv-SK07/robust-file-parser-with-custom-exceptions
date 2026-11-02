// Schema.h - Module 3 helpers: field types, validators, schema description.
#pragma once

#include <string>
#include <vector>

namespace rfp {

enum class FieldType { String, Integer, Double, Boolean };

struct FieldSpec {
    std::string name;
    FieldType type = FieldType::String;
    bool required = true;
};

const char* toString(FieldType t);

// Whole-string checks (surrounding spaces are ignored).
bool isValidInteger(const std::string& s);
bool isValidDouble(const std::string& s);
bool isValidBoolean(const std::string& s);  // true/false/yes/no/1/0 (any case)
bool isBlank(const std::string& s);         // empty or only whitespace
bool conformsTo(const std::string& value, FieldType t);

// Parses "id:int,name:string,price:double,note:string?"
//   types: string | int | double | bool      suffix '?' = optional field
// Throws std::invalid_argument on a malformed description.
std::vector<FieldSpec> parseSchemaSpec(const std::string& text);

}  // namespace rfp
