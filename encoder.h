#ifndef ENCODER_H
#define ENCODER_H

#include <string>
#include <cstdint>

struct encoder_rtn {
    std::uint32_t machine_code;
    std::string err;
    bool is_success;
};

encoder_rtn encoder_r(std::string instruct, std::string rd, std::string rs1, std::string rs2);

encoder_rtn encoder_i(std::string instruct, std::string rd, std::string rs1, std::int64_t imm);

#endif