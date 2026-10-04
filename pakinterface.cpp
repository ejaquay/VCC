//#define USE_LOGGING
//======================================================================
// This file is part of VCC (Virtual Color Computer).
// Vcc is Copyright 2015 by Joseph Forgione
//
// VCC (Virtual Color Computer) is free software, you can redistribute
// and/or modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.
//
// VCC (Virtual Color Computer) is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VCC (Virtual Color Computer).  If not, see
// <http://www.gnu.org/licenses/>.
//======================================================================

#include "defines.h"
#include "tcc1014mmu.h"
#include "tcc1014registers.h"
#include "pakinterface.h"
#include "config.h"
#include "Vcc.h"
#include "mc6821.h"
#include "resource.h"
#include "pakrouter.h"
#include <vcc/bus/null_cartridge.h>
#include <vcc/bus/cartridge_menu.h>
#include <vcc/bus/cartridge_messages.h>
#include <vcc/bus/dll_deleter.h>
#include <vcc/util/limits.h>
#include <vcc/util/logger.h>
#include <vcc/util/FileOps.h>
#include <vcc/util/DialogOps.h>
#include <fstream>
#include <Windows.h>
#include <commdlg.h>

using cartridge_loader_status = VCC::Core::cartridge_loader_status;
using cartridge_loader_result = VCC::Core::cartridge_loader_result;

// Storage for Pak ROMs
extern SystemState EmuState;

static VCC::Util::critical_section gPakMutex;
static char DllPath[MAX_PATH] = "";

void CartMenuCallBack(const char* name, int menu_id, MenuItemType type);
void PakAssertInterupt(Interrupt interrupt, InterruptSource source);

// Arrays for holding cartridge slots. Slot 0 is boot slot.
#define NumCartSlots 5
using plugin_ptr = cartridge_loader_result::cartridge_ptr_type;
static std::array<plugin_ptr, NumCartSlots> gCartSlots{
	std::make_unique<VCC::Core::null_cartridge>(),
	std::make_unique<VCC::Core::null_cartridge>(),
	std::make_unique<VCC::Core::null_cartridge>(),
	std::make_unique<VCC::Core::null_cartridge>(),
	std::make_unique<VCC::Core::null_cartridge>()
};

// Multi cart unloader and loader manages carts in slot 0-4
using plugin_handle = cartridge_loader_result::handle_type;
static std::array<plugin_handle, NumCartSlots> gCartHandles{};

// Router object routes plugin calls to the slots
static VCC::Core::PakRouter gPakRouter;

void UnloadCartridge(int slot);
static cartridge_loader_status load_any_cartridge(int slot, const char *filename);

//==========================================================================

//--------------------------------------------------------
// Slot callback implimenters
//--------------------------------------------------------

static void write_memory_byte_impl(size_t slot, unsigned char val, unsigned short adr) {
	gPakRouter.cart_write_memory(slot, val, adr);
}
static unsigned char read_memory_byte_impl(size_t slot,unsigned short adr) {
    return gPakRouter.cart_read_memory(slot, adr);
}
static void assert_cart_line_impl(size_t slot,bool state) {
    gPakRouter.cart_assert_line(slot, state);
}
static void assert_interrupt_impl(size_t slot, Interrupt intr, InterruptSource src) {
    gPakRouter.cart_assert_interrupt(slot, intr, src);
}
struct cpak_callbacks slot_callbacks = {
	assert_interrupt_impl,
	assert_cart_line_impl,
	write_memory_byte_impl,
	read_memory_byte_impl
};

//----------------------------------------------------------
// Slot adapters
//
//FIXME slot adapters only exist to satisfy cart definitions,
// they have no real purpose. Cart defintions and loader needs
// to be redefined so these can be eliminated.
//----------------------------------------------------------

