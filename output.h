#ifndef OUTPUT_H
#define OUTPUT_H

#include "./layout.h"

struct output_rtn {
    std::string err;
    bool is_success;
};

bool same_file_path(std::string first, std::string second);
output_rtn write_binary(std::string path, const std::vector<std::uint8_t>& bytes);
output_rtn write_listing(std::string path, const layout_rtn& layout);

#endif
