# The House of the Dead III — File Formats & Asset Architecture

This document describes the data file structure, container archives, and asset formats reverse-engineered from *The House of the Dead III* PC port (`hod3pc.exe`).

---

## 1. Directory Structure

A complete installation of the PC version contains the following directories and files:

```
game_data/
├── hod3pc.exe           # Main 32-bit x86 game executable (D3D8 / DInput8)
├── hod3launch.exe       # Original configuration launcher
├── KeyMask.dll          # Security / key check helper
├── fs/                  # Primary Sega AFS container archives
│   ├── BGM.AFS          # Background music streams (CRI ADX format, 48 kHz)
│   ├── MDL.AFS          # 3D character and enemy models (ZIP containing XMDL/XBOX .xmx)
│   ├── MOT.AFS          # Skeletal animations and motion tables
│   ├── SPR.AFS          # 2D Sprites, HUD, crosshairs (ZIP containing .dds textures)
│   ├── SPRTBL.AFS       # Sprite coordinate mapping and layout tables
│   ├── VOICE_US.AFS     # English voice lines (CRI ADX format)
│   ├── SE_US.AFS        # Sound effects (CRI ADX format)
│   ├── X3D.AFS          # 3D stage geometry and environment data
│   ├── COLLI.AFS        # Collision geometry and raycast bounding volumes
│   ├── DYN.AFS          # Dynamic interactive objects (barrels, crates, doors)
│   ├── FCV.AFS          # Camera curves and path vectors (on-rails cinematic camera)
│   ├── JIMAKU.AFS       # Subtitle text tables (Japanese: 字幕 / jimaku)
│   ├── DEMOKEY.AFS      # Recorded demo input sequences for attract mode
│   ├── EVTBL.BIN        # Event triggers and script dispatch tables
│   ├── MSCL.AFS         # Miscellaneous game assets
│   ├── SHA.AFS          # Shadow volumes and projection data
│   ├── SHPDBK.AFS       # Background shape data
│   └── TMP_COMMON.AFS   # Common temporary shared assets
├── fs2/                 # Stage-specific and prop model archives
│   └── pol_*.zip        # Pre-packaged ZIP archives holding .xmx meshes & .dds textures
├── media/               # Additional uncompressed audio assets
│   └── *.aif            # AIFF audio clips (footsteps, impacts, Foley sounds)
└── mvi/                 # Full-Motion Videos (FMV)
    ├── TITLE_US_CS.SFD  # Title sequence / attract movie
    └── CHAPTER[0-5]_US.SFD # Chapter intro & ending cutscenes (Sega Sofdec MPEG+ADX)
```

---

## 2. Container Formats

### 2.1 Sega AFS Archive (`.AFS`)

AFS (Audio File System / Archive File System) is Sega's standard container format used across Dreamcast, Chihiro, Naomi, and Xbox titles.

* **Header (Little-Endian)**:
  * `0x00 - 0x03` (`4 bytes`): Magic ASCII `"AFS\0"` (`0x41, 0x46, 0x53, 0x00`)
  * `0x04 - 0x07` (`4 bytes`): `u32 file_count`
  * `0x08 + i * 8` (`8 bytes` per entry):
    * `u32 offset`: Byte offset of file payload from start of archive
    * `u32 length`: Byte size of file payload
* **Filename Directory**:
  * Located at the offset stored immediately following the entry table (`8 + file_count * 8`).
  * Each directory record contains:
    * `32 bytes`: Null-terminated ASCII filename
    * `12 bytes`: Timestamp (year, month, day, hour, minute, second as `u16`)
    * `4 bytes`: File length (`u32`)

### 2.2 Inner AFS Payloads (Embedded ZIPs)

In *The House of the Dead III*, several AFS containers (such as `SPR.AFS`, `MDL.AFS`) contain individual files that are standard **PKZIP archives** (`PK\x03\x04` header).
* Extracting an entry from `SPR.AFS` yields one or more **DirectDraw Surface (`.dds`)** textures.
* Extracting an entry from `MDL.AFS` yields one or more **Xbox Model (`.xmx`)** meshes and associated textures.

---

## 3. Asset Formats

### 3.1 Textures (`.dds`)
* Standard Microsoft DirectDraw Surface (`DDS ` magic `0x44 0x44 0x53 0x20`).
* Primary formats:
  * **DXT1 / BC1** (opaque or 1-bit alpha textures)
  * **DXT3 / BC2** (sharp alpha textures)
  * **DXT5 / BC3** (smooth gradient alpha textures)
  * **A8R8G8B8** (uncompressed 32-bit RGBA)
