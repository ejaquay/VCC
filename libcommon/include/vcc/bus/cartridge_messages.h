////////////////////////////////////////////////////////////////////////////////
//	Copyright 2015 by Joseph Forgione
//	This file is part of VCC (Virtual Color Computer).
//	
//	VCC (Virtual Color Computer) is free software: you can redistribute itand/or
//	modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your
//	option) any later version.
//	
//	VCC (Virtual Color Computer) is distributed in the hope that it will be
//	useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
//	Public License for more details.
//	
//	You should have received a copy of the GNU General Public License along with
//	VCC (Virtual Color Computer). If not, see <http://www.gnu.org/licenses/>.
////////////////////////////////////////////////////////////////////////////////


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

// Hard Reset (any plugin)
inline constexpr uint32_t WM_VCC_CPU_RESET = WM_APP + 101;
inline LRESULT SendHardReset(HWND hwnd) {
	return SendMessage(hwnd,WM_VCC_CPU_RESET, 0, 0);
}

// Update menu request (any plugin)
inline constexpr uint32_t WM_VCC_UPD_MENU = WM_APP + 102;
inline LRESULT SendMenuUpdate(HWND hwnd) {
	return SendMessage(hwnd,WM_VCC_UPD_MENU, 0, 0);
}

// Soft RESET (any plugin)
inline constexpr uint32_t WM_VCC_SOFT_RESET = WM_APP + 103;
inline LRESULT SendSoftReset(HWND hwnd) {
	return SendMessage(hwnd,WM_VCC_SOFT_RESET, 0, 0);
}

// Set active slot (from MMI)
inline constexpr uint32_t WM_VCC_SET_ACTIVE_SLOT = WM_APP + 104;
inline LRESULT SendActiveSlot(HWND hwnd, uint32_t slotnum) {
	return SendMessage(hwnd,WM_VCC_SET_ACTIVE_SLOT, slotnum, 0);
}

// Unload slot (from MMI)
inline constexpr uint32_t WM_VCC_UNLOAD_SLOT = WM_APP + 105;
inline LRESULT SendUnloadSlot(HWND hwnd, uint32_t slotnum) {
	return SendMessage(hwnd,WM_VCC_UNLOAD_SLOT, slotnum, 0);
}

// Load cartridge plugin (from MMI)
inline constexpr uint32_t WM_VCC_LOAD_SLOT =  WM_APP + 106;
inline LRESULT SendLoadSlot(HWND hwnd, uint32_t slotnum, const PluginMsgData& pluginData) {
	return SendMessage(
	hwnd,
	WM_VCC_LOAD_SLOT,
	static_cast<WPARAM>(slotnum),
	reinterpret_cast<LPARAM>(&pluginData)
	);
}

