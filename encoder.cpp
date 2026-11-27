#include "./encoder.h"
#include "./operand.h"
#include <unordered_map>
#include <utility>

using namespace std;

static const unordered_map<string, pair<int, int>> R_FUNCT = {
    {"add", {0x0, 0x00}}, {"sub", {0x0, 0x20}}, {"sll", {0x1, 0x00}},
    {"slt", {0x2, 0x00}}, {"sltu", {0x3, 0x00}}, {"xor", {0x4, 0x00}},
    {"srl", {0x5, 0x00}}, {"sra", {0x5, 0x20}}, {"or", {0x6, 0x00}},
    {"and", {0x7, 0x00}}, {"mul", {0x0, 0x01}}, {"mulh", {0x1, 0x01}},
    {"mulhsu", {0x2, 0x01}}, {"mulhu", {0x3, 0x01}}, {"div", {0x4, 0x01}},
    {"divu", {0x5, 0x01}}, {"rem", {0x6, 0x01}}, {"remu", {0x7, 0x01}}
};

static const unordered_map<string, pair<int, int>> I_FUNCT = {
    {"addi", {0x0, 0x13}}, {"slti", {0x2, 0x13}}, {"sltiu", {0x3, 0x13}},
    {"xori", {0x4, 0x13}}, {"ori", {0x6, 0x13}}, {"andi", {0x7, 0x13}},
    {"slli", {0x1, 0x13}}, {"srli", {0x5, 0x13}}, {"srai", {0x5, 0x13}},
    {"lb", {0x0, 0x03}}, {"lh", {0x1, 0x03}}, {"lw", {0x2, 0x03}},
    {"lbu", {0x4, 0x03}}, {"lhu", {0x5, 0x03}}, {"jalr", {0x0, 0x67}}
};

static const unordered_map<string, int> I_SHIFT_FUNCT7 = {
    {"slli", 0x00}, {"srli", 0x00}, {"srai", 0x20}
};

static const unordered_map<string, pair<int, int>> S_FUNCT = {
    {"sb", {0x0, 0x23}}, {"sh", {0x1, 0x23}}, {"sw", {0x2, 0x23}}
};

static const unordered_map<string, pair<int, int>> B_FUNCT = {
    {"beq", {0x0, 0x63}}, {"bne", {0x1, 0x63}}, {"blt", {0x4, 0x63}},
    {"bge", {0x5, 0x63}}, {"bltu", {0x6, 0x63}}, {"bgeu", {0x7, 0x63}}
};

static const unordered_map<string, int> U_OPCODE = {
    {"lui", 0x37}, {"auipc", 0x17}
};

static const unordered_map<string, int> J_OPCODE = {
    {"jal", 0x6F}
};

static const unordered_map<string, pair<int, int>> CSR_FUNCT = {
    {"csrrw", {0x1, 0x73}}, {"csrrs", {0x2, 0x73}}, {"csrrc", {0x3, 0x73}},
    {"csrrwi", {0x5, 0x73}}, {"csrrsi", {0x6, 0x73}}, {"csrrci", {0x7, 0x73}}
};

static const unordered_map<string, uint32_t> FIXED_CODE = {
    {"ecall", 0x00000073u}, {"ebreak", 0x00100073u}, {"fence", 0x0FF0000Fu},
    {"mret", 0x30200073u}, {"wfi", 0x10500073u}
};

static const unordered_map<string, int> CSR_ADDRESS = {
    {"mstatus", 0x300}, {"mie", 0x304}, {"mtvec", 0x305},
    {"mscratch", 0x340}, {"mepc", 0x341}, {"mcause", 0x342},
    {"mtval", 0x343}, {"mip", 0x344}, {"mhartid", 0xF14}
};

