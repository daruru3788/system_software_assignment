#include "./assembler.h"

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