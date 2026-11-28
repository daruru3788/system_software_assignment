#include "./output.h"
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <cctype>

using namespace std;
namespace fs = std::filesystem;

bool same_file_path(string first, string second) {
    error_code error;
    if (fs::equivalent(first, second, error) && !error) return true;
    error.clear();
    auto first_path = fs::weakly_canonical(first, error);
    if (error) return first == second;
    auto second_path = fs::weakly_canonical(second, error);
    if (error) return first == second;
    string a = first_path.string();
    string b = second_path.string();
#ifdef _WIN32
    for (char& c : a) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    for (char& c : b) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
#endif
    return a == b;
}

static output_rtn write_file(string path, const char* data, size_t size) {
    ofstream file(path, ios::binary);
    if (!file.is_open()) return {"Cannot open output file: " + path, false};
    if (size > 0) file.write(data, static_cast<streamsize>(size));
    file.close();
    if (!file) return {"Failed to write output file: " + path, false};
    return {"", true};
}

output_rtn write_binary(string path, const vector<uint8_t>& bytes) {
    return write_file(path, reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

output_rtn write_listing(string path, const layout_rtn& layout) {
    ostringstream text;
    text << "ADDRESS   MACHINE CODE / BYTES    LINE  SOURCE\n";
    for (const placed_line& item : layout.lines) {
        if (item.bytes.empty()) {
            text << hex << uppercase << setfill('0') << setw(8) << item.address << "  ";
            text << setfill(' ') << setw(22) << "" << dec << setw(5) << item.source.line_num << "  " << item.source.original;
            if (item.byte_size != 0) text << "  [" << item.byte_size << (item.section == "B" ? " BSS bytes]" : " zero bytes]");
            text << '\n';
            continue;
        }
        bool machine = !item.source.instruction.empty() && item.source.instruction[0] != '.';
        for (size_t start = 0;start < item.bytes.size();start += 4) {
            text << hex << uppercase << setfill('0') << setw(8) << item.address + start << "  ";
            if (machine) {
                uint32_t code = 0;
                for (int i = 0;i < 4;i++) code |= static_cast<uint32_t>(item.bytes[start + i]) << (8 * i);
                text << setw(8) << code << "              ";
            } else {
                size_t count = min(static_cast<size_t>(4), item.bytes.size() - start);
                for (size_t i = 0;i < count;i++) text << setw(2) << static_cast<unsigned int>(item.bytes[start + i]) << ' ';
                text << string(22 - 3 * count, ' ');
            }
            text << dec << setfill(' ') << setw(5) << item.source.line_num << "  " << item.source.original << '\n';
        }
    }
    text << "\nSYMBOLS\nVALUE             SECTION GLOBAL NAME\n";
    vector<string> names;
    for (auto item : layout.symbols) names.push_back(item.first);
    sort(names.begin(), names.end());
    for (string name : names) {
        text << hex << uppercase << setfill('0') << setw(16) << static_cast<uint64_t>(layout.symbols.at(name));
        text << "  " << layout.symbol_sections.at(name) << "       " << (layout.globals.count(name) ? "G" : "-") << "    " << name << '\n';
    }
    string result = text.str();
    return write_file(path, result.data(), result.size());
}