* Easily ingested natively by DirectX 11 (`DirectXTex` / D3D11 resource creation) and Vulkan.

### 3.2 3D Models & Stages (`.xmx`, `.x3x`)
* **Character/Enemy Models (`.xmx`)**:
  * Header Magic: `4C-44-4D-58-58-4F-42-58` (`XMDL` + `XBOX` in little-endian format).
  * Sub-header at `0x10`: `LEDM` (`4C-45-44-4D`). All `VPRG` data offsets are relative to `0x10`.
  * Chunk tags (reversed 4CC): `EMAN` (`NAME`), `RTAM` (`MATR`), `MNXT` (`TXNM`), `VPRG` (`GRPV`), `XTRV` (`VRTX`), `XDNI` (`INDX`).
  * `VPRG` layout: `+0x0C` = `u32 vertex_count`, `+0x14` = `u32 xtrv_rel`, `+0x30` = `u32 d3d8_fvf`, `+0x34` = `u32 vertex_stride`, `+0x44` = `u32 xdni_rel`, `+0x48` = `u32 index_count`.
  * `XDNI` stores 16-bit triangle-strip indices (`D3DPT_TRIANGLESTRIP`).
  * Originates from Sega Chihiro arcade hardware (Xbox XDK architecture).
* **Stage Environment Geometry (`.x3x` in `X3D.AFS`)**:
  * Header Magic: `4F-44-33-58-58-4F-42-58` (`X3DO` + `XBOX` in little-endian format).
  * Sub-header: `OBJ3` (`33-4A-42-4F`).
  * Contains world-space static level meshes, materials, and sector bounds.
* **Stage Animations (`.xax` in `X3D.AFS`)**:
  * Environmental object animation tracks and keyframes.

### 3.3 Audio Streams & Sound Effects
* **BGM & Voice Lines (`.adx` in `BGM.AFS`, `VOICE_US.AFS`)**:
  * CRI Middleware ADX adaptive differential PCM compression.
  * Signature: `0x80 0x00` (big-endian).
  * Sample Rate: Typically 48,000 Hz (`0x0000BB80`).
  * Channels: Stereo (2) for BGM, Mono (1) for voice lines.
* **Sound Effects (`media/*.aif`)**:
  * *Reverse-Engineering Discovery*: Despite the `.aif` extension, all 576 audio files in `media/` are standard Microsoft **RIFF WAVE PCM 16-bit** audio (typically 22,050 Hz Mono). Can be parsed directly with standard PCM WAVE loaders.
  * Codec: CRI ADX adaptive differential pulse-code modulation.

### 3.4 Full-Motion Video (`.sfd`)
* Sega Sofdec (SFD) container.
* Video stream: MPEG-1 or MPEG-2 elementary video.
* Audio stream: Multiplexed CRI ADX audio.

---

## 4. Reverse-Engineered Extraction Quirks

During extraction of all 21 Sega AFS containers in `assets/game_data/fs`:
1. **Filename Paths**: Certain archives (e.g. `BGM.AFS`, `VOICE_US.AFS`) contain relative source paths (e.g., `../ok/fs/adx/bgm001.adx`). When unpacking, paths must be sanitized using `Path.GetFileName` to prevent directory traversal outside the target folder.
2. **32-Byte Truncation**: Filename records are fixed at 32 bytes without terminating null padding for long names. For instance, `../ok/fs/voice_us/voice_c_028.ad` truncates the trailing `x` in `.adx`.
3. **Implicit Name Containers**: Certain archives (such as `MDL.AFS`) have `dirOffset == 0`, meaning no string table is present. Entries are numbered sequentially (`entry_0`, `entry_1`, ...) and identified by payload magic (`PK\x03\x04` ZIP containers containing `.xmx` and `.dds`).
4. **Layout Tables (`SPRTBL.AFS`)**: Houses localized sprite table definitions (`sprtbl_u.bin` for US, `sprtbl_j.bin` for Japan, `sprtbl_f.bin` for France, `sprtbl_g.bin` for Germany, `sprtbl_i.bin` for Italy, `sprtbl_s.bin` for Spain).
5. **Subtitle Dictionaries (`JIMAKU.AFS`)**: Contains chapter and dialog subtitles per language code (`chap_en.bin`, `d1_en.bin`, etc.).
