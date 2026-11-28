#include "./assembler.h"
#include "./encoder.h"
#include "./value.h"
#include <climits>

using namespace std;

source_file read_source(std::string path) {
    ifstream file(path);
    if (!file.is_open()) return {{}, "Error: Couldn't open file", false};

    vector<source_line> source_lines;
    string original;
    int line_num = 0;
    while (getline(file, original)) {
        line_num++;

        string clean = original;
        if (!clean.empty() && clean.back() == '\r')
            clean.pop_back();

        auto parser_return = parser(clean);
        if (!parser_return.is_success) return {source_lines, "line "+ to_string(line_num) + ": " + parser_return.err, false};
        if (!parser_return.result.empty() && parser_return.result[0] == ':') return {source_lines, "line " + to_string(line_num) + ": Empty label", false};

        auto line_parser_return = line_parse(parser_return.result);
        string instruction = line_parser_return.instruction;
        string operands = line_parser_return.operands;
        string label = trim_text(line_parser_return.label);

        if (label.empty() && operands.empty() && instruction.empty()) continue;

        auto operands_parser_return = operands_parse(operands);
        if (!operands_parser_return.is_success) return {source_lines, "line "+ to_string(line_num) + ": " + operands_parser_return.err, false};
        auto op = operands_parser_return.operands;

        source_lines.push_back({line_num, original, label, instruction, op});
    }

    if (file.bad()) return {source_lines, "file bad", false};
    return {source_lines, "", true};
}

constants_rtn collect_constants(source_file lines) {
    if (!lines.is_success) return {{}, lines.err, false};
    set<string> exist_list = {"TEXT_START", "_data_start", "_bss_start", "_bss_end", "_stack_top"};
    std::unordered_map<std::string, std::int64_t> constants;
    vector<source_line> pending;
    for (source_line l : lines.source_lines) {
        if (l.instruction != ".equ") continue;
        string error_prefix = "line " + to_string(l.line_num) + ": ";
        if (l.operands.size() != 2) return {{}, error_prefix + "Not valid operands number: " + to_string(l.operands.size()), false};

        string name = l.operands[0];
        if (!valid_symbol_name(name)) return {{}, error_prefix + "Not valid name: " + name, false};
        if (exist_list.count(name)) return {{}, error_prefix + "Duplicated or reserved name: " + name, false};
        exist_list.insert(name);
        pending.push_back(l);
    }
    while (!pending.empty()) {
        vector<source_line> next;
        auto known = constants;
        known["TEXT_START"] = 0x10000;
        known["_stack_top"] = 0x40000;
        string last_error;
        for (source_line l : pending) {
            auto rtn = evaluate_value(l.operands[1], known);
            if (!rtn.is_success) {
                last_error = "line " + to_string(l.line_num) + ": " + rtn.err;
                if (!rtn.is_unresolved) return {{}, last_error, false};
                next.push_back(l);
            } else {
                constants[l.operands[0]] = rtn.integer;
                known[l.operands[0]] = rtn.integer;
            }
        }
        if (next.size() == pending.size()) return {{}, last_error + " (unresolved or cyclic .equ)", false};
        pending = next;
    }

    return {constants, "", true};
}

inst_size_rtn instruction_size(source_line line, constants_rtn constants) {
    string instruction = line.instruction;
    if (instruction == "la"){
        if (line.operands.size() != 2) return {0, "wrong number of operands", false};
        return {8, "", true};
    }
    if (instruction == "li") {
        if (line.operands.size() != 2) return {0, "wrong number of operands", false};
        auto value = evaluate_value(line.operands[1], constants.constants);
        if (!value.is_success) {
            if (value.is_unresolved) return {8, "", true};
            return {0, value.err, false};
        }
        auto normalized = normalize_word(value.integer);
        if (!normalized.is_success) return {0, normalized.err, false};
        int64_t integer_v = normalized.integer;
        if (integer_v >= -2048 && integer_v <= 2047) return {4, "", true};
        else return {8, "", true};
    }

    static const set<string> PSEUDO_INSTRUCTIONS = {
        "mv", "nop", "not", "neg",
        "j", "jr", "jal", "call", "ret",
        "beqz", "bnez", "bgt", "ble", "bgtu", "bleu",
        "csrr", "csrw", "csrsi", "csrci",
        "csrs", "bgtz", "bltz"
    };
    if (PSEUDO_INSTRUCTIONS.count(instruction)) return {4, "", true};
    if (is_real_instruction(instruction)) return {4, "", true};

    return {0, "Invalid instruction: " + instruction, false};
}

inst_size_rtn directive_size(source_line line, constants_rtn constants, std::uint32_t current_address) {
    if (line.instruction == ".byte" || line.instruction == ".half" || line.instruction == ".word") {
        if (line.operands.empty()) return {0, "Invalid operands number", false};
        int unit_size = 1;
        if (line.instruction == ".half") unit_size = 2;
        else if (line.instruction == ".word") unit_size = 4;

        if (line.operands.size() > static_cast<size_t>(INT_MAX / unit_size)) return {0, "Directive size out of range", false};
        int byte_size = static_cast<int>(line.operands.size()) * unit_size;
        return {byte_size, "", true};
    } else if (line.instruction == ".space") {
        if (line.operands.size() != 1) return {0, "Invalid operands number", false};
        string v = line.operands[0];
        if (v.empty()) return {0, "Empty operand", false};
        auto rtn = evaluate_value(v, constants.constants);
        if (!rtn.is_success) return {0, rtn.err, false};
        int64_t integer_v = rtn.integer;

        if (integer_v < 0) return {0, "Invalid value", false};
        if (integer_v > INT_MAX) return {0, "Directive size out of range", false};
        return {static_cast<int>(integer_v), "", true};
    } else if (line.instruction == ".align") {
        if (line.operands.size() != 1) return {0, "Invalid operands number", false};
        string v = line.operands[0];
        if (v.empty()) return {0, "Empty operand", false};
        auto rtn = evaluate_value(v, constants.constants);
        if (!rtn.is_success) return {0, rtn.err, false};
        int64_t integer_v = rtn.integer;
        if (!(integer_v >= 0 && integer_v < 64)) return {0, "Invalid k", false};
        uint64_t alignment = 1ULL << integer_v;
        uint64_t remainder = current_address % alignment;

        uint64_t padding = (remainder == 0) ? 0 : alignment - remainder;
        if (padding > INT_MAX) return {0, "Directive size out of range", false};
        return {static_cast<int>(padding), "", true};
    } else if (line.instruction == ".ascii" || line.instruction == ".asciz") {
        if (line.operands.size() != 1) return {0, "Invalid operands number", false};
        auto rtn = parse_string_literal(line.operands[0]);
        if (!rtn.is_success) return {0, rtn.err, false};
        size_t extra = (line.instruction == ".asciz") ? 1 : 0;
        if (rtn.bytes.size() > static_cast<size_t>(INT_MAX) - extra) return {0, "Directive size out of range", false};
        return {static_cast<int>(rtn.bytes.size() + extra), "", true};
    }

    static const set<string> ZERO_SIZE_DIRECTIVES = {
        ".text", ".data", ".bss", ".equ", ".globl", ".global"
    };
    if (ZERO_SIZE_DIRECTIVES.count(line.instruction)) return {0, "", true};

    return {0, "Invalid directive: " + line.instruction, false};
}
