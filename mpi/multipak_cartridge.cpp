#define USE_LOGGING
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
#include "multipak_cartridge.h"
#include "cartridge_slot_adapter.h"
#include "mpi.h"
#include "resource.h"
#include <vcc/util/coreutil.h>
#include <vcc/util/textutil.h>
#include <vcc/util/filesystem.h>
#include <vcc/util/logger.h>
#include <vcc/bus/cartridge_menu.h>
#include <vcc/bus/cartridge_menuitem.h>
#include <vcc/bus/cartridge_messages.h>


// SlotId is an unsigned int 0-4 used to indicate to a cartridge which slot
// it is in.  SlotId 0 is the boot slot, SlotId's 1-4 are multipak slots
// mpi_slot indexes used elsewhere in this source differ, they represent only
// multipak mpi_slots and are numbered 0-3,  (SlotId = mpi_slot+1)


multipak_cartridge::multipak_cartridge(
	multipak_configuration& configuration)
	:
	configuration_(configuration) {}

// MPI Cart information 
multipak_cartridge::name_type multipak_cartridge::name() const
{
	return ::VCC::Util::load_string(gModuleInstance, IDS_MODULE_NAME);
}

multipak_cartridge::catalog_id_type multipak_cartridge::catalog_id() const
{
	return ::VCC::Util::load_string(gModuleInstance, IDS_CATNUMBER);
}

multipak_cartridge::description_type multipak_cartridge::description() const
{
	return ::VCC::Util::load_string(gModuleInstance, IDS_CATNUMBER);
}

void multipak_cartridge::start()
{
	// Mount mpi slots 
	for (auto mpi_slot(0u); mpi_slot < slots_.size(); mpi_slot++)
	{
		const auto path(VCC::Util::find_pak_module_path(
					configuration_.slot_cartridge_path(mpi_slot)));
		if (!path.empty())
		{
			DLOG_C("\nmultipak_cartridge.start slot:%d %s\n",mpi_slot+1,path.c_str()); 
			CartLoadRequest slotData{};
			strcpy_s(slotData.pluginPath, slotData.size, path.c_str());
			SendLoadSlot(gVccWnd, mpi_slot+1, slotData); // Slot is 1-4
		}
	}

	switch_slot_ = configuration_.selected_slot();
	SendActiveSlot(gVccWnd, switch_slot_);
	SendMessage(gVccWnd,WM_VCC_UPD_MENU,(WPARAM) 0,(LPARAM) 0);
}

void multipak_cartridge::stop()
{
	DLOG_C("multipak_cartridge stop\n");
	// pakinteface will automatically stop multipak slots before
	// the boot slot is stopped. It does not need to be done here
	gConfigurationDialog.close();
}

void multipak_cartridge::reset()
{
	DLOG_C("multipak_cartridge reset\n");

	VCC::Util::section_locker lock(mutex_);

	unsigned char mpi_slot = switch_slot_ & 3;
	switch_slot_ = mpi_slot;

	// Tell WndPrc what the active slot is now (for pakinterface)
	SendActiveSlot(gVccWnd, switch_slot_);

	// TODO:  Should pakrouter be doing this
	for (const auto& cartridge_slot : slots_)
	{
		cartridge_slot.reset();
	}
}

void multipak_cartridge::process_horizontal_sync()
{
	DLOG_C("XXX multipak_cartridge hsync\n");
}

void multipak_cartridge::write_port(unsigned char port_id, unsigned char value)
{
	DLOG_C("XXX multipak_cartridge write_port\n"); 
}

unsigned char multipak_cartridge::read_port(unsigned char port_id)
{
	DLOG_C("XXX multipak_cartridge read_port\n"); 
	return 0;
}

unsigned char multipak_cartridge::read_memory_byte(unsigned short memory_address)
{
	DLOG_C("XXX multipak_cartridge read_memory_byte\n"); 
	return 0;
}

void multipak_cartridge::status(char* text_buffer, size_t buffer_size)
{
	DLOG_C("XXX multipak_cartridge status\n"); 
}

unsigned short multipak_cartridge::sample_audio()
{
	DLOG_C("XXX multipak_cartridge sample_audio\n"); 
	return 0;
}

void multipak_cartridge::menu_item_clicked(unsigned char menu_item_id)
{
	DLOG_C("multipak_cartridge menu_item_clicked %d\n", menu_item_id); 

	if (menu_item_id == 19)	//MPI Config
	{
		gConfigurationDialog.open();
	}
}


// Return MPI menu
bool multipak_cartridge::get_menu_item(menu_item_entry* item, size_t index)
{
	using VCC::Bus::gDllCartMenu;
	if (!item) return false;
	if (index == 0) {
		gDllCartMenu.clear();
		gDllCartMenu.add("", 0, MIT_Seperator);
		gDllCartMenu.add("MPI Config", ControlId(19), MIT_StandAlone);
	}
	return gDllCartMenu.copy_item( *item, index);
}


multipak_cartridge::label_type multipak_cartridge::slot_label(slot_id_type mpi_slot) const
{
	VCC::Util::section_locker lock(mutex_);
	return "";
}

multipak_cartridge::description_type multipak_cartridge::slot_description(slot_id_type mpi_slot) const
{
	VCC::Util::section_locker lock(mutex_);
	return "";
}

// Load a cartridge in multi slot
multipak_cartridge::mount_status_type multipak_cartridge::mount_cartridge(
	slot_id_type mpi_slot, const path_type& filename)
{
	// Send load slot request message to WndProc
	CartLoadRequest slotData{};
	strcpy_s(slotData.pluginPath, slotData.size, filename.c_str());
	SendLoadSlot(gVccWnd, mpi_slot+1, slotData); // Slot is 1-4

	// Send menu update to WndProc
	SendMessage(gVccWnd,WM_VCC_UPD_MENU,(WPARAM) 0,(LPARAM) 0);

	//	return loadedCartridge.load_result;
	return mount_status_type::success;
}

// The following has no effect until VCC is reset
void multipak_cartridge::switch_to_slot(slot_id_type mpi_slot)
{
	DLOG_C("multipak_cartridge set selected switch slot (0-3) %d\n", mpi_slot); 
	switch_slot_ = mpi_slot;
}

multipak_cartridge::slot_id_type multipak_cartridge::selected_switch_slot() const
{
	DLOG_C("multipak_cartridge get selected switch slot %d\n", switch_slot_); 
	return switch_slot_;
}

