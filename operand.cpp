#include "./operand.h"

using namespace std;

register_parser_rtn parse_register(string regiser_str) {
    if (regiser_str[0] == 'x') {
        if (regiser_str.size() == 1) return {-1, "Invalid register number", false};
        int num = 0;
        for (int i = 1;i < regiser_str.size();i++) {
            int tmp = (int) regiser_str[i] - '0';
            if (tmp < 0 || tmp > 9) return {-1, "Invalid register number", false};
            num = num * 10 + tmp;
            if (num > 31) return {-1, "Invalid register number", false};
        }
        if (num < 0 || num > 31) return {-1, "Invalid register number", false};
        return {num, "", true};
    }

    const unordered_map<string, int> REG = [] {
        unordered_map<string, int> m{
            {"zero", 0}, {"ra", 1}, {"sp", 2}, {"gp", 3}, {"tp", 4},
            {"s0", 8}, {"fp", 8}, {"s1", 9}};
        for (int i = 0; i < 3; ++i) m["t" + to_string(i)] = 5 + i;
        for (int i = 0; i < 8; ++i) m["a" + to_string(i)] = 10 + i;
        for (int i = 2; i < 12; ++i) m["s" + to_string(i)] = 16 + i;
        for (int i = 3; i < 7; ++i) m["t" + to_string(i)] = 25 + i;
        return m;
    }();

    auto reg_num = REG.find(regiser_str);
    if (reg_num == REG.end()) return {-1, "Invalid register number", false};
    return {reg_num->second, "", true};
}

integer_parser_rtn parse_integer(string integer_str) {
    bool is_minus = false;
    if (integer_str[0] == '-') {
        is_minus = true;
        integer_str = integer_str.substr(1);
    }
    int state;
    if (integer_str[0] == '0' && integer_str[0 + 1] == 'b') {
        state = 1;
        integer_str = integer_str.substr(2);
    } else if (integer_str[0] == '0' && integer_str[0 + 1] == 'x') {
        state = 2;
        integer_str = integer_str.substr(2);
    } else state = 0;
    if (integer_str.size() == 0) return {-1, "Invalid integer number", false};

    const uint64_t limit = (uint64_t)MAX_INT + is_minus;
    uint64_t num = 0;
    switch (state)
    {
    case 0:
        for (int i = 0;i < integer_str.size();i++) {
            int tmp = (int) integer_str[i] - '0';
            if (tmp < 0 || tmp > 9) return {-1, "Invalid integer number", false};
            if (num > (limit - tmp) / 10) return {-1, "Integer overflow", false};
            num = num * 10 + tmp;
        }
        break;
    
    case 1:
        for (int i = 0;i < integer_str.size();i++) {
            int tmp = (int) integer_str[i] - '0';
            if (!(tmp == 0 || tmp == 1)) return {-1, "Invalid integer number", false};
            if (num > (limit - tmp) / 2) return {-1, "Integer overflow", false};
            num = num * 2 + tmp;
        }
        break;
        
    case 2:
        transform(integer_str.begin(), integer_str.end(),integer_str.begin(), [](unsigned char c) { return toupper(c); });
        for (int i = 0;i < integer_str.size();i++) {
            int tmp_num = (int) integer_str[i] - '0';
            int tmp_over10 = (int) integer_str[i] - 'A';
            if (!((tmp_num >= 0 && tmp_num <= 9) || (tmp_over10 >= 0 && tmp_over10 <= 5))) return {-1, "Invalid integer number", false};
            if (tmp_num >= 0 && tmp_num <= 9) {
                if (num > (limit - tmp_num) / 16) return {-1, "Integer overflow", false};
                num = num * 16 + tmp_num;
            } else {
                if (num > (limit - 10 - tmp_over10) / 16) return {-1, "Integer overflow", false};
                num = num * 16 + (tmp_over10 + 10);
            }
        }
        break;
    }
    return {is_minus ? (int64_t)(0 - num) : (int64_t)num, "", true};
}

integer_parser_rtn parse_integer_char(string integer_str) {
    if ((integer_str.size() < 3 || !(integer_str.front() == '\'' && integer_str.back() == '\''))) return {-1, "Invalid char value", false};
    set<char> escape_char{'\\', 'n', 'r', 't', 'b', 'f', '\'', '\"', '0'};
    integer_str = integer_str.substr(1, integer_str.size() - 2);
    if (integer_str.size() == 2 && !(integer_str[0] == '\\' && escape_char.count(integer_str[1]))) return {-1, "Invalid char value", false};
    if (integer_str.size() != 1 && integer_str.size() != 2) return {-1, "Invalid char value", false};

    char c;
    if (integer_str.size() == 2) {
        switch (integer_str[1]) {
            case 'n':  c = '\n'; break;
            case 't':  c = '\t'; break;
            case 'r':  c = '\r'; break;
            case '0':  c = '\0'; break;
            case 'a':  c = '\a'; break;
            case 'b':  c = '\b'; break;
            case 'f':  c = '\f'; break;
            case 'v':  c = '\v'; break;
            case '\\': c = '\\'; break;
            case '\'': c = '\''; break;
            case '\"': c = '\"'; break;
        }
    } else c = integer_str[0];

    return {(int64_t) c, "", true};
}