encoder_rtn encoder_r(string instruct, string rd, string rs1, string rs2) {
    auto rd_rtn = parse_register(rd);
    if (!rd_rtn.is_success) return {0, rd_rtn.err, false};

    auto rs1_rtn = parse_register(rs1);
    if (!rs1_rtn.is_success) return {0, rs1_rtn.err, false};

    auto rs2_rtn = parse_register(rs2);
    if (!rs2_rtn.is_success) return {0, rs2_rtn.err, false};

    auto r_find = R_FUNCT.find(instruct);
    if (r_find == R_FUNCT.end()) return {0, "Invalid instruction", false};
    auto [funct3, funct7] = r_find->second;

    uint32_t code = (uint32_t)funct7 << 25 | rs2_rtn.register_num << 20 | rs1_rtn.register_num << 15 | funct3 << 12 | rd_rtn.register_num << 7 | 0x33;
    return {code, "", true};
}

encoder_rtn encoder_i(std::string instruct, std::string rd, std::string rs1, std::int64_t imm) {
    auto rd_rtn = parse_register(rd);
    if (!rd_rtn.is_success) return {0, rd_rtn.err, false};

    auto rs1_rtn = parse_register(rs1);
    if (!rs1_rtn.is_success) return {0, rs1_rtn.err, false};

    auto i_find = I_FUNCT.find(instruct);
    if (i_find == I_FUNCT.end()) return {0, "Invalid instruction", false};
    auto [funct3, opcode] = i_find->second;

    auto i_shift_find = I_SHIFT_FUNCT7.find(instruct);

    uint32_t imm_tmp;
    if (i_shift_find == I_SHIFT_FUNCT7.end()) {
        if (imm < -2048 || imm > 2047) return {0, "Out of range immediate", false};
        imm_tmp = static_cast<uint32_t>(imm) & 0xFFFu;
    } else {
        if (imm < 0 || imm > 31) return {0, "Out of range immediate", false};
        imm_tmp = (i_shift_find->second << 5) | imm;
    }

    uint32_t code = (uint32_t)imm_tmp << 20 | rs1_rtn.register_num << 15 | funct3 << 12 | rd_rtn.register_num << 7 | opcode;
    return {code, "", true};
}

encoder_rtn encoder_s(std::string instruct, std::string rs2, std::string rs1, std::int64_t imm) {
    auto rs1_rtn = parse_register(rs1);
    if (!rs1_rtn.is_success) return {0, rs1_rtn.err, false};

    auto rs2_rtn = parse_register(rs2);
    if (!rs2_rtn.is_success) return {0, rs2_rtn.err, false};

    auto s_find = S_FUNCT.find(instruct);
    if (s_find == S_FUNCT.end()) return {0, "Invalid instruction", false};
    auto [funct3, opcode] = s_find->second;

    if (imm < -2048 || imm > 2047) return {0, "Out of range immediate", false};
    uint32_t imm_tmp = static_cast<uint32_t>(imm) & 0xFFFu;
    uint32_t imm_upper = (imm_tmp >> 5) & 0x7F;
    uint32_t imm_lower = imm_tmp & 0x1F;

    uint32_t code = (uint32_t)imm_upper << 25 | rs2_rtn.register_num << 20 | rs1_rtn.register_num << 15 | funct3 << 12 | imm_lower << 7 | opcode;
    return {code, "", true};
}

encoder_rtn encoder_b(std::string instruct, std::string rs1, std::string rs2, std::int64_t offset) {
    auto rs1_rtn = parse_register(rs1);
    if (!rs1_rtn.is_success) return {0, rs1_rtn.err, false};

    auto rs2_rtn = parse_register(rs2);
    if (!rs2_rtn.is_success) return {0, rs2_rtn.err, false};

    auto b_find = B_FUNCT.find(instruct);
    if (b_find == B_FUNCT.end()) return {0, "Invalid instruction", false};
    auto [funct3, opcode] = b_find->second;

    if (offset > 4094 || offset < -4096) return {0, "Out of range offset", false};
    if (offset % 2 != 0) return {0, "Invalid offset", false};

    uint32_t offset_tmp = static_cast<uint32_t>(offset) & 0x1FFFu;
    uint32_t offset12 = (offset_tmp >> 12) & 0x1;
    uint32_t offset10_5 = (offset_tmp >> 5) & 0x3F;
    uint32_t offset4_1 = (offset_tmp >> 1) & 0xF;
    uint32_t offset_11 = (offset_tmp >> 11) & 0x1;

    uint32_t code = (uint32_t)offset12 << 31 | offset10_5 << 25 | rs2_rtn.register_num << 20 | rs1_rtn.register_num << 15 | funct3 << 12 | offset4_1 << 8 | offset_11 << 7 | opcode;
    return {code, "", true};
}

