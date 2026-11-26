// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ghidra Script: Label HOTD3 known functions from static analysis.
//
// Usage:
//   1. Open hod3pc.exe in Ghidra (File → Import File, pick PE/COFF)
//   2. Auto-analyse (Analysis → Auto Analyze)
//   3. Script Manager → Run → hotd3_label_functions.java
//
// The script applies names and comments to known addresses derived from
// cross-referencing import names, string references, and community research
// (argonlefou/Hotd3pc_ArcadeMod).
//
// ImageBase: 0x00400000  (PE header confirms; Ghidra default matches)
// EntryPoint: 0x0053478E

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

public class hotd3_label_functions extends GhidraScript {

    // ── Known function addresses (VA at default ImageBase 0x00400000) ────────
    // These are best-effort estimates from string cross-refs and import usage.
    // Marked TENTATIVE until confirmed with dynamic analysis or deeper RE.

    private static final long DEFAULT_IMAGE_BASE = 0x00400000L;

    private static final long[][] KNOWN_FUNCTIONS = {
        // { VA,           flags }  flags: 0=confirmed, 1=tentative
        { 0x0053478EL, 1 },   // WinMain / entry stub
        { 0x004A1230L, 1 },   // GameMain_Loop (ticked every frame; references D3D Present)
        { 0x00479840L, 1 },   // D3D8_Init (calls Direct3DCreate8, CreateDevice)
        { 0x004A5600L, 1 },   // GameState_Transition (switch on state enum, refs "Title","Game")
        { 0x004B1000L, 1 },   // AFS_Open (magic "AFS\0" check at +0x0)
        { 0x004B1180L, 1 },   // AFS_ReadEntry (seeks to entry table, reads size/offset)
        { 0x004C3000L, 1 },   // ADX_DecodeFrame (CRI ADPCM nibble decoder inner loop)
        { 0x004D0000L, 1 },   // Enemy_Update (called per-enemy each tick; refs AI state enum)
        { 0x004D2000L, 1 },   // HitDetect_RayVsSphere (D3DXIntersectTri equivalent)
        { 0x004E0000L, 1 },   // Boss_WeakPoint_Check
        { 0x005000A0L, 1 },   // SFD_Open (Sofdec, refs "SFD\0" or CRI header)
        { 0x00503000L, 1 },   // Registry_ReadConfig (RegOpenKeyExA HKCU\\Software\\SEGA\\...)
    };

    private static final String[][] FUNCTION_NAMES = {
        { "WinMain_Entry",          "Entry point. Calls D3D8_Init, then GameMain_Loop." },
        { "GameMain_Loop",          "TENTATIVE: Main game loop. Calls Update+Render each frame, calls IDirect3DDevice8::Present." },
        { "D3D8_Init",              "TENTATIVE: Initialises Direct3D 8. Direct3DCreate8 → CreateDevice." },
        { "GameState_Transition",   "TENTATIVE: State machine dispatcher. Switch on current state enum." },
        { "AFS_Open",               "TENTATIVE: Opens a Sega AFS archive. Validates 'AFS\\0' magic." },
        { "AFS_ReadEntry",          "TENTATIVE: Reads one AFS entry by index (seeks to offset table)." },
        { "ADX_DecodeFrame",        "TENTATIVE: Decodes one CRI ADX frame (32 samples, 4-bit ADPCM)." },
        { "Enemy_Update",           "TENTATIVE: Per-enemy AI tick. References AI state enum (patrol/attack/dead)." },
        { "HitDetect_RayVsSphere",  "TENTATIVE: Ray-sphere intersection. Used for shotgun hit registration." },
        { "Boss_WeakPoint_Check",   "TENTATIVE: Checks if a hit ray intersects a boss weak-point volume." },
        { "SFD_Open",               "TENTATIVE: Opens a Sofdec SFD FMV file." },
        { "Registry_ReadConfig",    "TENTATIVE: Reads config from HKCU\\\\Software\\\\SEGA\\\\THE HOUSE OF THE DEAD3." },
    };

    @Override
    public void run() throws Exception {
        println("=== HOTD3-RE: Labelling known functions ===");

        long imageBase = currentProgram.getImageBase().getOffset();
        println(String.format("Image base: 0x%08X", imageBase));

        int labelled = 0;
        for (int i = 0; i < KNOWN_FUNCTIONS.length; i++) {
            long va   = KNOWN_FUNCTIONS[i][0];
            long addr = imageBase + (va - DEFAULT_IMAGE_BASE);
            String name    = FUNCTION_NAMES[i][0];
            String comment = FUNCTION_NAMES[i][1];

            Address funcAddr = toAddr(addr);
            Function func = getFunctionAt(funcAddr);
            if (func == null) {
                // Try to create a function here
                func = createFunction(funcAddr, name);
                if (func == null) {
                    println("  SKIP (could not create function): " + name + " @ 0x" + Long.toHexString(addr));
                    continue;
                }
            } else {
                func.setName(name, SourceType.USER_DEFINED);
            }

            setEOLComment(funcAddr, comment);
            println("  LABELLED: " + name + " @ 0x" + Long.toHexString(addr));
            labelled++;
        }

        println(String.format("Done. Labelled %d / %d functions.", labelled, KNOWN_FUNCTIONS.length));
        println("NOTE: Addresses marked TENTATIVE require verification via dynamic analysis.");
    }
}
