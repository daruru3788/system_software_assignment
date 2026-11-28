#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <cstdint>
#include <set>
#include "./parser.h"
#include "./operand.h"

struct source_line {
    int line_num;
    std::string original;
    std::string label;
    std::string instruction;
    std::vector<std::string> operands;
};

struct source_file {
    std::vector<source_line> source_lines;
    std::string err;
    bool is_success;
};

struct constants_rtn {
    std::unordered_map<std::string, std::int64_t> constants;
    std::string err;
    bool is_success;
};

struct inst_size_rtn {
    int byte_size;
    std::string err;
    bool is_success;
};

source_file read_source(std::string path);

constants_rtn collect_constants(source_file lines);

inst_size_rtn instruction_size(source_line line, constants_rtn constants);

inst_size_rtn directive_size(source_line line, constants_rtn constants, std::uint32_t current_address);

#endif