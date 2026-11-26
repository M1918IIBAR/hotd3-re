// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "formats/format_registry.h"
#include <iostream>

namespace hotd3::formats {

FormatRegistry& FormatRegistry::instance() {
    static FormatRegistry s_instance;
    return s_instance;
}

bool FormatRegistry::init(const std::filesystem::path& game_data_path) {
    game_data_path_ = game_data_path;
    extracted_path_ = game_data_path.parent_path() / "extracted";

    std::cout << "[FormatRegistry] Initialized with game data: " << game_data_path_ << "\n";
    return true;
}

void FormatRegistry::shutdown() {
    archives_.clear();
}

AfsArchive* FormatRegistry::get_archive(const std::string& name) {
    auto it = archives_.find(name);
    if (it != archives_.end()) {
        return it->second.get();
    }

    auto afs_path = game_data_path_ / "fs" / (name + ".AFS");
    if (!std::filesystem::exists(afs_path)) {
        return nullptr;
    }

    auto archive = std::make_unique<AfsArchive>();
    if (!archive->open(afs_path)) {
        return nullptr;
    }

    auto* ptr = archive.get();
    archives_[name] = std::move(archive);
    return ptr;
}

ParsedTexture FormatRegistry::load_texture(const std::string& relative_path) {
    // Try extracted folder first
    auto direct_path = extracted_path_ / relative_path;
    if (std::filesystem::exists(direct_path)) {
        return DdsLoader::load_from_file(direct_path.string());
    }

    // Try relative to game_data
    auto game_path = game_data_path_ / relative_path;
    if (std::filesystem::exists(game_path)) {
        return DdsLoader::load_from_file(game_path.string());
    }

    return {};
}

ParsedModel FormatRegistry::load_model(const std::string& relative_path) {
    auto direct_path = extracted_path_ / relative_path;
    if (std::filesystem::exists(direct_path)) {
        return XmxLoader::load_from_file(direct_path.string());
    }

    auto game_path = game_data_path_ / relative_path;
    if (std::filesystem::exists(game_path)) {
        return XmxLoader::load_from_file(game_path.string());
    }

    return {};
}

} // namespace hotd3::formats
