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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "defines.h"
#include "mc6821.h"
#include "hd6309.h"
#include "keyboard.h"
#include "tcc1014graphics.h"
#include "tcc1014registers.h"
#include "coco3.h"
#include "pakinterface.h"
#include "cassette.h"
#include "logger.h"
#include "resource.h"
#include <cstdint>

// Comments added by EJJ April 2021
//
// The following four arrays were previously named rega* and regb*.
// Renamed to pia0* and pia1* to avoid confusion with A,B ports.
// These contain the six PIA registers for each PIA. (two elements
// in the *_dd arrays are unused).  Each PIA contains two peripheral
// data registers PDA and PDB, two data direction registers DDA and DDB,
// and two control registers CRA and CRB.

static unsigned char pia0[4]={0,0,0,0};      // PIA0 PDA,CRA,PDB,CRB
static unsigned char pia1[4]={0,0,0,0};      // PIA1 PDA,CRA,PDB,CRB
static unsigned char pia0_dd[4]={0,0,0,0};   // PIA0 DDA,xxx,DDB,xxx
static unsigned char pia1_dd[4]={0,0,0,0};   // PIA1 DDA,xxx,DDB,xxx

// In addition each PIA has two eight bit peripheral interfaces
// (PA and PB) and several control lines: CS0, CS1, and CS2 chip select
// lines; RS0 and RS1 register select lines; and CA1, CA2, CB1 and CB2
// interrupt status and control lines.

// Register select lines (RS0 and RS1) in conjunction with bit
// two of the control registors (CRA2 amd CRB2) determine which
// register is operated on.  The chip select lines are connected
// to the ports starting FF00 and FF20, thus reading/writing
// these ports affect the registers as follows:
//
//  PIA0  PIA1  RS1  RS0 CRA2 CRB2  Reg
//  ----  ----  ---  --- ---- ----  ---
//  FF00  FF20   0    0    1    x   PDA
//  FF00  FF20   0    0    0    x   DDA
//  FF01  FF21   0    1    x    x   CRA
//  FF02  FF22   1    0    x    1   PDB
//  FF02  FF22   1    0    x    0   DDB
//  FF03  FF23   1    1    x    x   CRB
//
// Control registers CRA and CRB control PIA operation.
//
// CRA1 and CRB1 control when IRQ is generated, if zero IRQ
// is set by high to low transition of CA1 and CB1. if one
// IRQ is set by low to high transition.
//
// CRA2 and CRB2 select data registor or direction registor. If set
// data registor is selected.
//
// CRA3, CRA4, CRA5, CRB3, CRB4, and CRB5 control the CA2 and CB2
// periperal control lines.  They determine if the control lines
// will be an interrupt input or and output control signal.  If
// CRA5 (CRB5) is low, CA2 (CB2) is an interrupt simular to CA1
// (CB1).  When CRA5 (CB5) is high CA2 (CB2) becomes an output
// signal the may be used fo control peripheral data transfers.
//
// CRA6, CRA7, CRB6, and CRB7 are interrupt flags bits set by
// transitions of the four interrupt and peritheral control
// lines when those lines are programmed to be inputs.
//
// In the CoCO PIA0 is used for keyboard, printer, horizontal and
// verticle sync interrupts, and joystick directrions.  PIA1 is used
// for cassette, printer, audio, and cartridge.
//
// PA0-PA6 are wired to keyboard rows 0-6 respectively
// PA0-PA3 are also wired to the joystick switches R1,L1,R2,L2
// PB0-PB7 are wired to keyboard cols 0-7 respectively
//
// Keyboard is accessed via PIA0 by setting FF00 for input,
// FF02 for output, then strobing the columns in FF02 and
// reading the key(s) from FF00. FF00 and FF02 are sometimes reversed.
// (eg RS tetris, which as of this date does not work)
//
// The PDA (PDB) register is accessed by setting CRA2 (CRB2) and
// writing to port FF00 (FF20).  The DDA (DDB) registers are
// set by clearing CRA3 and writing or writing to port FF00 (FF20)
//
// Bit 0 of $FF20 is cassette data input. Bit 1 is RS232 output.
// Bits 2-7 of $FF20 are the D/A converter, values here are compared
// to joystick readings.  The high bit of $FF00 will be set whenever
// the joystick values exceeds the D/A value.
//
// Bit 3 of $FF21 is cassette motor control
// Bit 0 of $FF22 is RS232 data input
// Bit 1 of $FF22 is sound output
// bit 3 of $FF23 is sound enable

// State flags for various periperals
static unsigned char LeftChannel=0,RightChannel=0;
static unsigned char Asample=0,Ssample=0,Csample=0;
static unsigned char CartInserted=0,CartAutoStart=1;
static unsigned char AddLF=0;

// Handles and data for printer
static HANDLE hPrintFile=INVALID_HANDLE_VALUE;
void CaptureBit(unsigned char);
static HANDLE hout=NULL;
void WritePrintMon(char *);
LRESULT CALLBACK PrintMon(HWND, UINT , WPARAM , LPARAM );
static BOOL MonState=FALSE;

