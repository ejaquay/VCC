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

	private:
		inline bool is_disk_port(int port) { return port >= 0x40 && port <= 0x5F; }

	    template<typename F>
	    void for_each_slot(F func);

		// Cartridge slots: 0 = boot slot, 1..4 = MPI slots
		std::array<cartridge*, 5> slots_;

		// Startup and current scs and cts slots 0..4
		int startup_slot_;
		int scs_slot_;     // disk controller slot
		int cts_slot_;     // cartridge slot
	};

	// Template to broadcast function to all slots
	// If only the boot slot is loaded, just do that one
	// If mpi slots are loaded do them all in 4 3 2 1 order
	template<typename F>
	void PakRouter::for_each_slot(F func)
	{
		// Boot-only mode: only slot 0
		if (cts_slot_ < 1) {
			auto* cart = slots_[0];
			if (cart) func(cart);
			return;
		}
		// MPI-active mode: scan 4 > 1
		for (int i = 4; i > 0; i--) {
			auto* cart = slots_[i];
			if (cart) func(cart);
		}
	}
};
