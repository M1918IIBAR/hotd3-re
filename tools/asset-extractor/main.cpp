// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Asset Extractor CLI — Unpacks Sega AFS archives and inspects game data.

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstdint>
#include <cstring>
#include <iomanip>

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct AfsHeader {
    char magic[4];     // "AFS\0"
    uint32_t count;
};

struct AfsEntryRecord {
    uint32_t offset;
    uint32_t length;
};

struct AfsFileDescriptor {
    char name[32];
    uint16_t year;
    uint16_t month;
    uint16_t day;
    uint16_t hour;
    uint16_t minute;
    uint16_t second;
    uint32_t length;
};
#pragma pack(pop)

struct ExtractedEntry {
    std::string name;
    uint32_t offset = 0;
    uint32_t length = 0;
};

bool extract_afs(const fs::path& afs_path, const fs::path& out_dir) {
    std::ifstream file(afs_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[Error] Failed to open AFS file: " << afs_path << "\n";
        return false;
    }

    AfsHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (std::memcmp(header.magic, "AFS\0", 4) != 0) {
        std::cerr << "[Error] Invalid magic (expected 'AFS\\0'): " << afs_path << "\n";
        return false;
    }

    std::cout << "[AFS] " << afs_path.filename().string() 
              << " contains " << header.count << " entries.\n";

    std::vector<AfsEntryRecord> records(header.count);
    file.read(reinterpret_cast<char*>(records.data()), header.count * sizeof(AfsEntryRecord));

    std::vector<ExtractedEntry> entries(header.count);
    for (uint32_t i = 0; i < header.count; ++i) {
        entries[i].offset = records[i].offset;
        entries[i].length = records[i].length;
        entries[i].name = "entry_" + std::to_string(i);
    }

    // Attempt to read the filename directory table if present
    if (header.count > 0 && records[0].offset >= 8 + header.count * 8 + 8) {
        file.seekg(8 + header.count * sizeof(AfsEntryRecord), std::ios::beg);
        uint32_t dir_offset = 0, dir_len = 0;
        file.read(reinterpret_cast<char*>(&dir_offset), sizeof(dir_offset));
        file.read(reinterpret_cast<char*>(&dir_len), sizeof(dir_len));

        if (dir_offset > 0 && dir_offset < fs::file_size(afs_path)) {
            file.seekg(dir_offset, std::ios::beg);
            std::vector<AfsFileDescriptor> descriptors(header.count);
            file.read(reinterpret_cast<char*>(descriptors.data()), header.count * sizeof(AfsFileDescriptor));

            for (uint32_t i = 0; i < header.count; ++i) {
                char clean_name[33] = {};
                std::memcpy(clean_name, descriptors[i].name, 32);
                clean_name[32] = '\0';
                if (std::strlen(clean_name) > 0) {
                    fs::path p(clean_name);
                    entries[i].name = p.filename().string();
                }
            }
        }
    }

    fs::create_directories(out_dir);

    // Extract each entry
    std::vector<char> buffer;
    for (uint32_t i = 0; i < header.count; ++i) {
        const auto& entry = entries[i];
        if (entry.length == 0) continue;

        buffer.resize(entry.length);
        file.seekg(entry.offset, std::ios::beg);
        file.read(buffer.data(), entry.length);

        fs::path dest = out_dir / entry.name;

        // Auto-detect extension if generic name was used
        if (entry.name.rfind("entry_", 0) == 0 && entry.length >= 4) {
            if (std::memcmp(buffer.data(), "PK\x03\x04", 4) == 0) {
                dest += ".zip";
            } else if (static_cast<uint8_t>(buffer[0]) == 0x80 && static_cast<uint8_t>(buffer[1]) == 0x00) {
                dest += ".adx";
            } else if (std::memcmp(buffer.data(), "XMDL", 4) == 0 || std::memcmp(buffer.data(), "LDMX", 4) == 0) {
                dest += ".xmx";
            }
        }

        std::ofstream out(dest, std::ios::binary);
        if (out.is_open()) {
            out.write(buffer.data(), entry.length);
        }
    }

    std::cout << "[AFS] Successfully extracted " << header.count 
              << " files to " << out_dir.string() << "\n";
    return true;
}

int main(int argc, char* argv[]) {
    std::cout << "=== HOTD3 Asset Extractor v0.1.0 ===\n\n";

    if (argc < 2) {
        std::cout << "Usage:\n";
        std::cout << "  hotd3-asset-extractor <archive.afs> [output_directory]\n";
        std::cout << "  hotd3-asset-extractor --all <game_data_path> [output_directory]\n\n";
        return 1;
    }

    std::string arg1 = argv[1];

    if (arg1 == "--all" && argc >= 3) {
        fs::path data_dir = argv[2];
        fs::path out_root = (argc >= 4) ? argv[3] : (data_dir / "extracted");

        fs::path fs_dir = data_dir / "fs";
        if (!fs::exists(fs_dir)) {
            fs_dir = data_dir;
        }

        for (const auto& entry : fs::directory_iterator(fs_dir)) {
            if (entry.path().extension() == ".AFS" || entry.path().extension() == ".afs") {
                fs::path target = out_root / entry.path().stem();
                extract_afs(entry.path(), target);
            }
        }
        return 0;
    }

    fs::path afs_file = arg1;
    fs::path out_dir = (argc >= 3) ? argv[2] : (afs_file.parent_path() / afs_file.stem());

    if (!extract_afs(afs_file, out_dir)) {
        return 1;
    }

    return 0;
}