struct multi_slot_adapter : public VCC::Core::cartridge_callbacks
{
    multi_slot_adapter(
        size_t slot,
        const cpak_callbacks& callbacks)
        : slot_(slot),
          callbacks_(callbacks)
    {}
    path_type configuration_path() const override {
        return {}; // Carts do NOT modify ini paths!!!
    }
    void write_memory_byte(unsigned char value, unsigned short address) override {
        callbacks_.write_memory_byte(slot_, value, address);
    }
    unsigned char read_memory_byte(unsigned short address) override {
        return callbacks_.read_memory_byte(slot_, address);
    }
    void assert_cartridge_line(bool state) override {
       callbacks_.assert_cartridge_line(slot_, state);
    }
    void assert_interrupt(Interrupt intr, InterruptSource src) override {
        callbacks_.assert_interrupt(slot_, intr, src);
    }
private:
    size_t slot_;
    const cpak_callbacks& callbacks_;
};

struct boot_slot_adapter : public ::VCC::Core::cartridge_callbacks
{
    path_type configuration_path() const override {
        return {}; // Carts do NOT modify ini paths!!!
    }
    void write_memory_byte(unsigned char value, unsigned short address) override {
        MemWrite8(value, address);
    }
    unsigned char read_memory_byte(unsigned short address) override {
        return MemRead8(address);
    }
    void assert_cartridge_line(bool line_state) override {
        SetCart(line_state);
    }
    void assert_interrupt(Interrupt interrupt, InterruptSource interrupt_source) override {
        PakAssertInterupt(interrupt, interrupt_source);
    }
};

//--------------------------------------------------------
//	Plugin exports
//--------------------------------------------------------

void PakTimer()
{
	VCC::Util::section_locker lock(gPakMutex);
	gPakRouter.process_horizontal_sync();
//	gCartSlots[0]->process_horizontal_sync();
}

//TODO
void ResetBus()
{
	VCC::Util::section_locker lock(gPakMutex);
	gCartSlots[0]->reset();
}

void GetModuleStatus(SystemState *SMState)
{
	gPakRouter.plugin_status(SMState->StatusLine,sizeof(SMState->StatusLine));
}

unsigned char PakReadPort (unsigned char port)
{
	VCC::Util::section_locker lock(gPakMutex);
	return gPakRouter.read_port(port);
//	return gCartSlots[0]->read_port(port);
}

void PakWritePort(unsigned char Port,unsigned char Data)
{
	VCC::Util::section_locker lock(gPakMutex);
	gPakRouter.write_port(Port,Data);
//	gCartSlots[0]->write_port(Port,Data);
}

unsigned char PackMem8Read (unsigned short Address)
{
	VCC::Util::section_locker lock(gPakMutex);
	return gPakRouter.read_memory_byte(Address&32767);
//	return gCartSlots[0]->read_memory_byte(Address&32767);
}

unsigned short PackAudioSample()
{
	VCC::Util::section_locker lock(gPakMutex);
	return gPakRouter.sample_audio();
}

void PakAssertLine(bool val){
	//FIME: what goes here?
}

//--------------------------------------------------------
// Convert PAK interrupt assert to CPU assert or Gime assert.
//--------------------------------------------------------
void PakAssertInterupt(Interrupt interrupt, InterruptSource source)
{
	// FIXME: GimeAssertCartInterupt() should be from assert line
	// but plugin and Gime interrupt code would need to be fixed
	// to be level based first...

	(void) source; // not used

	switch (interrupt) {
	case INT_CART:
		GimeAssertCartInterupt();
		break;
	case INT_NMI:
		CPUAssertInterupt(IS_NMI, INT_NMI);
		break;
	}
}

