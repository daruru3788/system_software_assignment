#include "./instruction.h"
#include "./encoder.h"
#include "./value.h"
#include <cctype>
#include <climits>

using namespace std;

static instruction_rtn instruction_result(encoder_rtn rtn) {
    if (!rtn.is_success) return {{}, rtn.err, false};
    return {{rtn.machine_code}, "", true};
}

static bool has_address_symbol(string expression, const unordered_map<string, string>& sections) {
    size_t i = 0;
    while (i < expression.size()) {
        char c = expression[i];
        if (c == '\'') {
            i++;
            while (i < expression.size()) {
                char next = expression[i++];
                if (next == '\\' && i < expression.size()) i++;
                else if (next == '\'') break;
            }
        } else if (isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.') {
            size_t start = i++;
            while (i < expression.size() && (isalnum(static_cast<unsigned char>(expression[i])) || expression[i] == '_' || expression[i] == '.')) i++;
            string name = expression.substr(start, i - start);
            auto found = sections.find(name);
            if (found != sections.end() && found->second != "=") return true;
        } else i++;
    }
    return false;
}

static value_rtn branch_offset(string expression, uint32_t address,
    const unordered_map<string, int64_t>& symbols, const unordered_map<string, string>& sections) {
    auto rtn = evaluate_value(expression, symbols);
    if (!rtn.is_success) return rtn;
    // Numeric/.equ expressions are offsets; labels and predefined addresses are targets.
    if (has_address_symbol(expression, sections)) {
        if (rtn.integer < INT64_MIN + static_cast<int64_t>(address)) return {0, "Branch target out of range", false, false, false};
        rtn.integer -= address;
    }
    return rtn;
}

