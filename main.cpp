#include <iostream>
#include <fstream>
#include <string>
#include <set>

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

    set<char> escape_char{'\\', 'n', 'r', 't', 'b', 'f', '\'', '\"', '0'};

    string line;
    int line_num = 1;
    int opened_line;
    while (getline(file, line)) {
        int start = 0;
        while (line.size() && line[start] == ' ' || line[start] == '\t') start++;

        int state = 0;
        string tmp_line;
        for (int i = start;i < line.size();i++) {
            char c = line[i];
            if (c == '\"' || c == '\'') {
                if (!state) {
                    state = (c == '\"') ? 1 : 2;
                    opened_line = line_num;
                }
                else if (state == 1 && c == '\"') state = 0;
                else if (state == 2 && c == '\'') state = 0;
            }
            if (state && c == '\\' && line.size() > i + 1 && escape_char.count(line[i + 1])) {
                i++;
                tmp_line.push_back('\\');
            }
            if (c == '#' && !state) break;
            tmp_line.push_back(line[i]);
        }
        if (state) {
            cerr << "Error: unclosed quote line " << opened_line << "\n";
            return 1;
        }

        while (tmp_line.size() && (tmp_line[tmp_line.size() - 1] == ' ' || tmp_line[tmp_line.size() - 1] == '\t')) tmp_line.pop_back();
        line = tmp_line;
        std::cout << line_num++ << " " << line << "\n";
    }
}