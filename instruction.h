#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include "./assembler.h"

struct instruction_rtn {
    std::vector<std::uint32_t> codes;
    std::string err;
    bool is_success;
};

instruction_rtn assemble_instruction(source_line line, std::uint32_t address,
    const std::unordered_map<std::string, std::int64_t>& symbols,
    const std::unordered_map<std::string, std::string>& symbol_sections, int expected_size);

#endif
