#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <set>
#include <iostream>

struct parser_rtn {
    std::string result;
    std::string err;
    bool is_success;
};

struct line_parser_rtn {
    std::string label;
    std::string instruction;
    std::string operands;
};

parser_rtn parser(std::string line);

line_parser_rtn line_parse(std::string clean_line);

#endif