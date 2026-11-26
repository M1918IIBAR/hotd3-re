// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ghidra Script: Identify and label D3D8 vtable calls.
//
// Usage:
//   Script Manager → Run → hotd3_d3d8_vtable.java
//
// HOTD3 PC uses Direct3D 8 (d3d8.dll), not D3D9 or later.
// This script searches for indirect calls that match known D3D8 vtable
// offsets and annotates them with the method name.
//
// IDirect3DDevice8 vtable layout (selected methods):
//   Offset  Method
//   0x00    QueryInterface
//   0x08    Release
//   0x14    TestCooperativeLevel
//   0x24    GetAvailableTextureMem
//   0x28    ResourceManagerDiscardBytes
//   0x40    CreateTexture
//   0x48    CreateVertexBuffer
//   0x50    CreateIndexBuffer
//   0x60    CopyRects
//   0x64    UpdateTexture
//   0x6C    GetFrontBuffer
//   0x7C    SetRenderTarget
//   0x88    Clear
//   0x98    SetTransform
//   0xA0    MultiplyTransform
//   0xA8    SetViewport
//   0xB8    SetMaterial
//   0xC8    SetLight
//   0xCC    LightEnable
//   0xD0    GetLight
//   0xE0    SetClipPlane
//   0xF0    SetRenderState
//   0x100   GetRenderState
//   0x11C   SetTexture
//   0x120   GetTexture
//   0x124   GetTextureStageState
//   0x128   SetTextureStageState
//   0x12C   ValidateDevice
//   0x138   SetPaletteEntries
//   0x13C   GetPaletteEntries
//   0x140   SetCurrentTexturePalette
//   0x148   DrawPrimitive
//   0x14C   DrawIndexedPrimitive
//   0x150   DrawPrimitiveUP
//   0x154   DrawIndexedPrimitiveUP
//   0x158   ProcessVertices
//   0x168   SetStreamSource
//   0x16C   GetStreamSource
//   0x170   SetIndices
//   0x178   BeginScene
//   0x17C   EndScene
//   0x180   DrawRectPatch
//   0x184   DrawTriPatch
//   0x188   DeletePatch
//   0x190   Present
//
// NOTE: These offsets may shift by ±4 bytes if the binary was built
//       against a slightly different D3D8 SDK version. Cross-check with
//       import table or d3d8.dll symbols to confirm.

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

import java.util.HashMap;
import java.util.Map;

public class hotd3_d3d8_vtable extends GhidraScript {

    // vtable offset → method name
    private static final Map<Integer, String> VTABLE = new HashMap<>();
    static {
        VTABLE.put(0x08,  "IDirect3DDevice8::Release");
        VTABLE.put(0x40,  "IDirect3DDevice8::CreateTexture");
        VTABLE.put(0x48,  "IDirect3DDevice8::CreateVertexBuffer");
        VTABLE.put(0x50,  "IDirect3DDevice8::CreateIndexBuffer");
        VTABLE.put(0x88,  "IDirect3DDevice8::Clear");
        VTABLE.put(0xF0,  "IDirect3DDevice8::SetRenderState");
        VTABLE.put(0x11C, "IDirect3DDevice8::SetTexture");
        VTABLE.put(0x128, "IDirect3DDevice8::SetTextureStageState");
        VTABLE.put(0x148, "IDirect3DDevice8::DrawPrimitive");
        VTABLE.put(0x14C, "IDirect3DDevice8::DrawIndexedPrimitive");
        VTABLE.put(0x150, "IDirect3DDevice8::DrawPrimitiveUP");
        VTABLE.put(0x168, "IDirect3DDevice8::SetStreamSource");
        VTABLE.put(0x170, "IDirect3DDevice8::SetIndices");
        VTABLE.put(0x178, "IDirect3DDevice8::BeginScene");
        VTABLE.put(0x17C, "IDirect3DDevice8::EndScene");
        VTABLE.put(0x190, "IDirect3DDevice8::Present");
    }

    @Override
    public void run() throws Exception {
        println("=== HOTD3-RE: Annotating D3D8 vtable calls ===");

        int annotated = 0;
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext() && !monitor.isCancelled()) {
            Instruction instr = it.next();

            // Look for: call dword ptr [eax + offset]  (or ecx, edx, etc.)
            // Mnemonic: CALL, operand contains a register-relative memory ref
            if (!instr.getMnemonicString().equalsIgnoreCase("CALL")) continue;

            String repr = instr.toString();
            // Simple heuristic: match patterns like "[eax+0x190]"
            for (Map.Entry<Integer, String> entry : VTABLE.entrySet()) {
                String hexOffset = String.format("0x%x", entry.getKey());
                if (repr.contains("+" + hexOffset + "]") || repr.contains("+0x" + Integer.toHexString(entry.getKey()) + "]")) {
                    setEOLComment(instr.getAddress(), "→ " + entry.getValue());
                    annotated++;
                    break;
                }
            }
        }

        println(String.format("Done. Annotated %d D3D8 vtable calls.", annotated));
    }
}
