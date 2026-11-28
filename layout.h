#ifndef LAYOUT_H
#define LAYOUT_H

#include "./assembler.h"

struct placed_line {
    source_line source;
    std::string section;
    std::uint32_t address;
    int byte_size;
    std::vector<std::uint8_t> bytes;
};

struct layout_rtn {
    std::vector<placed_line> lines;
    std::unordered_map<std::string, std::int64_t> symbols;
    std::unordered_map<std::string, std::string> symbol_sections;
    std::set<std::string> globals;
    std::uint32_t text_start;
    std::uint32_t data_start;
    std::uint32_t data_end;
    std::uint32_t bss_start;
    std::uint32_t bss_end;
    std::string err;
    bool is_success;
};

struct image_rtn {
    std::vector<std::uint8_t> bytes;
    std::string err;
    bool is_success;
};

layout_rtn layout_sections(source_file source, constants_rtn constants);
image_rtn generate_image(layout_rtn& layout);

#endif
