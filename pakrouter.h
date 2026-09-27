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
#pragma once

#include "mc6821.h"
#include "MachineDefs.h"
#include "tcc1014registers.h"
#include "tcc1014mmu.h"
#include <array>
#include <vcc/bus/cartridge.h>
#include <vcc/bus/cpak_cartridge.h>

namespace VCC::Core
{
	class PakRouter
	{
	public:
		// Constructor
		PakRouter();

		void set_startup_slot(unsigned startup_slot);
		void set_slots(std::array<cartridge*, 5> slots);

		// Plugin operations
		void reset();
		void process_horizontal_sync();
		void write_port(unsigned char port, unsigned char data);
		unsigned char read_port(unsigned char port);
		unsigned char read_memory_byte(unsigned short address);
		unsigned short sample_audio();

		// Callbacks
		void cart_write_memory(int slot, unsigned char val, unsigned short adr) {
			MemWrite8(val, adr);
		};
		unsigned char cart_read_memory(int slot,unsigned short adr){
			return MemRead8(adr);
		};
		void cart_assert_line(int slot, bool state){
			SetCart(state);
		};
		void cart_assert_interrupt(int slot, Interrupt intr, InterruptSource src){
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

	private:

		// Test if a disk port
		inline bool is_disk_port(int port)
		{ 
			return port >= 0x40 && port <= 0x5F;
		}

		// Test for inactive MPI
		inline bool mpi_not_active() const
		{
			return cts_slot_ < 1;
		}

		// slot hsync 
		inline void slot_process_hsync(int slot)
		{
			if (auto* cart = slots_[slot])
        		cart->process_horizontal_sync();
		}

		// get audio sample from slot 
		inline int slot_sample_audio(int slot)
		{
			if (auto* cart = slots_[slot])
				return cart->sample_audio();
			return 0;
		}

		// Read slot memory
		inline unsigned char slot_read_memory(int slot, unsigned short address)
		{
			if (auto* cart = slots_[slot])
				return cart->read_memory_byte(address);
			return 0;
		}

		// Write slot port
		inline void slot_write_port(int slot,unsigned char port, unsigned char value)
		{
			if (auto* cart = slots_[slot])
				cart->write_port(port, value);
		}

		// Read slot port
		inline unsigned char slot_read_port(int slot, unsigned char port)
		{
			if (auto* cart = slots_[slot])
				return cart->read_port(port);
			return 0;
		}

	private:

		// Cartridge slots: 0 = boot slot, 1..4 = MPI slots
		std::array<cartridge*, 5> slots_;

		// Startup and current scs and cts slots 0..4
		int startup_slot_;
		int scs_slot_;     // disk controller slot
		int cts_slot_;     // cartridge slot
	};
}

