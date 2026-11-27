#include <iostream>
#include <fstream>
#include "./parser.h"
#include "./operand.h"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <path>\n";
        return 1;
    }
    char* path = argv[1];

    ifstream file(path);
    if (!file.is_open()) {
        cerr << "Error: Couldn't open " << path << "\n";
        return 1;
    }

    string line;
    int line_num = 1;
    while (getline(file, line)) {
        parser_rtn rtn = parser(line);
        if (rtn.is_success) {
            cout << line_num << " " << rtn.result << "\n";
            line_parser_rtn line_rtn = line_parse(rtn.result);
            cout << '[' << line_rtn.label << "] [" << line_rtn.instruction << "] [" << line_rtn.operands << "]\n";
            operands_parser_rtn operands_rtn = operands_parse(line_rtn.operands);
            if (!operands_rtn.is_success) {
                cerr << operands_rtn.err << " line: " << line_num << "\n";
                return 1; 
            }
            cout << "명령어: " << line_rtn.instruction << "\n";
            cout << "operand 개수: " << operands_rtn.operands.size() << "\n";
            for (string op : operands_rtn.operands) cout << "[" << op << "]\n";
            line_num++;
        }
        else {
            cerr << rtn.err << line_num << "\n";
            return 1;
        }
    }
}