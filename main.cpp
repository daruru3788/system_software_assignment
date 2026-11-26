#include <iostream>
#include <fstream>
#include "./parser.h"

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
            cout << line_num++ << " " << rtn.result << "\n";
            line_parser_rtn line_rtn = line_parse(rtn.result);
            cout << '[' << line_rtn.label << "] [" << line_rtn.instruction << "] [" << line_rtn.operands << "]\n"; 
        }
        else {
            cerr << rtn.err << line_num++ << "\n";
            return 1;
        }
    }
}