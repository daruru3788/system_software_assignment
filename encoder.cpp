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