//--------------------------------------------------------
// Build Plugin dynamic menus
//--------------------------------------------------------
void BuildCartMenu()
{
	//VCC::Util::section_locker lock(gPakMutex);
	using VCC::Bus::gVccCartMenu;
	gVccCartMenu.clear();

	if (gCartSlots[0]->name().empty()) {
		// Items to load the boot slot
		gVccCartMenu.add("Load &MPI", ControlId(3), MIT_StandAlone);
		gVccCartMenu.add("Load &DLL", ControlId(1), MIT_StandAlone);
		gVccCartMenu.add("Load &ROM", ControlId(4), MIT_StandAlone);
	} else {
		// Items to remove boot slot.
		std::string tmp = "&Eject " + gCartSlots[0]->name();
		gVccCartMenu.add(tmp, ControlId(2), MIT_StandAlone);

		// Items from loaded plugins
		for (int slot : {0, 4, 3, 2, 1}) {
			auto* cart = gCartSlots[slot].get();
			if (!cart) continue;
			for (int ndx = 0; ndx < MAX_MENU_ITEMS; ndx++) {
				menu_item_entry item;
				if (!cart->get_menu_item(&item, ndx)) break;
				// Bias menu id's for slot location
				if (item.menu_id >= MID_CONTROL)
					item.menu_id += (slot * 50);
				gVccCartMenu.add(item.name,item.menu_id,item.type);
			}
		}
	}
}

//--------------------------------------------------------
// Dialog for loading boot slot plugin
//--------------------------------------------------------
void LoadCartridgeDialog(int type)
{
	char inifile[MAX_PATH];
	GetIniFilePath(inifile);

	static char cartDir[MAX_PATH] = "";
	FileDialog dlg;
	if (type == 0) {
		dlg.setTitle(TEXT("Load Program Pack"));
		dlg.setFilter("Hardware Packs\0*.dll\0"
				"All Supported Formats (*.dll;*.ccc;*.rom)\0*.dll;*.ccc;*.rom\0\0");
		Setting().read("DefaultPaths", "DLLPath", "", cartDir, MAX_PATH);
	} else {
		dlg.setTitle(TEXT("Load ROM"));
		dlg.setFilter("Rom Packs(*.ccc;*.rom)\0*.ccc;*.rom\0"
				"All Supported Formats (*.dll;*.ccc;*.rom)\0*.dll;*.ccc;*.rom\0\0");
		Setting().read("DefaultPaths", "RomPath", "", cartDir, MAX_PATH);
	}
	dlg.setInitialDir(cartDir);
	dlg.setFlags(OFN_FILEMUSTEXIST);
	if (dlg.show()) {
		auto status = PakLoadCartridge(dlg.path());
		if (status == cartridge_loader_status::success) {
			char filetype[4];
			dlg.getdir(cartDir);
			dlg.gettype(filetype);
			if ((strcmp(filetype,"dll") == 0) | (strcmp(filetype,"DLL") == 0 )) {  // DLL?
				Setting().write("DefaultPaths", "DLLPath", cartDir);
			} else {
				Setting().write("DefaultPaths", "RomPath", cartDir);
			}
		}
	}
}

//--------------------------------------------------------
// Load cartridge to boot slot
//--------------------------------------------------------

cartridge_loader_status PakLoadCartridge(const char* filename)
{
	static const std::map<cartridge_loader_status, UINT> string_id_map = {
		{ cartridge_loader_status::already_loaded, IDS_MODULE_ALREADY_LOADED},
		{ cartridge_loader_status::cannot_open, IDS_MODULE_CANNOT_OPEN},
		{ cartridge_loader_status::not_found, IDS_MODULE_NOT_FOUND },
		{ cartridge_loader_status::not_loaded, IDS_MODULE_NOT_LOADED },
		{ cartridge_loader_status::not_rom, IDS_MODULE_NOT_ROM },
		{ cartridge_loader_status::not_expansion, IDS_MODULE_NOT_EXPANSION }
	};

	const auto result(load_any_cartridge(0, filename));
	if (result == cartridge_loader_status::success)
	{
		Setting().write("Module", "OnBoot", filename);
		return result;
	}

	// Tell user why load failed
	auto error_string(VCC::Core::cartridge_load_error_string(result));
	error_string += "\n\n";
	error_string += filename;
	MessageBox(EmuState.WindowHandle, error_string.c_str(), "Load Error", MB_OK | MB_ICONERROR);

	return result;
}

