// Test for MelonLibraryScan.h (no JUCE):
//   g++ -std=c++17 -O2 -I.. test_library_scan.cpp -o test_library_scan && ./test_library_scan [folder]
// With a folder argument it prints the list (one relative path per line), so the pedal's
// library.py can be checked against it: both must print the same lines in the same order.
#include "MelonLibraryScan.h"
#include <cstdio>
#include <fstream>

namespace fs = std::filesystem;
static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    failures += ok ? 0 : 1;
}
static void touch (const fs::path& p)
{
    fs::create_directories (p.parent_path());
    std::ofstream (p) << "x";
}

int main (int argc, char** argv)
{
    const std::vector<std::string> ir { "wav", "aif", "aiff", "flac" };
    if (argc > 1)
    {
        for (auto& f : melon::library::scan (argv[1], ir))
            std::printf ("%s\n", f.c_str());
        return 0;
    }

    std::printf ("Melon library scan test\n");
    const auto root = fs::temp_directory_path() / "melon_library_test";
    fs::remove_all (root);
    for (const char* f : { "b.wav", "A.WAV", "a/z.wav", "a/b/c.flac", "Mesa 4x12/V30 SM57.wav", "Ólá/ação.wav", "readme.txt",
                           ".hidden.wav", "._mac.wav", ".importing-x.wav", ".Trashes/t.wav", "a/.git/x.wav", "noext" })
        touch (root / fs::u8path (f));

    const auto list = melon::library::scan (root, ir);
    const std::vector<std::string> expected { "A.WAV", "Mesa 4x12/V30 SM57.wav", "a/b/c.flac", "a/z.wav", "b.wav", "\xC3\x93l\xC3\xA1/a\xC3\xA7\xC3\xA3o.wav" };
    for (auto& f : list)
        std::printf ("    %s\n", f.c_str());
    check (list == expected, "Recursive, extensions case-insensitive, hidden skipped, sorted by UTF-8 bytes");
    check (melon::library::scan (root / "missing", ir).empty(), "Missing folder gives an empty list");

    fs::remove_all (root);
    std::printf (failures == 0 ? "All tests passed\n" : "%d test(s) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
