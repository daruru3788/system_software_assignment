#include "./assembler.h"
#include "./encoder.h"
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

        auto line_parser_return = line_parse(parser_return.result);
        string instruction = line_parser_return.instruction;
        string operands = line_parser_return.operands;
        string label = line_parser_return.label;

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
    auto source_lines = lines.source_lines;
    set<string> exist_list = {"TEXT_START", "_data_start", "_bss_start", "_bss_end", "_stack_top"};
    std::unordered_map<std::string, std::int64_t> constants;
    for (source_line l : source_lines) {
        if (l.instruction != ".equ") continue;
        string error_prefix = "line " + to_string(l.line_num) + ": ";
        if (l.operands.size() != 2) return {{}, error_prefix + "Not valid operands number: " + to_string(l.operands.size()), false};

        string name = l.operands[0];
        string v = l.operands[1];
        if (name.empty()) return {{}, error_prefix + "Empty name", false};
        if (name[0] != '.' && name[0] != '_' && (!(name[0] >= 'A' && name[0] <= 'Z') && !(name[0] >= 'a' && name[0] <= 'z'))) return {{}, error_prefix + "Not valid name: " + name, false};
        bool is_continue = false;
        for (int i = 1;i < name.size();i++) {
            if (name[i] != '.' && name[i] != '_' && (!(name[i] >= 'A' && name[i] <= 'Z') && !(name[i] >= 'a' && name[i] <= 'z')) && !(name[i] >= '0' && name[i] <= '9')) {
                is_continue = true;
                break;
            }
        }
        if (is_continue) return {{}, error_prefix + "Not valid name: " + name, false};
        if (exist_list.count(name)) return {{}, error_prefix + "Duplicated or reserved name: " + name, false};

        integer_parser_rtn integer_rtn;
        if (v[0] == '\'') integer_rtn = parse_integer_char(v);
        else integer_rtn = parse_integer(v);
        if (!integer_rtn.is_success) return {{}, error_prefix + integer_rtn.err, false};

        constants[name] = integer_rtn.integer;
        exist_list.insert(name);
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
        string v = line.operands[1];

        int64_t integer_v;
        if (v[0] == '\'') {
            auto rtn = parse_integer_char(v);
            if (!rtn.is_success) return {0, rtn.err, false};
            integer_v = rtn.integer;
        } else if ((v[0] >= '0' && v[0] <= '9') || v[0] == '-') {
            auto rtn = parse_integer(v);
            if (!rtn.is_success) return {0, rtn.err, false};
            integer_v = rtn.integer;
        } else {
            auto const_map = constants.constants;
            auto num = const_map.find(v);
            if (num == const_map.end()) return {8, "", true};
            integer_v = num->second;
        }

        if (!(integer_v >= -2147483648 && integer_v <= 4294967295)) return {0, "Out of Range", false};
        if (integer_v > 2147483647) integer_v -= 4294967296;
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
        int64_t integer_v;
        if (v[0] == '\'') {
            auto rtn = parse_integer_char(v);
            if (!rtn.is_success) return {0, rtn.err, false};
            integer_v = rtn.integer;
        } else if ((v[0] >= '0' && v[0] <= '9') || v[0] == '-') {
            auto rtn = parse_integer(v);
            if (!rtn.is_success) return {0, rtn.err, false};
            integer_v = rtn.integer;
        } else {
            auto const_map = constants.constants;
            auto num = const_map.find(v);
            if (num == const_map.end()) return {0, "Couldn't find constant", false};
            integer_v = num->second;
        }

        if (integer_v < 0) return {0, "Invalid value", false};
        if (integer_v > INT_MAX) return {0, "Directive size out of range", false};
        return {static_cast<int>(integer_v), "", true};
    } else if (line.instruction == ".align") {
        if (line.operands.size() != 1) return {0, "Invalid operands number", false};
        string v = line.operands[0];
        if (v.empty()) return {0, "Empty operand", false};
        int64_t integer_v;
        if ((v[0] >= '0' && v[0] <= '9') || v[0] == '-') {
            auto rtn = parse_integer(v);
            if (!rtn.is_success) return {0, rtn.err, false};
            integer_v = rtn.integer;
        } else {
            auto const_map = constants.constants;
            auto num = const_map.find(v);
            if (num == const_map.end()) return {0, "Couldn't find constant", false};
            integer_v = num->second;
        }
        if (!(integer_v >= 0 && integer_v < 64)) return {0, "Invalid k", false};
        uint64_t alignment = 1ULL << integer_v;
        uint64_t remainder = current_address % alignment;

        uint64_t padding = (remainder == 0) ? 0 : alignment - remainder;
        if (padding > INT_MAX) return {0, "Directive size out of range", false};
        return {static_cast<int>(padding), "", true};
    } else if (line.instruction == ".ascii" || line.instruction == ".asciz") {
        if (line.operands.size() != 1) return {0, "Invalid operands number", false};
        string op = line.operands[0];
        if (op.size() < 2 || op.front() != '"' || op.back() != '"') return {0, "Invalid string literal", false};
        int cnt = (line.instruction == ".asciz") ? 1 : 0;
        set<char> escape_char{'\\', 'n', 'r', 't', 'b', 'f', '\'', '\"', '0'};
        for (size_t i = 1;i < op.size() - 1;i++) {
            char c = op[i];
            if (c == '"') return {0, "Invalid string literal", false};
            if (c == '\\') {
                if (i + 1 >= op.size() - 1 || !escape_char.count(op[i + 1])) return {0, "Invalid escape sequence", false};
                i++;
            }
            if (cnt == INT_MAX) return {0, "Directive size out of range", false};
            cnt++;
        }
        return {cnt, "", true};
    }

    static const set<string> ZERO_SIZE_DIRECTIVES = {
        ".text", ".data", ".bss", ".equ", ".globl", ".global"
    };
    if (ZERO_SIZE_DIRECTIVES.count(line.instruction)) return {0, "", true};

    return {0, "Invalid directive: " + line.instruction, false};
}