//static unsigned char CoutSample=0;
//extern STRConfig CurrentConfig;
// Shift Row Col

// pia0_read is called when emulation reads address FF00-FF03
unsigned char pia0_read(unsigned char port)
{
	unsigned char dda,ddb;
	dda=(pia0[1] & 4);
	ddb=(pia0[3] & 4);

	switch (port)
	{
        // Read CRA, CRB
		case 1:
			return(pia0[port]);
		break;
		case 3:
			return(pia0[port]);
		break;

		// Read PDA, DDA, PDB, or PDA
		case 0:
			if (dda)
			{
				pia0[1]=(pia0[1] & 63);  // Clear interrupt bit
				return (vccKeyboardGetScan(pia0[2]));
			}
			else
				return(pia0_dd[port]);
		break;

		// Read PDB or DDB
		case 2:
			if (ddb)
			{
				pia0[3]=(pia0[3] & 63);   // Clear interrupt bit
			    return(pia0[port] & pia0_dd[port]);
			}
			else
				return(pia0_dd[port]);
		break;
	}
	return(0);
}

// pia1_read is called when emulation reads address FF20-FF23
unsigned char pia1_read(unsigned char port)
{
	static unsigned int Flag=0,Flag2=0;
	unsigned char dda,ddb;
	port-=0x20;
	dda=(pia1[1] & 4);
	ddb=(pia1[3] & 4);

	switch (port)
	{
		case 1:
		//	return(0);
		case 3:
			return(pia1[port]);
		break;

		case 2:
			if (ddb)
			{
				pia1[3]= (pia1[3] & 63);
				return(pia1[port] & pia1_dd[port]);
			}
			else
				return(pia1_dd[port]);
		break;

		case 0:
			if (dda)
			{
				pia1[1]=(pia1[1] & 63); //Cass In
				Flag=pia1[port] ;//& pia1_dd[port];
				return(Flag);
			}
			else
				return(pia1_dd[port]);
		break;
	}
	return(0);
}

// pia0_write is called when emulation writes address FF00-FF03
void pia0_write(unsigned char data,unsigned char port)
{
	unsigned char dda,ddb;
	dda=(pia0[1] & 4);
	ddb=(pia0[3] & 4);

	switch (port)
	{
	case 0:
		if (dda)
			pia0[port]=data;
		else
			pia0_dd[port]=data;
		return;
	case 2:
		if (ddb)
			pia0[port]=data;
		else
			pia0_dd[port]=data;
		return;
	break;

	case 1:
		pia0[port]= (data & 0x3F);
		return;
	break;

	case 3:
		pia0[port]= (data & 0x3F);
		return;
	break;
	}
	return;
}

// pia1_write is called when emulation writes address FF20-FF23
void pia1_write(unsigned char data,unsigned char port)
{
	unsigned char dda,ddb;
	static unsigned short LastSS=0;
	port-=0x20;

	dda=(pia1[1] & 4);
	ddb=(pia1[3] & 4);
	switch (port)
	{
	case 0:
		if (dda)
		{
			pia1[port]=data;
			CaptureBit((pia1[0]&2)>>1);
			if (GetMuxState()==0)
				if ((pia1[3] & 8)!=0)//==0 for cassette writes
					Asample	= (pia1[0] & 0xFC)>>1; //0 to 127
				else
					Csample = (pia1[0] & 0xFC);
		}
		else
			pia1_dd[port]=data;
		return;
	break;

	case 2: //FF22
		if (ddb)
		{
			pia1[port]=(data & pia1_dd[port]);
			SetGimeVdgMode2( (pia1[2] & 248) >>3);
			Ssample=(pia1[port] & 2)<<6;
		}
		else
			pia1_dd[port]=data;
		return;
	break;

	case 1:
		pia1[port]= (data & 0x3F);
		Motor((data & 8)>>3);
		return;
	break;

	case 3:
		pia1[port]= (data & 0x3F);
		return;
	break;
	}
	return;
}

unsigned char VDG_Mode(void)
{
	return( (pia1[2] & 248) >>3);
}


void irq_hs(int phase)	//63.5 uS
{

	switch (phase)
	{
	case FALLING:	//HS went High to low
		if ( (pia0[1] & 2) ) //IRQ on low to High transition
			return;
		pia0[1]=(pia0[1] | 128);
		if (pia0[1] & 1)
			CPUAssertInterupt(IRQ,1);
	break;

	case RISING:	//HS went Low to High
		if ( !(pia0[1] & 2) ) //IRQ  High to low transition
		return;
		pia0[1]=(pia0[1] | 128);
		if (pia0[1] & 1)
			CPUAssertInterupt(IRQ,1);
	break;

	case ANY:
		pia0[1]=(pia0[1] | 128);
		if (pia0[1] & 1)
			CPUAssertInterupt(IRQ,1);
	break;
	} //END switch

	return;
}

