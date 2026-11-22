// MelonLibraryScan.h - the file list behind the "library index" parameters of Melon NAM
// (model_index) and Melon Cab (ir_index). No JUCE: also built by the tests.
//
// The list is every file under `root` (all sub-folders) whose extension is in `exts`
// (case-insensitive), skipping anything whose name, or any parent folder's name inside
// root, starts with '.' (hidden files, "._" macOS files, ".importing-" temp files).
// Entries are paths relative to root with '/' separators, sorted by their UTF-8 bytes.
//
// This order is a contract with the pedal controller (melon_pedal, library.py), which
// turns a preset's relative path into the index:
//     sorted(paths, key=lambda p: p.encode("utf-8"))
// Change both together, or presets will pick the wrong file.
#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace melon::library
{
inline std::string toUtf8 (const std::filesystem::path& p)
{
    const auto s = p.generic_u8string(); // std::string in C++17, std::u8string in C++20
    return std::string (s.begin(), s.end());
}

// UTF-8 text -> path (on Windows a plain std::string would be read in the ANSI code page)
inline std::filesystem::path fromUtf8 (const std::string& s)
{
   #if defined(__cpp_char8_t)
    return std::filesystem::path (std::u8string (s.begin(), s.end()));
   #else
    return std::filesystem::u8path (s);
   #endif
}

inline std::string lower (std::string s)
{
    for (auto& c : s)
        c = (char) std::tolower ((unsigned char) c);
    return s;
}

// exts without the dot, lower case: { "nam", "aidax", "json" }
inline std::vector<std::string> scan (const std::filesystem::path& root, const std::vector<std::string>& exts)
{
    namespace fs = std::filesystem;
    std::vector<std::string> out;
    std::error_code ec;
    if (! fs::is_directory (root, ec))
        return out;

    // Does not follow symlinked folders (no loops); symlinked files are listed.
    fs::recursive_directory_iterator it (root, fs::directory_options::skip_permission_denied, ec), end;
    for (; ! ec && it != end; it.increment (ec))
    {
        const auto& entry = *it;
        const auto  name  = toUtf8 (entry.path().filename());
        if (name.empty() || name[0] == '.')
        {
            if (entry.is_directory (ec))
                it.disable_recursion_pending(); // skip hidden folders and everything inside
            continue;
        }
        if (! entry.is_regular_file (ec))
            continue;

        const auto ext = lower (toUtf8 (entry.path().extension()));
        if (ext.size() < 2 || std::find (exts.begin(), exts.end(), ext.substr (1)) == exts.end())
            continue;

        out.push_back (toUtf8 (entry.path().lexically_relative (root)));
    }

    std::sort (out.begin(), out.end()); // std::string compares bytes as unsigned char = UTF-8 byte order
    return out;
}

} // namespace melon::library
