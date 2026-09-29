//#define USE_LOGGING
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

#include "mc6821.h"
#include "MachineDefs.h"
#include "tcc1014registers.h"
#include "tcc1014mmu.h"
#include "pakrouter.h"
#include <array>
#include <vcc/bus/cartridge.h>
#include <vcc/bus/cpak_cartridge.h>
#include <vcc/util/logger.h>
#include <typeinfo>
#include <algorithm>

// pakrouter handles routing and control of the plugin binary interface
// An array of pointers to installed plugin objects is used. The array
// contains five slots, 0 = boot slot, 1..4 are MPI slots, if present.
// binary calls are routed to/from either boot slot or the MPI slots.

namespace VCC::Core
{
	PakRouter::PakRouter()
	{ 
		scs_slot_ = 0;
		cts_slot_ = 0;
		slots_.fill(nullptr);
		line_states_.fill(false);
		active_line_state_ = false;
	}

	// Set the active slot 0..4.  This is called via a message from the
	// MPI when it is active.  It is also used to clear the active_slot
	// whenever the boot slot (where MPI must live) is empty.
	void PakRouter::set_active_slot(unsigned active_slot)
	{
		DLOG_C("PakRouter::set_active_slot %d\n",active_slot);
		scs_slot_ = active_slot;
		cts_slot_ = active_slot;
	}

	// reset() is invoked on hard reset or power up.
	void PakRouter::reset()
	{
		DLOG_C("PakRouter::reset\n");
	}

	// Set plugin pointer array. This is called anytime a plugin is updated.
	// Take care to not change plugin in an active slot (cts,scs)
	void PakRouter::set_slots(std::array<cartridge*, 5> slots)
	{
		DLOG_C("PakRouter::set_slots");
		for (int i = 0; i <= 4; ++i) {
        	slots_[i] = slots[i];
    		DLOG_C(" %d:%s",i,slots_[i]->name().c_str());
			if (i == cts_slot_)
				DLOG_C("*");
			else if (i == scs_slot_)
				DLOG_C("~");
		}
		DLOG_C("\n");
	}

	// Callbacks
	void PakRouter::cart_write_memory(int slot, unsigned char val, unsigned short adr) {
//DLOG_C("%dw ",slot);
		MemWrite8(val, adr);
	};

	unsigned char PakRouter::cart_read_memory(int slot,unsigned short adr){
//DLOG_C("%dr ",slot);
		return MemRead8(adr);
	};

	void PakRouter::cart_assert_line(int slot, bool state){
		// TODO:  add logic to set state from active cart
		SetCart(state);
	};

	void PakRouter::cart_assert_interrupt(int slot, Interrupt intr, InterruptSource src){
//		DLOG_C("PakRouter interupt slot:%d int:%d\n", slot, intr);
		(void) src; // not used
		switch (intr) {
		case INT_CART:
			GimeAssertCartInterupt();
			break;
		case INT_NMI:
			CPUAssertInterupt(IS_NMI, INT_NMI);
			break;
		}
	};

	// Horizontal sync
	void PakRouter::process_horizontal_sync()
	{
		if (mpi_not_active()) {
			slot_process_hsync(0);
			return;
		}
		for (int i = 4; i > 0; i--)
			slot_process_hsync(i);
	}

	// Sample audio
	unsigned short PakRouter::sample_audio()
	{
		// Cartridge audio is two packed unsigned 8-bit channels. Audio-producing
		// cartridges use 0x80 as the midpoint level, while cartridges without
		// PakSampleAudio return 0 through the compatibility shim. The old code 
		// added complete 16-bit packed samples, which allowed the right channel
		// to carry into the left channel and made two 0x8080 midpoint samples wrap to
		// 0x0100. Mix the channels independently around 0x80 instead.
		// ALTERNATE: Return after first cart that supplies a non-zero sample???
	
		int left = 0;
		int right = 0;
		int mask = 0xFF;
		int center = 0x80;
		bool have_sample = false;

		// if mpi is not loaded return sample from boot slot
		if (mpi_not_active()) {
			return slot_sample_audio(0);
		}
		// sum samples for MPI slots
		for (int i = 1; i <= 4; i++) {
			int sample = slot_sample_audio(i);
			if (sample != 0) {
				have_sample = true;
				left += ((sample >> 8) & mask) - center;
				right += (sample & mask) - center;
			}
		};
		if (!have_sample) return 0;
		left = std::clamp(left + center, 0, mask);
		right = std::clamp(right + center, 0, mask);
		return right + (left << 8);
	}

	// Cart memory reads only from CTS slot
	unsigned char PakRouter::read_memory_byte(unsigned short address)
	{
		return PakRouter::slot_read_memory(cts_slot_,address);
	}

	// Write to port
	void PakRouter::write_port(unsigned char port, unsigned char value)
	{
//DLOG_C("pw p:%d v:%d\n",port,value);
		// Slot-select register (0x7F)
		if (port == 0x7F) {
			int scs = value & 3;
			int cts = (value >> 4) & 3;
			scs_slot_ = scs + 1;
			cts_slot_ = cts + 1;
			return;
		}
		// Disk controller ports (0x40–0x5F) scs slot only
		if (is_disk_port(port)) {
			PakRouter::slot_write_port(scs_slot_, port, value);
			return;
		}
		// Broadcast other port writes
		for (int i = 1; i <= 4; i++) {
			PakRouter::slot_write_port(i, port, value);
		}
	}

	// Read port
	unsigned char PakRouter::read_port(unsigned char port)
	{
//DLOG_C("pr p:%d v:%d\n",port);
		// Slot-select register (0x7F)
		if (port == 0x7F) {
			int scs = (scs_slot_ -1) & 3;
			int cts = (cts_slot_ -1) & 3;
			return (cts << 4) | scs;
		}
		// Disk controller ports (0x40–0x5F) scs slot only
		if (is_disk_port(port)) {
			return PakRouter::slot_read_port(scs_slot_, port);
		}
		// No MPI slot zero only
		if (mpi_not_active()) {
			return PakRouter::slot_read_port(0, port);
		}
		// MPI ports priority scan
		for (int i = 1; i <= 4; i++) {
			unsigned char data = PakRouter::slot_read_port(i, port);
			if (data != i) return data;
		}
		return 0;
	}
}
