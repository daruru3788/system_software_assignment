#include "./value.h"
#include <climits>
#include <cctype>

using namespace std;

string trim_text(string text) {
    size_t start = 0;
    while (start < text.size() && isspace(static_cast<unsigned char>(text[start]))) start++;
    size_t end = text.size();
    while (end > start && isspace(static_cast<unsigned char>(text[end - 1]))) end--;
    return text.substr(start, end - start);
}

bool valid_symbol_name(string name) {
    if (name.empty()) return false;
    for (size_t i = 0;i < name.size();i++) {
        char c = name[i];
        bool letter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        bool digit = c >= '0' && c <= '9';
        if (!(letter || c == '_' || c == '.' || (i > 0 && digit))) return false;
    }
    return true;
}

integer_parser_rtn normalize_word(int64_t value) {
    if (value < -2147483648LL || value > 4294967295LL) return {0, "32-bit value out of range", false};
    if (value > 2147483647LL) value -= 4294967296LL;
    return {value, "", true};
}

string_rtn parse_string_literal(string text) {
    if (text.size() < 2 || text.front() != '"' || text.back() != '"') return {{}, "Invalid string literal", false};
    vector<uint8_t> bytes;
    for (size_t i = 1;i < text.size() - 1;i++) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == '"') return {{}, "Invalid string literal", false};
        if (c == '\\') {
            if (i + 1 >= text.size() - 1) return {{}, "Incomplete escape sequence", false};
            char escape = text[++i];
            if (escape == 'n') c = '\n';
            else if (escape == 'r') c = '\r';
            else if (escape == 't') c = '\t';
            else if (escape == 'b') c = '\b';
            else if (escape == 'f') c = '\f';
            else if (escape == '0') c = 0;
            else if (escape == '\\' || escape == '\'' || escape == '"') c = escape;
            else return {{}, "Invalid escape sequence", false};
        }
        bytes.push_back(c);
    }
    return {bytes, "", true};
}

struct expression_state {
    string text;
    size_t position;
    const unordered_map<string, int64_t>* symbols;
    bool has_symbol;
    int depth;
};

static void skip_space(expression_state& state) {
    while (state.position < state.text.size() && isspace(static_cast<unsigned char>(state.text[state.position]))) state.position++;
}

static value_rtn expression_error(string err, bool unresolved = false) {
    return {0, err, false, unresolved, false};
}

static int operator_priority(string op) {
    if (op == "|") return 1;
    if (op == "^") return 2;
    if (op == "&") return 3;
    if (op == "<<" || op == ">>") return 4;
    if (op == "+" || op == "-") return 5;
    if (op == "*" || op == "/" || op == "%") return 6;
    return 0;
}

static value_rtn calculate_operator(int64_t a, int64_t b, string op) {
    int64_t result = 0;
    if (op == "+") {
        if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) return expression_error("Expression overflow");
        result = a + b;
    } else if (op == "-") {
        if ((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b)) return expression_error("Expression overflow");
        result = a - b;
    } else if (op == "*") {
        if (a > 0) {
            if ((b > 0 && a > INT64_MAX / b) || (b < 0 && b < INT64_MIN / a)) return expression_error("Expression overflow");
        } else if (a < 0) {
            if ((b > 0 && a < INT64_MIN / b) || (b < 0 && a < INT64_MAX / b)) return expression_error("Expression overflow");
        }
        result = a * b;
    } else if (op == "/" || op == "%") {
        if (b == 0) return expression_error("Division by zero");
        if (a == INT64_MIN && b == -1) return expression_error("Expression overflow");
        result = (op == "/") ? a / b : a % b;
    } else if (op == "<<") {
        if (b < 0 || b >= 64) return expression_error("Invalid shift count");
        if (a >= 0) {
            if (static_cast<uint64_t>(a) > (static_cast<uint64_t>(INT64_MAX) >> b)) return expression_error("Expression overflow");
            result = static_cast<int64_t>(static_cast<uint64_t>(a) << b);
        } else {
            uint64_t magnitude = static_cast<uint64_t>(-(a + 1)) + 1;
            if (magnitude > ((1ULL << 63) >> b)) return expression_error("Expression overflow");
            magnitude <<= b;
            result = (magnitude == (1ULL << 63)) ? INT64_MIN : -static_cast<int64_t>(magnitude);
        }
    } else if (op == ">>") {
        if (b < 0 || b >= 64) return expression_error("Invalid shift count");
        if (a >= 0) result = a >> b;
        else result = -1 - static_cast<int64_t>(static_cast<uint64_t>(-(a + 1)) >> b);
    } else if (op == "&") result = a & b;
    else if (op == "^") result = a ^ b;
    else if (op == "|") result = a | b;
    else return expression_error("Invalid operator");
    return {result, "", true, false, false};
}

