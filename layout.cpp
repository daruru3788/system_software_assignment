#include "./layout.h"
#include "./instruction.h"
#include "./value.h"
#include <algorithm>

using namespace std;

static string line_error(source_line line, string message) {
    return "line " + to_string(line.line_num) + ": " + message;
}

layout_rtn layout_sections(source_file source, constants_rtn constants) {
    layout_rtn result{};
    result.text_start = 0x10000;
    if (!source.is_success || !constants.is_success) {
        result.err = !source.is_success ? source.err : constants.err;
        return result;
    }
    result.symbols = constants.constants;
    set<string> names = {"TEXT_START", "_data_start", "_bss_start", "_bss_end", "_stack_top"};
    for (auto item : constants.constants) {
        names.insert(item.first);
        result.symbol_sections[item.first] = "=";
    }
    result.symbols["TEXT_START"] = result.text_start;
    result.symbols["_stack_top"] = 0x40000;
    result.symbol_sections["TEXT_START"] = "T";
    result.symbol_sections["_stack_top"] = "B";

    vector<source_line> sections[3];
    int current_section = 0;
    for (source_line line : source.source_lines) {
        if (!line.label.empty()) {
            if (!valid_symbol_name(line.label)) {
                result.err = line_error(line, "Invalid label: " + line.label);
                return result;
            }
            if (names.count(line.label)) {
                result.err = line_error(line, "Duplicated or reserved name: " + line.label);
                return result;
            }
            names.insert(line.label);
        }
        string instruction = line.instruction;
        if (instruction == ".text" || instruction == ".data" || instruction == ".bss") {
            if (!line.operands.empty()) {
                result.err = line_error(line, "Section directive does not take operands");
                return result;
            }
            // A label before a section switch belongs to the previous section.
            if (!line.label.empty()) {
                source_line label_line = line;
                label_line.instruction.clear();
                sections[current_section].push_back(label_line);
            }
            if (instruction == ".text") current_section = 0;
            else if (instruction == ".data") current_section = 1;
            else current_section = 2;
            continue;
        }
        if (instruction == ".globl" || instruction == ".global") {
            if (line.operands.empty()) {
                result.err = line_error(line, "Missing global symbol");
                return result;
            }
            for (string name : line.operands) {
                if (!valid_symbol_name(name)) {
                    result.err = line_error(line, "Invalid global symbol: " + name);
                    return result;
                }
                result.globals.insert(name);
            }
        }
        sections[current_section].push_back(line);
    }

    uint64_t address = result.text_start;
    const string section_names[3] = {"T", "D", "B"};
    for (int section = 0;section < 3;section++) {
        if (section > 0) address = (address + 15) & ~15ULL;
        if (address > UINT32_MAX) {
            result.err = "Section address exceeds RV32 address space";
            return result;
        }
        if (section == 1) {
            result.data_start = static_cast<uint32_t>(address);
            result.symbols["_data_start"] = address;
            result.symbol_sections["_data_start"] = "D";
        } else if (section == 2) {
            result.bss_start = static_cast<uint32_t>(address);
            result.symbols["_bss_start"] = address;
            result.symbol_sections["_bss_start"] = "B";
        }
        for (source_line line : sections[section]) {
            if (!line.label.empty()) {
                result.symbols[line.label] = address;
                result.symbol_sections[line.label] = section_names[section];
            }
            inst_size_rtn size{0, "", true};
            if (!line.instruction.empty()) {
                bool is_directive = line.instruction[0] == '.';
                bool metadata = line.instruction == ".equ" || line.instruction == ".globl" || line.instruction == ".global";
                if (section == 2 && !(metadata || line.instruction == ".space" || line.instruction == ".align")) {
                    result.err = line_error(line, "Only .space and .align may reserve BSS storage");
                    return result;
                }
                if (!is_directive && section != 0) {
                    result.err = line_error(line, "Instruction outside .text");
                    return result;
                }
                if (!is_directive && address % 4 != 0) {
                    result.err = line_error(line, "Instruction address is not 4-byte aligned");
                    return result;
                }
                if (line.instruction == ".half" && address % 2 != 0) {
                    result.err = line_error(line, "Halfword address is not 2-byte aligned");
                    return result;
                }
                constants_rtn known{result.symbols, "", true};
                if (is_directive) size = directive_size(line, known, static_cast<uint32_t>(address));
                else size = instruction_size(line, known);
                if (!size.is_success) {
                    result.err = line_error(line, size.err);
                    return result;
                }
            }
            if (size.byte_size < 0 || address + static_cast<uint64_t>(size.byte_size) > UINT32_MAX) {
                result.err = line_error(line, "Section size exceeds RV32 address space");
                return result;
            }
            result.lines.push_back({line, section_names[section], static_cast<uint32_t>(address), size.byte_size, {}});
            address += size.byte_size;
        }
        if (section == 1) result.data_end = static_cast<uint32_t>(address);
    }
    result.bss_end = static_cast<uint32_t>(address);
    result.symbols["_bss_end"] = address;
    result.symbol_sections["_bss_end"] = "B";
    result.is_success = true;
    return result;
}

