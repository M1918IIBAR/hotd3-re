// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Sega AFS container archive parser

#pragma once

#include "core/types.h"
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <memory>

namespace hotd3::formats {

struct AfsEntryInfo {
    u32 id = 0;
    std::string name;
    u32 offset = 0;
    u32 length = 0;
};

class AfsArchive {
public:
    AfsArchive() = default;
    ~AfsArchive();

    bool open(const std::filesystem::path& path);
    void close();

    bool is_open() const { return file_.is_open(); }
    u32 entry_count() const { return static_cast<u32>(entries_.size()); }
    const std::vector<AfsEntryInfo>& entries() const { return entries_; }

    const AfsEntryInfo* find_entry(const std::string& name) const;
    const AfsEntryInfo* get_entry(u32 index) const;

    // Read full payload of an entry into a byte buffer
    std::vector<u8> read_entry(u32 index);
    std::vector<u8> read_entry(const std::string& name);

private:
    std::ifstream file_;
    std::filesystem::path path_;
    std::vector<AfsEntryInfo> entries_;
};

} // namespace hotd3::formats