static value_rtn parse_expression(expression_state& state, int min_priority);

static value_rtn parse_primary(expression_state& state) {
    skip_space(state);
    if (state.position >= state.text.size()) return expression_error("Missing expression value");
    if (state.depth >= 128) return expression_error("Expression nesting too deep");
    char c = state.text[state.position];
    if (c == '+' || c == '-' || c == '~') {
        state.position++;
        skip_space(state);
        // Let the integer parser accept the magnitude of INT64_MIN.
        if (c == '-' && state.position < state.text.size() && isdigit(static_cast<unsigned char>(state.text[state.position]))) {
            size_t start = state.position;
            while (state.position < state.text.size() && isalnum(static_cast<unsigned char>(state.text[state.position]))) state.position++;
            auto rtn = parse_integer("-" + state.text.substr(start, state.position - start));
            if (!rtn.is_success) return expression_error(rtn.err);
            return {rtn.integer, "", true, false, false};
        }
        state.depth++;
        auto rtn = parse_primary(state);
        state.depth--;
        if (!rtn.is_success) return rtn;
        if (c == '-' && rtn.integer == INT64_MIN) return expression_error("Expression overflow");
        if (c == '-') rtn.integer = -rtn.integer;
        else if (c == '~') rtn.integer = ~rtn.integer;
        return rtn;
    }
    if (c == '(') {
        state.position++;
        state.depth++;
        auto rtn = parse_expression(state, 1);
        state.depth--;
        if (!rtn.is_success) return rtn;
        skip_space(state);
        if (state.position >= state.text.size() || state.text[state.position] != ')') return expression_error("Missing closing parenthesis");
        state.position++;
        return rtn;
    }
    if (c == '\'') {
        size_t start = state.position++;
        bool closed = false;
        while (state.position < state.text.size()) {
            char next = state.text[state.position++];
            if (next == '\\') {
                if (state.position >= state.text.size()) break;
                state.position++;
            } else if (next == '\'') {
                closed = true;
                break;
            }
        }
        if (!closed) return expression_error("Unclosed character literal");
        auto rtn = parse_integer_char(state.text.substr(start, state.position - start));
        if (!rtn.is_success) return expression_error(rtn.err);
        return {rtn.integer, "", true, false, false};
    }
    size_t start = state.position;
    while (state.position < state.text.size()) {
        char next = state.text[state.position];
        if (!(isalnum(static_cast<unsigned char>(next)) || next == '_' || next == '.')) break;
        state.position++;
    }
    if (start == state.position) return expression_error("Invalid expression token");
    string token = state.text.substr(start, state.position - start);
    if (isdigit(static_cast<unsigned char>(token[0]))) {
        auto rtn = parse_integer(token);
        if (!rtn.is_success) return expression_error(rtn.err);
        return {rtn.integer, "", true, false, false};
    }
    if (!valid_symbol_name(token)) return expression_error("Invalid symbol: " + token);
    state.has_symbol = true;
    auto found = state.symbols->find(token);
    if (found == state.symbols->end()) return expression_error("Undefined symbol: " + token, true);
    return {found->second, "", true, false, true};
}

static value_rtn parse_expression(expression_state& state, int min_priority) {
    auto left = parse_primary(state);
    if (!left.is_success) return left;
    while (true) {
        skip_space(state);
        if (state.position >= state.text.size()) break;
        string op = state.text.substr(state.position, 1);
        if (state.position + 1 < state.text.size()) {
            string pair = state.text.substr(state.position, 2);
            if (pair == "<<" || pair == ">>") op = pair;
        }
        int priority = operator_priority(op);
        if (priority < min_priority) break;
        state.position += op.size();
        auto right = parse_expression(state, priority + 1);
        if (!right.is_success) return right;
        left = calculate_operator(left.integer, right.integer, op);
        if (!left.is_success) return left;
    }
    return left;
}

value_rtn evaluate_value(string expression, const unordered_map<string, int64_t>& symbols) {
    expression_state state{expression, 0, &symbols, false, 0};
    auto rtn = parse_expression(state, 1);
    if (!rtn.is_success) return rtn;
    skip_space(state);
    if (state.position != expression.size()) return expression_error("Unexpected expression text: " + expression.substr(state.position));
    rtn.has_symbol = state.has_symbol;
    return rtn;
}
