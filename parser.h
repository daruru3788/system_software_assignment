#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <set>
#include <iostream>
#include <vector>

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

struct operands_parser_rtn {
    std::vector<std::string> operands;
    std::string err;
    bool is_success;
};

parser_rtn parser(std::string line);

line_parser_rtn line_parse(std::string clean_line);

operands_parser_rtn operands_parse(std::string operands);

#endif