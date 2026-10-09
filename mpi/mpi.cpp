////////////////////////////////////////////////////////////////////////////////
//	Copyright 2015 by Joseph Forgione
//	This file is part of VCC (Virtual Color Computer).
//	
//	This is an expansion module for the Vcc Emulator. It simulated the functions
//	of the TRS-80 Multi-Pak Interface.
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

#include "mpi.h"
#include "resource.h"
#include <vcc/util/DialogOps.h>
#include <vcc/util/limits.h>
#include <vcc/util/logger.h>
#include <vcc/bus/cartridge_menuitem.h>
#include <vcc/bus/cartridge_messages.h>

HINSTANCE gModuleInstance = nullptr;
static std::string gConfigurationFilename;
HWND gVccWnd;

slot_id_type SlotId = 0;

multipak_configuration gMultiPakConfiguration("MPI");
multipak_supervisor gMultiPakInterface(gMultiPakConfiguration);

// the config dialog
configuration_dialog gConfigurationDialog(gMultiPakConfiguration, gMultiPakInterface);

// DLL exports
extern "C"
{
	__declspec(dllexport) const char* PakGetName()
	{
		static char string_buffer[MAX_LOADSTRING];
		LoadString(gModuleInstance, IDS_MODULE_NAME, string_buffer, MAX_LOADSTRING);
		return string_buffer;
	}

	__declspec(dllexport) const char* PakGetCatalogId()
	{
		static char string_buffer[MAX_LOADSTRING];
		LoadString(gModuleInstance, IDS_CATNUMBER, string_buffer, MAX_LOADSTRING);
		return string_buffer;
	}

	//Initialize MPI -	capture callback addresses and build menus.
	__declspec(dllexport) void PakInitialize(
		slot_id_type SlotId,                   // should always be zero
		const char* const configuration_path,
		HWND hVccWnd,
		const cpak_callbacks* const callbacks)
	{
		//Prevent MPI load in a multipak slot
		//TODO: enum for VCC error codes
		if (SlotId != 0) {
			int32_t err = 1;
			SendFatalError(hVccWnd, err);
			return;
		}

		gMultiPakConfiguration.configuration_path(configuration_path);
		gConfigurationFilename = configuration_path;
		gVccWnd = hVccWnd;
		gMultiPakInterface.start();  //TODO Not needed
	}

	__declspec(dllexport) void PakTerminate()
	{
		gConfigurationDialog.close();
		//TODO remove pakinterface, move all other needed clean up to here
		gMultiPakInterface.stop();
	}

	__declspec(dllexport) void PakMenuItemClicked(unsigned char menu_item_id)
	{
		//TODO move item clicked to config
		gMultiPakInterface.menu_item_clicked(menu_item_id);
	}

	// Fetch menu item list for MPI and carts it has loaded
	__declspec(dllexport) bool PakGetMenuItem(menu_item_entry* item, size_t index)
	{
		//TODO move get menu item to config
		return gMultiPakInterface.get_menu_item(item, index);
	}

	// Write to port
	//__declspec(dllexport) void PakWritePort(unsigned char port_id,unsigned char value)
	//{
	//	gMultiPakInterface.write_port(port_id, value);
	//	return;
	//}

	// Read from port
	//__declspec(dllexport) unsigned char PakReadPort(unsigned char port_id)
	//{
	//	return gMultiPakInterface.read_port(port_id);
	//}

	// Reset module
	__declspec(dllexport) unsigned char PakReset()
	{
		//TODO move to config
		gMultiPakInterface.reset();
		return 0;
	}

	//__declspec(dllexport)  void PakProcessHorizontalSync()
	//{
	//	gMultiPakInterface.process_horizontal_sync();
	//}

	//__declspec(dllexport)  unsigned char PakReadMemoryByte(unsigned short memory_address)
	//{
	//	return gMultiPakInterface.read_memory_byte(memory_address);
	//}

	// Return MPI status.
	//__declspec(dllexport) void PakGetStatus(char* text_buffer, size_t buffer_size)
	//{
//		gMultiPakInterface.status(text_buffer, buffer_size);
//	}

	//__declspec(dllexport) unsigned short PakSampleAudio()
	//{
	//	return gMultiPakInterface.sample_audio();
	//}
}

// DLLMain
BOOL WINAPI DllMain(HINSTANCE module_instance, DWORD reason, LPVOID /*reserved*/)
{
	switch (reason) {
	case DLL_PROCESS_ATTACH:
		gModuleInstance = module_instance;
		break;
	case DLL_PROCESS_DETACH:
		PakTerminate();
		break;
	}
	return TRUE;
}

