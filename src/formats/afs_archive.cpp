// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "formats/afs_archive.h"
#include <cstring>
#include <algorithm>
#include <iostream>

namespace hotd3::formats {

#pragma pack(push, 1)
struct RawAfsHeader {
    char magic[4];     // "AFS\0"
    u32 file_count;
};

struct RawAfsRecord {
    u32 offset;
    u32 length;
};

struct RawAfsDescriptor {
    char name[32];
    u16 year;
    u16 month;
    u16 day;
    u16 hour;
    u16 minute;
    u16 second;
    u32 length;
};
#pragma pack(pop)

AfsArchive::~AfsArchive() {
    close();
}

bool AfsArchive::open(const std::filesystem::path& path) {
    close();

    file_.open(path, std::ios::binary);
    if (!file_.is_open()) {
        std::cerr << "[AfsArchive] Failed to open: " << path << "\n";
        return false;
    }

    RawAfsHeader header{};
    file_.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (std::memcmp(header.magic, "AFS\0", 4) != 0) {
        std::cerr << "[AfsArchive] Invalid magic in: " << path << "\n";
        close();
        return false;
    }

    path_ = path;
    entries_.resize(header.file_count);

    std::vector<RawAfsRecord> records(header.file_count);
    file_.read(reinterpret_cast<char*>(records.data()), header.file_count * sizeof(RawAfsRecord));

    for (u32 i = 0; i < header.file_count; ++i) {
        entries_[i].id = i;
        entries_[i].offset = records[i].offset;
        entries_[i].length = records[i].length;
        entries_[i].name = "entry_" + std::to_string(i);
    }

    // Read filename descriptors if present
    if (header.file_count > 0 && records[0].offset >= 8 + header.file_count * 8 + 8) {
        file_.seekg(8 + header.file_count * sizeof(RawAfsRecord), std::ios::beg);
        u32 dir_offset = 0;
        u32 dir_len = 0;
        file_.read(reinterpret_cast<char*>(&dir_offset), sizeof(dir_offset));
        file_.read(reinterpret_cast<char*>(&dir_len), sizeof(dir_len));

        auto file_size = std::filesystem::file_size(path);
        if (dir_offset > 0 && dir_offset < file_size) {
            file_.seekg(dir_offset, std::ios::beg);
            std::vector<RawAfsDescriptor> descriptors(header.file_count);
            file_.read(reinterpret_cast<char*>(descriptors.data()), header.file_count * sizeof(RawAfsDescriptor));

            for (u32 i = 0; i < header.file_count; ++i) {
                char clean_name[33] = {};
                std::memcpy(clean_name, descriptors[i].name, 32);
                clean_name[32] = '\0';
                std::string s(clean_name);
                // Extract filename only, stripping any relative directory prefixes
                auto slash = s.find_last_of("/\\");
                if (slash != std::string::npos) {
                    s = s.substr(slash + 1);
                }
                if (!s.empty()) {
                    entries_[i].name = s;
                }
            }
        }
    }

    return true;
}

void AfsArchive::close() {
    if (file_.is_open()) {
        file_.close();
    }
    entries_.clear();
}

const AfsEntryInfo* AfsArchive::find_entry(const std::string& name) const {
    auto it = std::find_if(entries_.begin(), entries_.end(),
        [&name](const AfsEntryInfo& e) { return e.name == name; });
    return it != entries_.end() ? &(*it) : nullptr;
}

const AfsEntryInfo* AfsArchive::get_entry(u32 index) const {
    if (index < entries_.size()) {
        return &entries_[index];
    }
    return nullptr;
}

std::vector<u8> AfsArchive::read_entry(u32 index) {
    if (index >= entries_.size() || !file_.is_open()) {
        return {};
    }

    const auto& e = entries_[index];
    if (e.length == 0) {
        return {};
    }

    std::vector<u8> buffer(e.length);
    file_.seekg(e.offset, std::ios::beg);
    file_.read(reinterpret_cast<char*>(buffer.data()), e.length);
    return buffer;
}

std::vector<u8> AfsArchive::read_entry(const std::string& name) {
    const auto* e = find_entry(name);
    return e ? read_entry(e->id) : std::vector<u8>{};
}

} // namespace hotd3::formats
