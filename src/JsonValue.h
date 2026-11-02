// JsonValue.h - parsed value of the JSON-like format.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace rfp {

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string numberText;  // original text, used to tell 3 from 3.5
    std::string text;        // for String
    std::vector<JsonValue> items;        // for Array
    std::vector<std::string> keys;       // for Object (same order as `items`)
    std::size_t line = 0;                // where the value started
    std::size_t column = 0;

    // Object lookup: returns nullptr if the key is missing.
    const JsonValue* find(const std::string& key) const {
        for (std::size_t i = 0; i < keys.size(); ++i) {
            if (keys[i] == key) return &items[i];
        }
        return nullptr;
    }
};

}  // namespace rfp
