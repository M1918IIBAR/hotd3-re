// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Central registry for loading game assets and formats

#pragma once

#include "core/types.h"
#include "formats/afs_archive.h"
#include "formats/dds_loader.h"
#include "formats/xmx_loader.h"

#include <string>
#include <unordered_map>
#include <memory>
#include <filesystem>

namespace hotd3::formats {

class FormatRegistry {
public:
    static FormatRegistry& instance();

    bool init(const std::filesystem::path& game_data_path);
    void shutdown();

    // Accessors for major archives
    AfsArchive* get_archive(const std::string& name);

    // Helpers to load textures and meshes
    ParsedTexture load_texture(const std::string& relative_path);
    ParsedModel   load_model(const std::string& relative_path);

    const std::filesystem::path& game_data_path() const { return game_data_path_; }
    const std::filesystem::path& extracted_path() const { return extracted_path_; }

private:
    FormatRegistry() = default;
    ~FormatRegistry() = default;

    std::filesystem::path game_data_path_;
    std::filesystem::path extracted_path_;
    std::unordered_map<std::string, std::unique_ptr<AfsArchive>> archives_;
};

} // namespace hotd3::formats
