#ifndef ENCODER_H
#define ENCODER_H

#include <string>
#include <cstdint>

struct encoder_rtn {
    std::uint32_t machine_code;
    std::string err;
    bool is_success;
};

bool is_real_instruction(std::string instruct);

encoder_rtn encoder_r(std::string instruct, std::string rd, std::string rs1, std::string rs2);

encoder_rtn encoder_i(std::string instruct, std::string rd, std::string rs1, std::int64_t imm);

encoder_rtn encoder_s(std::string instruct, std::string rs2, std::string rs1, std::int64_t imm);

encoder_rtn encoder_b(std::string instruct, std::string rs1, std::string rs2, std::int64_t offset);

encoder_rtn encoder_u(std::string instruct, std::string rd, std::int64_t imm20);

encoder_rtn encoder_j(std::string instruct, std::string rd, std::int64_t offset);

encoder_rtn encoder_fixed(std::string instruct);

encoder_rtn encoder_csr(std::string instruct, std::string rd, std::string csr, std::string source);

#endif
