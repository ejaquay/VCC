// Windows messages sent to VCC main message loop
// Be sure to add these to proxy_msgwin for DLL's

#pragma once
#include <windows.h>
#include <cstdint>

// Structure for transmitting plugin data across DLL boundaries
struct PluginMsgData {
	uint32_t size;              // Size of data
	char pluginPath[MAX_PATH];  // Plugin filename
};

// Hard Reset
inline constexpr uint32_t WM_VCC_CPU_RESET = WM_APP + 101;

inline LRESULT SendHardReset(HWND hwnd) {
	return SendMessage(hwnd,WM_VCC_CPU_RESET, 0, 0);
}

// Update menu request
inline constexpr uint32_t WM_VCC_UPD_MENU = WM_APP + 102;

inline LRESULT SendMenuUpdate(HWND hwnd) {
	return SendMessage(hwnd,WM_VCC_UPD_MENU, 0, 0);
}

// Soft RESET
inline constexpr uint32_t WM_VCC_SOFT_RESET = WM_APP + 103;

inline LRESULT SendSoftReset(HWND hwnd) {
	return SendMessage(hwnd,WM_VCC_SOFT_RESET, 0, 0);
}

// Set startup slot
inline constexpr uint32_t WM_VCC_SET_START_SLOT = WM_APP + 104;

inline LRESULT SendStartSlot(HWND hwnd, uint32_t slotnum) {
	return SendMessage(hwnd,WM_VCC_SOFT_RESET, slotnum, 0);
}

// Unload slot
inline constexpr uint32_t WM_VCC_UNLOAD_SLOT = WM_APP + 105;

inline LRESULT SendUnloadSlot(HWND hwnd, uint32_t slotnum) {
	return SendMessage(hwnd,WM_VCC_UNLOAD_SLOT, slotnum, 0);
}

// Load cartridge plugin
inline constexpr uint32_t WM_VCC_LOAD_SLOT =  WM_APP + 106;

inline LRESULT SendLoadSlot(HWND hwnd, uint32_t slotnum, const PluginMsgData& pluginData) {
    return SendMessage(
        hwnd,
        WM_VCC_LOAD_SLOT,
        static_cast<WPARAM>(slotnum),
        reinterpret_cast<LPARAM>(&pluginData)
    );
}