void irq_fs(int phase)	//60HZ Vertical sync pulse 16.667 mS
{
	if ((CartInserted==1) & (CartAutoStart==1))
		AssertCart();
	switch (phase)
	{
	case 0:	//FS went High to low
		if ( (pia0[3] & 2)==0 ) //IRQ on High to low transition
			pia0[3]=(pia0[3] | 128);
		if (pia0[3] & 1)
			CPUAssertInterupt(IRQ,1);

		return;
	break;

	case 1:	//FS went Low to High

		if ( (pia0[3] & 2) ) //IRQ  Low to High transition
		{
		pia0[3]=(pia0[3] | 128);
		if (pia0[3] & 1)
			CPUAssertInterupt(IRQ,1);
		}
		return;
	break;
	} //END switch

	return;
}

void AssertCart(void)
{
	pia1[3]=(pia1[3] | 128);
	if (pia1[3] & 1)
		CPUAssertInterupt(FIRQ,0);
	else
		CPUDeAssertInterupt(FIRQ); //Kludge but working
}

void PiaReset()
{
	// Clear the PIA registers
	for (uint8_t index=0; index<4; index++)
	{
		pia0[index]=0;
		pia1[index]=0;
		pia0_dd[index]=0;
		pia1_dd[index]=0;
	}
}

unsigned char GetMuxState(void)
{
	return ( ((pia0[1] & 8)>>3) + ((pia0[3] & 8) >>2));
}

unsigned char DACState(void)
{
	return (pia1[0]>>2);
}

void SetCart(unsigned char cart)
{
	CartInserted=cart;
	return;
}

unsigned int GetDACSample(void)
{
	static unsigned int RetVal=0;
	static unsigned short SampleLeft=0,SampleRight=0,PakSample=0;
	static unsigned short OutLeft=0,OutRight=0;
	static unsigned short LastLeft=0,LastRight=0;
	PakSample=PackAudioSample();
	SampleLeft=(PakSample>>8)+Asample+Ssample;
	SampleRight=(PakSample & 0xFF)+Asample+Ssample; //9 Bits each
	SampleLeft=SampleLeft<<6;	//Conver to 16 bit values
	SampleRight=SampleRight<<6;	//For Max volume
	if (SampleLeft==LastLeft)	//Simulate a slow high pass filter
	{
		if (OutLeft)
			OutLeft--;
	}
	else
	{
		OutLeft=SampleLeft;
		LastLeft=SampleLeft;
	}

	if (SampleRight==LastRight)
	{
		if (OutRight)
			OutRight--;
	}
	else
	{
		OutRight=SampleRight;
		LastRight=SampleRight;
	}

	RetVal=(OutLeft<<16)+(OutRight);
	return(RetVal);
}

unsigned char SetCartAutoStart(unsigned char Tmp)
{
	if (Tmp !=QUERY)
		CartAutoStart=Tmp;
	return(CartAutoStart);
}

unsigned char GetCasSample(void)
{
	return(Csample);
}

void SetCassetteSample(unsigned char Sample)
{
	pia1[0]=pia1[0] & 0xFE;
	if (Sample>0x7F)
		pia1[0]=pia1[0] | 1;
}

void CaptureBit(unsigned char Sample)
{
	unsigned long BytesMoved=0;
	static unsigned char BitMask=1,StartWait=1;
	static char Byte=0;
	if (hPrintFile==INVALID_HANDLE_VALUE)
		return;
	if (StartWait & Sample)	//Waiting for start bit
		return;
	if (StartWait)
	{
		StartWait=0;
		return;
	}
	if (Sample)
		Byte|=BitMask;
	BitMask=BitMask<<1;
	if (!BitMask)
	{
		BitMask=1;
		StartWait=1;
		WriteFile(hPrintFile,&Byte,1,&BytesMoved,NULL);
		if (MonState)
			WritePrintMon(&Byte);
		if ((Byte==0x0D) & AddLF)
		{
			Byte=0x0A;
			WriteFile(hPrintFile,&Byte,1,&BytesMoved,NULL);
		}
		Byte=0;
	}
	return;
}

int OpenPrintFile(char *FileName)
{
	hPrintFile=CreateFile( FileName,GENERIC_READ | GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
	if (hPrintFile==INVALID_HANDLE_VALUE)
		return(0);
	return(1);
}

void ClosePrintFile(void)
{
	CloseHandle(hPrintFile);
	hPrintFile=INVALID_HANDLE_VALUE;
	FreeConsole();
	hout=NULL;
	return;
}

void SetSerialParams(unsigned char TextMode)
{
	AddLF=TextMode;
	return;
}

void SetMonState(BOOL State)
{
	if (MonState & !State)
	{
		FreeConsole();
		hout=NULL;
	}
	MonState=State;
	return;
}
void WritePrintMon(char *Data)
{
	unsigned long dummy;
	if (hout==NULL)
	{
		AllocConsole();
		hout=GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTitle("Printer Monitor");
	}
	WriteConsole(hout,Data,1,&dummy,0);
	if (Data[0]==0x0D)
	{
		Data[0]=0x0A;
		WriteConsole(hout,Data,1,&dummy,0);
	}
}