encoder_rtn encoder_u(std::string instruct, std::string rd, std::int64_t imm20) {
    auto rd_rtn = parse_register(rd);
    if (!rd_rtn.is_success) return {0, rd_rtn.err, false};

    auto u_find = U_OPCODE.find(instruct);
    if (u_find == U_OPCODE.end()) return {0, "Invalid instruction", false};
    auto opcode = u_find->second;

    if (imm20 < -524288 || imm20 > 1048575) return {0, "Out of range immediate", false};
    uint32_t imm_tmp = static_cast<uint32_t>(imm20) & 0xFFFFF;

    uint32_t code = (uint32_t)imm_tmp << 12 | rd_rtn.register_num << 7 | opcode;
    return {code, "", true};
}

encoder_rtn encoder_j(std::string instruct, std::string rd, std::int64_t offset) {
    auto rd_rtn = parse_register(rd);
    if (!rd_rtn.is_success) return {0, rd_rtn.err, false};

    auto j_find = J_OPCODE.find(instruct);
    if (j_find == J_OPCODE.end()) return {0, "Invalid instruction", false};
    auto opcode = j_find->second;

    if (offset < -1048576 || offset > 1048574) return {0, "Out of range immediate", false};
    if (offset % 2 != 0) return {0, "Invalid offset", false};
    uint32_t offset_tmp = static_cast<uint32_t>(offset) & 0x1FFFFFu;
    uint32_t offset20 = (offset_tmp >> 20) & 0x1;
    uint32_t offset10_1 = (offset_tmp >> 1) & 0x3FF;
    uint32_t offset11 = (offset_tmp >> 11) & 0x1;
    uint32_t offset19_12 = (offset_tmp >> 12) & 0xFF;

    uint32_t code = (uint32_t)offset20 << 31 | offset10_1 << 21 | offset11 << 20 | offset19_12 << 12 | rd_rtn.register_num << 7 | opcode;
    return {code, "", true};
}

encoder_rtn encoder_fixed(std::string instruct) {
    auto fixed_find = FIXED_CODE.find(instruct);
    if (fixed_find == FIXED_CODE.end()) return {0, "Invalid instruction", false};
    auto code = fixed_find->second;

    return {code, "", true};
}

encoder_rtn encoder_csr(std::string instruct, std::string rd, std::string csr, std::string source) {
    auto csr_find = CSR_FUNCT.find(instruct);
    if (csr_find == CSR_FUNCT.end()) return {0, "Invalid instruction", false};
    auto [funct3, opcode] = csr_find->second;

    auto rd_rtn = parse_register(rd);
    if (!rd_rtn.is_success) return {0, rd_rtn.err, false};

    int64_t csr_num;
    auto address_find = CSR_ADDRESS.find(csr);
    if (address_find != CSR_ADDRESS.end()) {
        csr_num = address_find->second;
    } else {
        auto csr_rtn = parse_integer(csr);
        if (!csr_rtn.is_success) return {0, csr_rtn.err, false};
        csr_num = csr_rtn.integer;
    }
    if (csr_num < 0 || csr_num > 4095) return {0, "Out of range CSR number", false};

    int64_t source_num;
    if (funct3 >= 5) {
        auto source_rtn = parse_integer(source);
        if (!source_rtn.is_success) return {0, source_rtn.err, false};
        source_num = source_rtn.integer;
        if (source_num < 0 || source_num > 31) return {0, "Out of range CSR immediate", false};
    } else {
        auto source_rtn = parse_register(source);
        if (!source_rtn.is_success) return {0, source_rtn.err, false};
        source_num = source_rtn.register_num;
    }

    uint32_t code = (static_cast<uint32_t>(csr_num) << 20) | (static_cast<uint32_t>(source_num) << 15) | (static_cast<uint32_t>(funct3) << 12) | (static_cast<uint32_t>(rd_rtn.register_num) << 7) | static_cast<uint32_t>(opcode);
    return {code, "", true};
}
