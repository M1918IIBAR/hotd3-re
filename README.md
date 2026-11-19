# hotd3-re — The House of the Dead III: Reverse Engineered

<p align="center">
  <strong>An open-source, clean-room reverse engineering of The House of the Dead III (PC, 2005)</strong><br>
  <em>64-bit native · DirectX 11 / Vulkan · Cross-platform</em>
</p>

---

Completing the trilogy alongside:
- 🧟 [**ReWrithe**](https://github.com/TawhidI-602/The-House-of-the-Dead-ReWrithe) — The House of the Dead (1996/1998)
- 🧟 [**hotd2-re**](https://github.com/GitFist/hotd2-re) — The House of the Dead 2

## About

**hotd3-re** is a reverse-engineered reimplementation of *The House of the Dead III* PC port (`hod3pc.exe`). The goal is to produce a fully playable, modern, 64-bit native executable that:

- 🖥️ Runs natively on **Windows, Linux, and macOS**
- 🎮 Supports **DX11 and Vulkan** rendering backends
- 🔫 Has first-class **light-gun support** (Sinden, GUN4IR, AimTrak)
- 📺 Supports **arbitrary resolutions**, widescreen, ultrawide, and 4K/8K
- ⚡ Runs at **unlocked framerates** with proper frame pacing
- 🕹️ Offers both **arcade-accurate** and **enhanced** modes

## ⚠️ Legal Notice

This repository contains **only original source code** written by the project contributors. **No copyrighted game assets, data files, textures, models, sounds, or videos from Sega are included.**

To run hotd3-re, you must supply your own legally obtained copy of *The House of the Dead III* for PC. The setup utility will read and validate your game files.

## Building

### Prerequisites

- **CMake** 3.20+
- **C++20** compatible compiler (MSVC 2022, GCC 12+, Clang 15+)
- **SDL2** development libraries
- **DirectX 11** SDK (Windows, included with Windows SDK)
- **Vulkan SDK** (optional, for Vulkan backend)

### Build Steps

```bash
# Clone the repository
git clone https://github.com/YOUR_USERNAME/hotd3-re.git
cd hotd3-re

# Configure (DX11 on Windows)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Configure (Vulkan on any platform)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DHOTD3_BUILD_VULKAN=ON -DHOTD3_BUILD_DX11=OFF

# Build
cmake --build build --config Release

# Run (after providing game files)
./build/hotd3-re --game-data /path/to/your/hotd3/files
```

### vcpkg (Windows)
```bash
vcpkg install sdl2:x64-windows
cmake -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
```

## Project Structure

```
hotd3-re/
├── docs/                    # RE documentation, memory maps, format specs
├── tools/                   # Asset extractor, Ghidra scripts
│   ├── asset-extractor/     # Extract & inspect original game data files
│   └── ghidra-scripts/      # Automated analysis scripts for Ghidra
├── src/
│   ├── main.cpp             # Entry point
│   ├── core/                # Game loop, state machine, timer, config
│   ├── render/              # Abstracted rendering
│   │   ├── renderer.h       # Abstract renderer interface
│   │   ├── dx11/            # DirectX 11 backend
│   │   ├── vulkan/          # Vulkan backend
│   │   └── common/          # Shared vertex formats, shader defs
│   ├── audio/               # Sound engine (OpenAL Soft)
│   ├── input/               # Input abstraction (mouse, gun, gamepad)
│   ├── physics/             # Hit detection, raycasting
│   ├── scene/               # Chapter/level management
│   ├── ui/                  # HUD, menus, credit system
│   ├── video/               # FMV/cutscene playback
│   └── formats/             # Original data file parsers
├── assets/                  # YOUR game files go here (gitignored)
├── lib/                     # Third-party dependencies
├── CMakeLists.txt
└── README.md
```

## Status

> 🚧 **Early Development** — Phase 0 (Reverse Engineering & Documentation)

- [ ] Binary analysis and function labeling in Ghidra
- [ ] Data file format documentation
- [ ] Asset extractor tool
- [ ] Skeleton engine with SDL2 window
- [ ] DX11 rendering backend
- [ ] Title screen rendering
- [ ] Game state machine
- [ ] Input system
- [ ] Audio engine
- [ ] Gameplay systems
- [ ] Vulkan rendering backend
- [ ] Cross-platform builds

## Contributing

Contributions are welcome! Key areas where help is needed:

1. **Reverse engineering** — Analyzing `hod3pc.exe` in Ghidra/IDA and documenting functions
2. **Data format research** — Understanding original texture, model, sound, and level formats
3. **Rendering** — Implementing DX11/Vulkan backends
4. **Testing** — Comparing behavior against the original game

Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## Acknowledgements

- [**Hotd3pc_ArcadeMod**](https://github.com/argonlefou/Hotd3pc_ArcadeMod) by argonlefou — Extensive prior RE work on the HOTD3 PC binary
- [**ReWrithe**](https://github.com/TawhidI-602/The-House-of-the-Dead-ReWrithe) by TawhidI-602 — Inspiration and reference for HOTD1
- [**hotd2-re**](https://github.com/GitFist/hotd2-re) by GitFist — Companion project for HOTD2

## License

This project is licensed under the **GNU General Public License v3.0** — see [LICENSE](LICENSE) for details.

The source code in this repository is entirely original work. No Sega copyrighted materials are included.
