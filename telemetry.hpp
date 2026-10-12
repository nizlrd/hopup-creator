#pragma once
#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include "guide.hpp"

// Read-only local-player/camera adapter adapted from ../19-fivem_minimap.
// Every supported slot is instruction-validated before use. No scanning fallback.
namespace telemetry {
constexpr uintptr_t world_slot_rva=0x25B14B0, world_reference_rva=0x1858B;
uintptr_t world_slot=0, viewport_slot=0, process_image_base=0;
HANDLE read_process=GetCurrentProcess();
bool access_allowed=false;
inline void set_access(bool allowed) {
    access_allowed=allowed;
    if(!allowed)world_slot=viewport_slot=process_image_base=0;
}
    bool read_bytes(uintptr_t address, void *value, size_t size)
    {
        if (!access_allowed || !size || address < 0x10000 || address > 0x00007FFFFFFFFFFFULL - size) return false;
        SIZE_T copied = 0;
        return ReadProcessMemory(read_process, reinterpret_cast<const void *>(address),
            value, size, &copied) && copied == size;
    }
    template <typename T> bool read(uintptr_t address, T &value)
    {
        return read_bytes(address, &value, sizeof(T));
    }
    bool resolve_player_slot()
    {
        wchar_t path[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
        if (!length || length >= MAX_PATH) return false;
        const wchar_t *name = std::wcsrchr(path, L'\\');
        name = name ? name + 1 : path;
        if (_wcsicmp(name, L"FiveM_b3258_GTAProcess.exe") != 0) return false;
        const uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        unsigned char instruction[24] = {};
        if (!read(base + world_reference_rva, instruction)) return false;
        const unsigned char prefix[] = { 0x48, 0x8B, 0x05 };
        const unsigned char suffix[] = { 0x48, 0x8B, 0x48, 0x08, 0x48, 0x85, 0xC9, 0x74,
            0x30, 0xF3, 0x0F, 0x10, 0x81, 0x18, 0x15, 0x00, 0x00 };
        if (std::memcmp(instruction, prefix, sizeof(prefix)) ||
            std::memcmp(instruction + 7, suffix, sizeof(suffix))) return false;
        int32_t displacement = 0;
        std::memcpy(&displacement, instruction + 3, sizeof(displacement));
        const uintptr_t resolved = base + world_reference_rva + 7 + displacement;
        if (resolved != base + world_slot_rva) return false;
        world_slot = resolved;
        process_image_base = base;
        return true;
    }
    bool resolve_viewport()
    {
        if (!world_slot) return false;
        constexpr uintptr_t reference_rva = 0xD3CE74, slot_rva = 0x201DBA0;
        const uintptr_t base = process_image_base;
        unsigned char instruction[23] = {};
        const unsigned char pattern[] = {0x33,0xC0,0x48,0x39,0x05,0,0,0,0,0x74,0x2E,
            0x48,0x8B,0x0D,0,0,0,0,0x48,0x85,0xC9,0x74,0x22};
        if (!read(base + reference_rva, instruction)) return false;
        for (size_t i = 0; i < sizeof(pattern); ++i)
        {
            if ((i >= 5 && i <= 8) || (i >= 14 && i <= 17)) continue;
            if (instruction[i] != pattern[i]) return false;
        }
        int32_t displacement = 0;
        std::memcpy(&displacement, instruction + 5, sizeof(displacement));
        const uintptr_t resolved = base + reference_rva + 9 + displacement;
        if (resolved != base + slot_rva) return false;
        viewport_slot = resolved;
        return true;
    }
struct snapshot { hopout::vec3 player; float matrix[16]={}, inverse[16]={}; };
inline bool sample(snapshot &out) {
    out={};
    if(!world_slot || !viewport_slot) return false;
    uintptr_t world=0,ped=0,viewport=0,world_after=0,ped_after=0,viewport_after=0;
    snapshot next;
    if(!read(world_slot,world)||!world||!read(world+8,ped)||!ped||
       !read(ped+0x90,next.player)||!hopout::valid(next.player)||
       !read(viewport_slot,viewport)||!viewport||!read(viewport+0x90,next.matrix)||
       !hopout::inverse(next.matrix,next.inverse)||!read(world_slot,world_after)||world_after!=world||
       !read(world+8,ped_after)||ped_after!=ped||!read(viewport_slot,viewport_after)||viewport_after!=viewport)return false;
    out=next; return true;
}
inline bool initialize() {
    world_slot=viewport_slot=process_image_base=0;
    return resolve_player_slot() && resolve_viewport();
}
}
