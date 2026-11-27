#ifndef OPERAND_H
#define OPERAND_H

#include <string>
#include <unordered_map>
#include <cstdint>
#include <algorithm>
#include <cctype>
#include <set>

#define MAX_INT 9223372036854775807

struct register_parser_rtn {
    int register_num;
    std::string err;
    bool is_success;
};

struct integer_parser_rtn {
    std::int64_t integer;
    std::string err;
    bool is_success;
};

struct memory_operand_parser_rtn {
    std::string offset;
    int register_num;
    std::string err;
    bool is_success;
};

register_parser_rtn parse_register(std::string regiser_str);

integer_parser_rtn parse_integer(std::string integer_str);

integer_parser_rtn parse_integer_char(std::string integer_str);

memory_operand_parser_rtn parse_memory_operand(std::string operand);

#endif