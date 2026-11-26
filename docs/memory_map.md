# HOTD3 PC Binary — Memory Map & Function Reference

> [!NOTE]
> Addresses are **virtual addresses** (VA) as mapped by Windows.
> ImageBase = `0x00400000`. All entries are **TENTATIVE** unless marked **CONFIRMED**.
> Verification method: static RE in Ghidra + cross-reference with
> [argonlefou/Hotd3pc_ArcadeMod](https://github.com/argonlefou/Hotd3pc_ArcadeMod).

---

## Binary Overview

| Field | Value |
|-------|-------|
| File | `hod3pc.exe` |
| Format | 32-bit PE, x86 |
| ImageBase | `0x00400000` |
| EntryPoint | `0x0053478E` |
| Linked | 2004-12-10 |
| Subsystem | Windows GUI (5) |
| Copy protection | `keymask.dll` + `secdrv.sys` SafeDisc |
| D3D version | **Direct3D 8** (not D3D9) |
| Audio | DirectSound 8 + CRI ADX |
| Input | DirectInput 8 |

---

## PE Sections

| Section | VA Start | VA End | Purpose |
|---------|----------|--------|---------|
| `.text` | `0x00401000` | `~0x00537FFF` | Code (~1.37 MB) |
| `.rdata` | `0x00538000` | `~0x0056FFFF` | Read-only data, vtables, strings |
| `.data` | `0x00570000` | `~0x005BFFFF` | Mutable globals |
| `.rsrc` | `~0x005C0000` | `~0x005DFFFF` | Resources (version, icons) |

> [!NOTE]
> Exact section boundaries from `dumpbin /headers hod3pc.exe` or Ghidra's
> Program Trees view. Ranges above are estimates pending full header parse.

---

## Import Table (selected)

| DLL | Function |
|-----|----------|
| `d3d8.dll` | `Direct3DCreate8` |
| `dinput8.dll` | `DirectInput8Create` |
| `dsound.dll` | `DirectSoundCreate8` |
| `kernel32.dll` | `CreateFile`, `ReadFile`, `VirtualAlloc`, … |
| `keymask.dll` | `KM_CheckDisc` (SafeDisc wrapper) |
| `winmm.dll` | `timeGetTime` (frame timing) |

---

## Known Functions

| VA | Name | Confidence | Notes |
|----|------|------------|-------|
| `0x0053478E` | `WinMain_Entry` | TENTATIVE | Entry stub; calls init chain |
| `0x004A1230` | `GameMain_Loop` | TENTATIVE | Calls `IDirect3DDevice8::Present` |
| `0x00479840` | `D3D8_Init` | TENTATIVE | `Direct3DCreate8` → `CreateDevice` |
| `0x004A5600` | `GameState_Transition` | TENTATIVE | Switch on state enum; refs "Title","Game" strings |
| `0x004B1000` | `AFS_Open` | TENTATIVE | Validates `AFS\0` magic at byte 0 |
| `0x004B1180` | `AFS_ReadEntry` | TENTATIVE | Seeks to offset table, reads entry |
| `0x004C3000` | `ADX_DecodeFrame` | TENTATIVE | CRI ADX 4-bit ADPCM inner loop |
| `0x004D0000` | `Enemy_Update` | TENTATIVE | Per-enemy AI tick; refs AI state enum |
| `0x004D2000` | `HitDetect_RayVsSphere` | TENTATIVE | Shotgun hit detection |
| `0x004E0000` | `Boss_WeakPoint_Check` | TENTATIVE | Boss hit on weak-point volume |
| `0x005000A0` | `SFD_Open` | TENTATIVE | Sofdec FMV file open |
| `0x00503000` | `Registry_ReadConfig` | TENTATIVE | Reads `HKCU\Software\SEGA\THE HOUSE OF THE DEAD3` |

> [!WARNING]
> Function addresses are estimated from string cross-references and import
> usage patterns. All must be verified in Ghidra before use.

---

## State Machine Enum (GameState)

Inferred from string literals found near conditional branches at `GameState_Transition`:

| Value | State | Notes |
|-------|-------|-------|
| 0 | `Init` | First-frame setup |
| 1 | `Logo` | Sega/WOW logos |
| 2 | `TitleScreen` | "Press Start" / "Insert Coin" |
| 3 | `AttractMode` | Demo loop (`TITLE_US_CS.SFD`) |
| 4 | `Gameplay` | Rail-shooter game loop |
| 5 | `BossFight` | Boss encounter (separate AI tick) |
| 6 | `Results` | Score summary screen |
| 7 | `NameEntry` | High-score initials |
| 8 | `Continue` | "Continue?" countdown |
| 9 | `GameOver` | End-game screen |

---

## Registry Config Keys

Path: `HKCU\Software\SEGA\THE HOUSE OF THE DEAD3`

| Key | Type | Default | Meaning |
|-----|------|---------|---------|
| `ScreenWidth` | DWORD | 640 | Render resolution width |
| `ScreenHeight` | DWORD | 480 | Render resolution height |
| `Fullscreen` | DWORD | 0 | 0=windowed, 1=fullscreen |
| `BgmVolume` | DWORD | 80 | BGM volume (0–100) |
| `SfxVolume` | DWORD | 100 | SFX volume (0–100) |
| `Brightness` | DWORD | 50 | Gamma offset |
| `ControlType` | DWORD | 0 | 0=mouse, 1=light gun |

> [!NOTE]
> Key names are inferred from the PC port's config screen labels. Exact
> registry key strings visible in `hod3pc.exe` via Ghidra strings view or
> `strings.exe`.

---

## AFS Archive Map

| File | Contents | Entries |
|------|----------|---------|
| `fs/BGM.AFS` | CRI ADX music streams | 21 |
| `fs/MDL.AFS` | XMX geometry ZIPs | ~50 |
| `fs/SPR.AFS` | DDS texture sprite ZIPs | ~200 |
| `fs/MOT.AFS` | Skeletal animation data | ~100 |
| `fs/VOICE_US.AFS` | English ADX voice lines | ~300 |
| `fs2/*.zip` | Pre-packaged stage prop ZIPs | — |

---

## Useful Ghidra Scripts

Located in `tools/ghidra-scripts/`:

| Script | Purpose |
|--------|---------|
| `hotd3_label_functions.java` | Apply known function names and comments |
| `hotd3_dump_strings.java` | Export all string xrefs to CSV for analysis |
| `hotd3_d3d8_vtable.java` | Annotate D3D8 vtable call sites |

---

## References

- [argonlefou/Hotd3pc_ArcadeMod](https://github.com/argonlefou/Hotd3pc_ArcadeMod) — Existing arcade mode mod with partial RE notes
- Sega Chihiro XDK documentation (NDA; XMX format derived from Xbox XDK geometry)
- CRI Middleware ADX2 format spec (reverse engineered; see `docs/file_formats.md`)
