#include "./parser.h"

using namespace std;

parser_rtn parser(string line) {
    set<char> escape_char{'\\', 'n', 'r', 't', 'b', 'f', '\'', '\"', '0'};

    int start = 0;
    while (line.size() > start && (line[start] == ' ' || line[start] == '\t')) start++;

    int state = 0;
    string tmp_line;
    for (int i = start;i < line.size();i++) {
        char c = line[i];
        if (c == '\"' || c == '\'') {
            if (!state) {
                state = (c == '\"') ? 1 : 2;
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
    if (state) return {line, "Error: unclosed quote line ", false};

    while (tmp_line.size() && (tmp_line[tmp_line.size() - 1] == ' ' || tmp_line[tmp_line.size() - 1] == '\t')) tmp_line.pop_back();
    line = tmp_line;
    return {line, "", true};
}