#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <string>
#include <vector>
#include <fstream>
#include "./parser.h"

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

source_file read_source(std::string path);

#endif