instruction_rtn assemble_instruction(source_line line, uint32_t address,
    const unordered_map<string, int64_t>& symbols,
    const unordered_map<string, string>& symbol_sections, int expected_size) {
    string instruction = line.instruction;
    auto op = line.operands;
    string count_error = "Invalid operands number for " + instruction;

    if (instruction == "li" || instruction == "la") {
        if (op.size() != 2) return {{}, count_error, false};
        auto value = evaluate_value(op[1], symbols);
        if (!value.is_success) return {{}, value.err, false};
        int64_t integer_v;
        if (instruction == "li") {
            auto normalized = normalize_word(value.integer);
            if (!normalized.is_success) return {{}, normalized.err, false};
            integer_v = normalized.integer;
            if (expected_size == 4) return instruction_result(encoder_i("addi", op[0], "x0", integer_v));
        } else {
            if (value.integer < 0 || value.integer > UINT32_MAX) return {{}, "Address out of range", false};
            uint32_t difference = static_cast<uint32_t>(value.integer) - address;
            integer_v = normalize_word(difference).integer;
        }
        int64_t lo = static_cast<uint32_t>(integer_v) & 0xFFFu;
        if (lo >= 2048) lo -= 4096;
        int64_t hi = (integer_v - lo) / 4096;
        auto upper = encoder_u(instruction == "li" ? "lui" : "auipc", op[0], hi);
        if (!upper.is_success) return {{}, upper.err, false};
        auto lower = encoder_i("addi", op[0], op[0], lo);
        if (!lower.is_success) return {{}, lower.err, false};
        return {{upper.machine_code, lower.machine_code}, "", true};
    }

    if (instruction == "nop" || instruction == "ret") {
        if (!op.empty()) return {{}, count_error, false};
        if (instruction == "nop") return instruction_result(encoder_i("addi", "x0", "x0", 0));
        return instruction_result(encoder_i("jalr", "x0", "ra", 0));
    } else if (instruction == "mv" || instruction == "not" || instruction == "neg") {
        if (op.size() != 2) return {{}, count_error, false};
        if (instruction == "mv") return instruction_result(encoder_i("addi", op[0], op[1], 0));
        if (instruction == "not") return instruction_result(encoder_i("xori", op[0], op[1], -1));
        return instruction_result(encoder_r("sub", op[0], "x0", op[1]));
    } else if (instruction == "jr") {
        if (op.size() != 1) return {{}, count_error, false};
        return instruction_result(encoder_i("jalr", "x0", op[0], 0));
    } else if (instruction == "j" || instruction == "call") {
        if (op.size() != 1) return {{}, count_error, false};
        op = {instruction == "j" ? "x0" : "ra", op[0]};
        instruction = "jal";
    } else if (instruction == "jal" && op.size() == 1) {
        op = {"ra", op[0]};
    } else if (instruction == "beqz" || instruction == "bnez" || instruction == "bltz" || instruction == "bgtz") {
        if (op.size() != 2) return {{}, count_error, false};
        if (instruction == "bgtz") op = {"x0", op[0], op[1]};
        else op = {op[0], "x0", op[1]};
        if (instruction == "beqz") instruction = "beq";
        else if (instruction == "bnez") instruction = "bne";
        else instruction = "blt";
    } else if (instruction == "bgt" || instruction == "ble" || instruction == "bgtu" || instruction == "bleu") {
        if (op.size() != 3) return {{}, count_error, false};
        op = {op[1], op[0], op[2]};
        if (instruction == "bgt") instruction = "blt";
        else if (instruction == "ble") instruction = "bge";
        else if (instruction == "bgtu") instruction = "bltu";
        else instruction = "bgeu";
    } else if (instruction == "csrr") {
        if (op.size() != 2) return {{}, count_error, false};
        op = {op[0], op[1], "x0"};
        instruction = "csrrs";
    } else if (instruction == "csrw" || instruction == "csrs" || instruction == "csrsi" || instruction == "csrci") {
        if (op.size() != 2) return {{}, count_error, false};
        op = {"x0", op[0], op[1]};
        if (instruction == "csrw") instruction = "csrrw";
        else if (instruction == "csrs") instruction = "csrrs";
        else if (instruction == "csrsi") instruction = "csrrsi";
        else instruction = "csrrci";
    }

    string format = instruction_format(instruction);
    if (format == "R") {
        if (op.size() != 3) return {{}, count_error, false};
        return instruction_result(encoder_r(instruction, op[0], op[1], op[2]));
    } else if (format == "I" || format == "S") {
        static const set<string> memory_instructions = {"lb", "lh", "lw", "lbu", "lhu", "sb", "sh", "sw", "jalr"};
        if (memory_instructions.count(instruction) && !(instruction == "jalr" && op.size() == 3)) {
            if (op.size() != 2) return {{}, count_error, false};
            auto memory = parse_memory_operand(op[1]);
            if (!memory.is_success) return {{}, memory.err, false};
            auto value = evaluate_value(memory.offset, symbols);
            if (!value.is_success) return {{}, value.err, false};
            string reg = "x" + to_string(memory.register_num);
            if (format == "S") return instruction_result(encoder_s(instruction, op[0], reg, value.integer));
            return instruction_result(encoder_i(instruction, op[0], reg, value.integer));
        }
        if (op.size() != 3) return {{}, count_error, false};
        auto value = evaluate_value(op[2], symbols);
        if (!value.is_success) return {{}, value.err, false};
        return instruction_result(encoder_i(instruction, op[0], op[1], value.integer));
    } else if (format == "B") {
        if (op.size() != 3) return {{}, count_error, false};
        auto value = branch_offset(op[2], address, symbols, symbol_sections);
        if (!value.is_success) return {{}, value.err, false};
        return instruction_result(encoder_b(instruction, op[0], op[1], value.integer));
    } else if (format == "U") {
        if (op.size() != 2) return {{}, count_error, false};
        auto value = evaluate_value(op[1], symbols);
        if (!value.is_success) return {{}, value.err, false};
        return instruction_result(encoder_u(instruction, op[0], value.integer));
    } else if (format == "J") {
        if (op.size() != 2) return {{}, count_error, false};
        auto value = branch_offset(op[1], address, symbols, symbol_sections);
        if (!value.is_success) return {{}, value.err, false};
        return instruction_result(encoder_j(instruction, op[0], value.integer));
    } else if (format == "CSR") {
        if (op.size() != 3) return {{}, count_error, false};
        static const set<string> csr_names = {"mstatus", "mie", "mtvec", "mscratch", "mepc", "mcause", "mtval", "mip", "mhartid"};
        if (!csr_names.count(op[1])) {
            auto value = evaluate_value(op[1], symbols);
            if (!value.is_success) return {{}, value.err, false};
            op[1] = to_string(value.integer);
        }
        if (instruction == "csrrwi" || instruction == "csrrsi" || instruction == "csrrci") {
            auto value = evaluate_value(op[2], symbols);
            if (!value.is_success) return {{}, value.err, false};
            op[2] = to_string(value.integer);
        }
        return instruction_result(encoder_csr(instruction, op[0], op[1], op[2]));
    } else if (format == "FIXED") {
        if (!op.empty()) return {{}, count_error, false};
        return instruction_result(encoder_fixed(instruction));
    }
    return {{}, "Invalid instruction: " + instruction, false};
}
