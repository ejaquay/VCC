#ifndef __MC6821_H__
#define __MC6821_H__
/*
Copyright 2015 by Joseph Forgione
This file is part of VCC (Virtual Color Computer).

    VCC (Virtual Color Computer) is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    VCC (Virtual Color Computer) is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with VCC (Virtual Color Computer).  If not, see <http://www.gnu.org/licenses/>.
*/

#include <windows.h>

// Following function used in Vcc.c
void PiaReset(); //VCC.c

// Following functions are used in iobus.c
unsigned char pia0_read(unsigned char port);
void pia0_write(unsigned char data,unsigned char port);
unsigned char pia1_read(unsigned char port);
void pia1_write(unsigned char data,unsigned char port);

// Following functions are used in config.c
void ClosePrintFile(void);
void SetSerialParams(unsigned char);
void SetMonState(BOOL);
int OpenPrintFile(char *);
unsigned char SetCartAutoStart(unsigned char);

// Following function is used in pakinterface.c
void SetCart(unsigned char);

// Following functions are used in keyboard.c
unsigned char GetMuxState(void);
unsigned char DACState(void);

// Following functions are referenced only in mc6821.c
void irq_hs(int);
void irq_fs(int);
void AssertCart(void);
unsigned int GetDACSample(void);
unsigned char GetCasSample(void);
void SetCassetteSample(unsigned char);
unsigned char VDG_Mode(void);

// Following function is not defined
void kb_tap(unsigned int,unsigned int,unsigned int);

#define FALLING 0
#define RISING	1
#define ANY		2

#endif