//--------------------------------------------------------
// Send plugin list to router
//--------------------------------------------------------
void UpdateRouterSlots() {
	std::array<VCC::Core::cartridge*, 5> ptrs;
	for (int i = 0; i < 5; ++i) {
		ptrs[i] = dynamic_cast<VCC::Core::cartridge*>(gCartSlots[i].get());
	}
	gPakRouter.set_slots(ptrs);
}

//--------------------------------------------------------
// Unload slot
//--------------------------------------------------------
void UnloadCartridge(int slot)
{
	// Security lock
	VCC::Util::section_locker lock(gPakMutex);

	// gActiveCartrige is one of a ROM, DLL, or NULL cartridge
	gCartSlots[slot]->stop();
	gCartSlots[slot] = std::make_unique<VCC::Core::null_cartridge>();
	gCartHandles[slot].reset();
	gCartSlots[slot]->start();

	// Update router slot list
	UpdateRouterSlots();

	// update menus
	SendMessage(EmuState.WindowHandle,WM_VCC_UPD_MENU,(WPARAM) 0,(LPARAM) 0);
}

//--------------------------------------------------------
// Unload boot slot
//--------------------------------------------------------
void UnloadDll()
{
	UnloadCartridge(0);
	gPakRouter.set_active_slot(0);
}

//--------------------------------------------------------
// Load cartridge to slot
//--------------------------------------------------------

static cartridge_loader_status load_any_cartridge(int slot, const char *filename)
{
	slot_id_type SlotId = slot;

	// DLL plugins need ini file path so they can manage settings
	char iniPath[MAX_PATH]="";
	GetIniFilePath(iniPath);

	// FIXME:  Adapters are not necessary and shold be elminiated
	std::unique_ptr<VCC::Core::cartridge_callbacks> adapter;
	if (SlotId == 0) {
		adapter = std::make_unique<boot_slot_adapter>();
	} else { 
		adapter = std::make_unique<multi_slot_adapter>(SlotId -1, slot_callbacks);
	}

	// Load the cartridge
	auto loadedCartridge = VCC::Core::load_cartridge(
		filename,
		std::move(adapter),   // FIXME remove this (requires router and cart def work)
		SlotId,
		iniPath,
		EmuState.hMsgProxy,
		slot_callbacks);

	if (loadedCartridge.load_result != cartridge_loader_status::success) {
		DLOG_C("pakinterface load_any_cartridge slot %d %s failed\n", slot, filename);
		return loadedCartridge.load_result;
	}

	// Unload current cart in slot (could be the empty cartridge)
	UnloadCartridge(slot);
	if (slot == 0) gPakRouter.set_active_slot(0);

	VCC::Util::section_locker lock(gPakMutex);
	strcpy(DllPath, filename);
	gCartSlots[slot]   = std::move(loadedCartridge.cartridge);
	gCartHandles[slot] = std::move(loadedCartridge.handle);

	if (slot == 0) {
		DLOG_C("\npakinterface initialize boot slot\n",);
		// Initialize the cartridge, if it is MPI it will update the startup slot
		gCartSlots[0]->start();
		gPakRouter.reset();
		EmuState.ResetPending = 2;
		SendMessage(EmuState.WindowHandle,WM_VCC_UPD_MENU,(WPARAM) 0,(LPARAM) 0);
	} else {
		DLOG_C("\npakinterface initialize cart slot %d\n",slot);
		gCartSlots[slot]->start();
	}

	// Update router slot list.  TODO:  Do this sooner??
	UpdateRouterSlots();

	// TODO: update menus or reset if slot active (later)
	//SendMessage(EmuState.WindowHandle,WM_VCC_UPD_MENU,(WPARAM) 0,(LPARAM) 0);

	return loadedCartridge.load_result;
}

void GetCurrentModule(char *DefaultModule)
{
	strcpy(DefaultModule,DllPath);
	return;
}

void UpdateBusPointer()
{
	// Do nothing for now. What the plan was for this is unknown.
}

