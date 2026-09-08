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

#include <vcc/bus/cartridge.h>
#include <vcc/bus/cpak_cartridge.h>
#include <vcc/bus/pakrouter.h>
#include <vcc/util/logger.h>
#include <typeinfo>

// pakrouter handles routing and control of cpu driven plugin exports.
// An array of pointers to installed plugin objects is used. The array
// contains five slots, 0 = boot slot, 1..4 are MPI slots, if present.

namespace VCC::Core
{
	PakRouter::PakRouter() { 
		startup_slot_ = 0;
		scs_slot_ = 0;
		cts_slot_ = 0;
		slots_.fill(nullptr);
	}

	// Set the startup slot 0..4.  Message from MPI plugin.  Plugin is responsible
	// for correctness, if the startup slot is empty, the plughin should set the
	// startup slot to zero.
	void PakRouter::set_startup_slot(unsigned startup_slot) {
		startup_slot_ = (startup_slot > 4) ? 0 : startup_slot;
		DLOG_C("PakRouter::set_startup_slot %d\n",startup_slot_);
	}

	// reset() is invoked on hard reset or power up.
	void PakRouter::reset()
	{
		scs_slot_ = cts_slot_ = startup_slot_;  // FIXME Reset does not read startup slot
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

	// horizontal sync
	void PakRouter::process_horizontal_sync()
	{
		for_each_slot([&](auto* cart){
    		cart->process_horizontal_sync();
		});
	}

	// Audio samples.
	unsigned short PakRouter::sample_audio()
	{
		unsigned short sample = 0;
		for_each_slot([&](auto* cart){
			sample += cart->sample_audio();
		});
		return sample;
	}

	// Cart memory reads only from CTS slot
	unsigned char PakRouter::read_memory_byte(unsigned short address)
	{
		auto* cart = slots_[cts_slot_];
		return cart->read_memory_byte(address);
	}

	void PakRouter::write_port(unsigned char port, unsigned char value)
	{
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
			auto* cart = slots_[scs_slot_];
			if (cart) cart->write_port(port, value);
			return;
		}
		// Broadcast other port writes
		for_each_slot([&](auto* cart){
    		cart->write_port(port, value);
		});
	}

	unsigned char PakRouter::read_port(unsigned char port)
	{
		// Slot-select register (0x7F)
		if (port == 0x7F) {
			return (cts_slot_ << 4) | scs_slot_;
		}
		// Disk controller ports (0x40–0x5F) scs slot only
		if (is_disk_port(port)) {
			auto* cart = slots_[scs_slot_];
			return cart ? cart->read_port(port) : 0;
		}
		// No MPI slot zero only
		if (cts_slot_ < 1) {
			auto* cart = slots_[0];
			if (!cart) return 0;
			return cart->read_port(port);
		}
		// MPI ports priority scan
		for (int i = 4; i > 0; i--) {
			auto* cart = slots_[i];
			if (!cart) continue;
			unsigned char data = cart->read_port(port);
			if (data != 0) return data;
		}
		return 0;
	}
}
