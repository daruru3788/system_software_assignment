#include <iostream>
#include <filesystem>
#include <new>
#include "./assembler.h"
#include "./layout.h"
#include "./output.h"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " input.s [-o output.bin] [--list output.lst]\n";
        return 1;
    }
    string input_path = argv[1];
    string output_path;
    string listing_path;
    for (int i = 2;i < argc;i++) {
        string option = argv[i];
        if ((option != "-o" && option != "--list") || i + 1 >= argc) {
            cerr << "Invalid command line option: " << option << '\n';
            return 1;
        }
        string path = argv[++i];
        if (option == "-o") {
            if (!output_path.empty()) { cerr << "Duplicate -o option\n"; return 1; }
            output_path = path;
        } else {
            if (!listing_path.empty()) { cerr << "Duplicate --list option\n"; return 1; }
            listing_path = path;
        }
    }
    if (output_path.empty()) {
        filesystem::path path(input_path);
        path.replace_extension(".bin");
        output_path = path.string();
    }
    if (same_file_path(input_path, output_path) || (!listing_path.empty() &&
        (same_file_path(input_path, listing_path) || same_file_path(output_path, listing_path)))) {
        cerr << "Input, binary and listing paths must be different\n";
        return 1;
    }
    try {
        auto source = read_source(input_path);
        if (!source.is_success) { cerr << source.err << '\n'; return 1; }
        auto constants = collect_constants(source);
        if (!constants.is_success) { cerr << constants.err << '\n'; return 1; }
        auto layout = layout_sections(source, constants);
        if (!layout.is_success) { cerr << layout.err << '\n'; return 1; }
        auto image = generate_image(layout);
        if (!image.is_success) { cerr << image.err << '\n'; return 1; }
        if (!listing_path.empty()) {
            auto result = write_listing(listing_path, layout);
            if (!result.is_success) { cerr << result.err << '\n'; return 1; }
        }
        auto result = write_binary(output_path, image.bytes);
        if (!result.is_success) { cerr << result.err << '\n'; return 1; }
        cout << "Wrote " << image.bytes.size() << " bytes to " << output_path << '\n';
    } catch (const bad_alloc&) {
        cerr << "Not enough memory to assemble this input\n";
        return 1;
    } catch (const exception& error) {
        cerr << "Assembly failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