//--------------------------------------------------------
// unload bootslot
//--------------------------------------------------------
void UnloadPack()
{
	//UnloadDll();
	UnloadCartridge(0);
	strcpy(DllPath,"");
	SetCart(0);
	gPakRouter.set_active_slot(0);
	EmuState.ResetPending=2;

	char inifile[MAX_PATH];
	GetIniFilePath(inifile);
	Setting().delete_key("Module", "OnBoot");
}

//--------------------------------------------------------
// load bootslot.  type: 0 program cartridge 1 rom cartridge
//--------------------------------------------------------
void LoadPack(int type) {
	LoadCartridgeDialog(type);
	gPakRouter.set_active_slot(0);
	EmuState.ResetPending=2;
}

//--------------------------------------------------------
// CartMenuActivated is called from VCC WndPrc when a cartridge
// menu item is clicked. MenuID is unsigned value less that 250
//--------------------------------------------------------
void CartMenuActivated(unsigned int MenuID)
{
	if (MenuID >= 250) return;

	switch (MenuID)
	{
	case 1:
		LoadPack(0);
		return;

	case 2:
		UnloadPack();
		return;

	case 3:
	{
		char path[MAX_PATH];
		GetModuleFileName(nullptr,path,MAX_PATH);
		PathRemoveFileSpec(path);
		strncat(path,"mpi.dll",MAX_PATH);
		PakLoadCartridge(path);
		return;
	}
	case 4:
		LoadPack(1);
		return;

	default:
		break;
	}

	VCC::Util::section_locker lock(gPakMutex);

	// menu_item_clicked takes unsigned char. This limits total number of menu items
	// to 255. 50 are allocated to host cart and 50 each to mpi carts for 250 total.

	//gActiveCartrige->menu_item_clicked(menu_item);
	gCartSlots[0]->menu_item_clicked(MenuID);
}

//--------------------------------------------------------------
// Messages from MPI
// -------------------------------------------------------------

// Set startup slot currenly comes from a radio button click in the MPI.
// TODO: Vcc init sets it to zero.  MPI send it from settings via message.
bool SetActiveSlot(unsigned int cts)
{
	DLOG_C("Pakinterface SetActiveSlot CTS/SCS: %d\n",cts);

	// Startup sllot is 0-4 (cts+1).  This allows the pakrouter to decide where to
	// apply memory and regsister I/O requests from the Coco CPU.  If startup slot
	// is zero the MPI slots are ignored. If start up slot is non zero it controls
	// the cts/scs functions of the mpi slots, numbered 1-4.

	gPakRouter.set_active_slot(cts+1);
	return true;
}

// Unload slot contents
// mpi/configuration_dialog.cpp:231
// mpi/multipak_cartridge.cpp:338

bool UnloadSlot(unsigned int slot)
{
	DLOG_C("Pakinterface UnloadSlot: %d\n",slot);
	if (slot == 0) SetActiveSlot(0);
	UnloadCartridge(slot);
	return true;
}

// Load slot (1-4)
// mpi/configuration_dialog.cpp
// mpi/multipak_cartridge.cpp
bool LoadSlot(unsigned int slot, const PluginMsgData * data)
{
	if (slot < 1 || slot > 4) {
		DLOG_C("Pakinterface LoadSlot bad slot num\n");
		return false;
	}

	if (data == nullptr) {
		DLOG_C("Pakinterface LoadSlot null data pointer\n");
		return false;
	}

	if (data->size != sizeof(PluginMsgData)) {
		DLOG_C("Pakinterface LoadSlot bad data size\n");
		return false;
	}

	//DLOG_C("Pakinterface LoadSlot: %d %s\n",slot,data->pluginPath);

	// TODO remove ignore check after parallel testing completes
	gIgnoreNextDuplicateCheck = true;
	load_any_cartridge(slot, data->pluginPath);
	return true;
}

//Following to remind me of better way for plugin descriptions.(loading not required)
//PrintLogC("Comments: %s\n", VCC::Util::GetVersionInfo(data->pluginPath,"Comments"));

