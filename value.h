#ifndef VALUE_H
#define VALUE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "./operand.h"

struct value_rtn {
    std::int64_t integer;
    std::string err;
    bool is_success;
    bool is_unresolved;
    bool has_symbol;
};

struct string_rtn {
    std::vector<std::uint8_t> bytes;
    std::string err;
    bool is_success;
};

std::string trim_text(std::string text);
bool valid_symbol_name(std::string name);
value_rtn evaluate_value(std::string expression, const std::unordered_map<std::string, std::int64_t>& symbols);
integer_parser_rtn normalize_word(std::int64_t value);
string_rtn parse_string_literal(std::string text);

#endif