static void append_bytes(vector<uint8_t>& bytes, uint32_t value, int size) {
    for (int i = 0;i < size;i++) bytes.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xFFu));
}

image_rtn generate_image(layout_rtn& layout) {
    if (!layout.is_success) return {{}, layout.err, false};
    vector<uint8_t> image(static_cast<size_t>(layout.data_end - layout.text_start), 0);
    for (placed_line& item : layout.lines) {
        if (item.section == "B") continue;
        source_line line = item.source;
        string instruction = line.instruction;
        vector<uint8_t> bytes;
        if (instruction.empty() || instruction == ".equ" || instruction == ".globl" || instruction == ".global") continue;
        if (instruction[0] != '.') {
            auto rtn = assemble_instruction(line, item.address, layout.symbols, layout.symbol_sections, item.byte_size);
            if (!rtn.is_success) return {{}, line_error(line, rtn.err), false};
            for (uint32_t code : rtn.codes) append_bytes(bytes, code, 4);
        } else if (instruction == ".byte" || instruction == ".half" || instruction == ".word") {
            int unit_size = 1;
            int64_t minimum = -128;
            int64_t maximum = 255;
            if (instruction == ".half") {
                unit_size = 2;
                minimum = -32768;
                maximum = 65535;
            } else if (instruction == ".word") {
                unit_size = 4;
                minimum = -2147483648LL;
                maximum = 4294967295LL;
            }
            for (string operand : line.operands) {
                auto rtn = evaluate_value(operand, layout.symbols);
                if (!rtn.is_success) return {{}, line_error(line, rtn.err), false};
                if (rtn.integer < minimum || rtn.integer > maximum) return {{}, line_error(line, "Data value out of range"), false};
                append_bytes(bytes, static_cast<uint32_t>(rtn.integer), unit_size);
            }
        } else if (instruction == ".ascii" || instruction == ".asciz") {
            auto rtn = parse_string_literal(line.operands[0]);
            if (!rtn.is_success) return {{}, line_error(line, rtn.err), false};
            bytes = rtn.bytes;
            if (instruction == ".asciz") bytes.push_back(0);
        } else if (instruction == ".space" || instruction == ".align") {
            // The image is initialized with zeroes; BSS never enters this image.
            continue;
        } else return {{}, line_error(line, "Invalid directive: " + instruction), false};

        if (bytes.size() != static_cast<size_t>(item.byte_size)) return {{}, line_error(line, "Size changed after layout"), false};
        size_t offset = item.address - layout.text_start;
        if (offset > image.size() || bytes.size() > image.size() - offset) return {{}, line_error(line, "Output offset out of range"), false};
        copy(bytes.begin(), bytes.end(), image.begin() + offset);
        item.bytes = bytes;
    }
    return {image, "", true};
}
