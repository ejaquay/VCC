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

#include <vcc/bus/cartridge.h>

namespace VCC::Core
{
	class PakRouter
	{
	public:
		// Constructor
		PakRouter();

		// UI driven router control
		void reset();
		void set_slots(std::array<cartridge*, 5> slots);
		void set_active_slot(unsigned active_slot);

		// Exported plugin cpu loop operations
		void process_horizontal_sync();
		void write_port(unsigned char port, unsigned char data);
		unsigned char read_port(unsigned char port);
		unsigned char read_memory_byte(unsigned short address);
		unsigned short sample_audio();

		// Render loop callbacks
		void plugin_status(char * txt, size_t len);

		// Plugin cpu loop callbacks
		void cart_write_memory(int slot, unsigned char val, unsigned short adr);
		unsigned char cart_read_memory(int slot,unsigned short adr);
		void cart_assert_line(int slot, bool state);  // see line_states_
		void cart_assert_interrupt(int slot, Interrupt intr, InterruptSource src);

	private:

		// Test if a disk port
		inline bool is_disk_port(int port)
		{ 
			return port >= 0x40 && port <= 0x5F;
		}

		// Test for inactive MPI
		inline bool mpi_not_active() const
		{
			return cts_slot_ == 0;
		}

		// Get plugin status text
		inline void slot_get_status(int slot, char * buf, size_t len)
		{
			if (auto* cart = slots_[slot])
				cart->status(buf,len);
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
		std::array<cartridge*, 5> slots_{};

		// CTS and SCS slot numbers 0..4
		// cts_slot_ == 0 implies that the MPI is not active.  When
		// MPI is active these are one more than actual SCS and CTS
		int cts_slot_ = 0; // cartridge slot    (cart reads/writes)
		int scs_slot_ = 0; // spare select slot (disk and line state)

		// Line states per active slot. Select cart (SCS) can change
		// line state but if SCS changes an assert_cart_line(state)
		// should be generated for the selected slot if that causes
		// active_line_state_ to change.
		std::array<bool,5> line_states_{};
		bool active_line_state_ = false;

	};
}

