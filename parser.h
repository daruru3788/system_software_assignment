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

parser_rtn parser(std::string line);

#endif