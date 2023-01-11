//------------------------------------------------------------------
// Copyright E J Jaquay 2022
//
// This file is part of VCC (Virtual Color Computer).
//
// VCC (Virtual Color Computer) is free software: you can redistribute it
// and/or modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// VCC (Virtual Color Computer) is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// See the GNU General Public License for more details.  You should have
// received a copy of the GNU General Public License  along with VCC 
// (Virtual Color Computer). If not see <http://www.gnu.org/licenses/>.
//
//------------------------------------------------------------------

#include "acia.h"
#include "sc6551.h"
#include "logger.h"

//------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------

// Transfer points for menu callback and cpu assert interrupt
typedef void (*DYNAMICMENUCALLBACK)( char *,int, int);
typedef void (*ASSERTINTERUPT) (unsigned char,unsigned char);

void (*DynamicMenuCallback)(char *,int,int)=NULL;
void BuildDynaMenu(void);

LRESULT CALLBACK ConfigDlg(HWND, UINT, WPARAM, LPARAM);
void LoadConfig(void);
void SaveConfig(void);

//------------------------------------------------------------------------
// Globals
//------------------------------------------------------------------------
static HINSTANCE g_hDLL = NULL;      // DLL handle
static char IniFile[MAX_PATH];       // Ini file name
static char IniSect[MAX_LOADSTRING]; // Ini file section
char *IOModeTxt[] = {"RW","R","W"};
char *ComTypeTxt[] = {"CONS","FILE","TCP","COM"};

//------------------------------------------------------------------------
//  DLL Entry point
//------------------------------------------------------------------------
BOOL APIENTRY
DllMain(HINSTANCE hinst, DWORD reason, LPVOID foo)
{

    if (reason == DLL_PROCESS_ATTACH) {
        g_hDLL = hinst;
    } else if (reason == DLL_PROCESS_DETACH) {
        sc6551_close();
        AciaStat[0]='\0';
    }
    return TRUE;
}

//-----------------------------------------------------------------------
//  Dll export register the DLL and build entry on dynamic menu
//-----------------------------------------------------------------------
__declspec(dllexport) void
ModuleName(char *ModName,char *CatNumber,DYNAMICMENUCALLBACK Temp)
{
PrintLogF("Acia DLL register\n");
    LoadString(g_hDLL,IDS_MODULE_NAME,ModName,MAX_LOADSTRING);
    LoadString(g_hDLL,IDS_CATNUMBER,CatNumber,MAX_LOADSTRING);
    DynamicMenuCallback = Temp;
    strcpy(IniSect,ModName);   // Use module name for ini file section
    if (DynamicMenuCallback != NULL) BuildDynaMenu();
    return ;
}

//-----------------------------------------------------------------------
// Dll export write to port
//-----------------------------------------------------------------------
__declspec(dllexport) void
PackPortWrite(unsigned char Port,unsigned char Data)
{
    sc6551_write(Data,Port);
    return;
}

//-----------------------------------------------------------------------
// Dll export read from port
//-----------------------------------------------------------------------
__declspec(dllexport) unsigned char
PackPortRead(unsigned char Port)
{
    return sc6551_read(Port);
}

//-----------------------------------------------------------------------
// Dll export Heartbeat (Called right after HSYNC) 
//-----------------------------------------------------------------------
__declspec(dllexport) void HeartBeat(void)
{
    sc6551_ping();
	return;
}

//-----------------------------------------------------------------------
// Dll export supply transfer point for interrupt
//-----------------------------------------------------------------------
__declspec(dllexport) void AssertInterupt(ASSERTINTERUPT Dummy)
{
    AssertInt=Dummy;
    return;
}

//-----------------------------------------------------------------------
// Dll export return module status for VCC status line
//-----------------------------------------------------------------------
__declspec(dllexport) void ModuleStatus(char *status)
{
    strncpy (status,AciaStat,16);
    status[16]='\n';
    return;
}

//-----------------------------------------------------------------------
//  Dll export run config dialog
//-----------------------------------------------------------------------
__declspec(dllexport) void ModuleConfig(unsigned char MenuID)
{
    DialogBox(g_hDLL, (LPCTSTR) IDD_PROPPAGE, NULL, (DLGPROC) ConfigDlg);
    return;
}

//-----------------------------------------------------------------------
// Dll export VCC ini file path and load settings
//-----------------------------------------------------------------------
__declspec(dllexport) void SetIniPath (char *IniFilePath)
{
PrintLogF("Acia load config from %s\n",IniFilePath);
    strcpy(IniFile,IniFilePath);
    LoadConfig();
    return;
}

//-----------------------------------------------------------------------
//  Add config option to Cartridge menu
//----------------------------------------------------------------------
void BuildDynaMenu(void)
{
    DynamicMenuCallback("",0,0);     // begin
    DynamicMenuCallback("",6000,0);  // seperator
    DynamicMenuCallback("ACIA Config",5016,STANDALONE); // Config
    DynamicMenuCallback("",1,0);     // end
}

//-----------------------------------------------------------------------
//  Load saved config from ini file
//----------------------------------------------------------------------
void LoadConfig(void)
{
    AciaComType=GetPrivateProfileInt("Acia","AciaComType",COM_CONSOLE,IniFile);
    AciaFileMode=GetPrivateProfileInt("Acia","AciaFileMode",FILE_NONE,IniFile);
    AciaTcpPort=GetPrivateProfileInt("Acia","AciaTcpPort",1024,IniFile);
    AciaComPort=GetPrivateProfileInt("Acia","AciaComPort",1,IniFile);
    AciaTextMode=GetPrivateProfileInt("Acia","AciaTextMode",0,IniFile);
    GetPrivateProfileString("Acia","AciaFilePath","AciaFile.txt",
							AciaFilePath, MAX_PATH,IniFile);

    // String for Vcc status line
    sprintf(AciaStat,"Acia %s %s",ComTypeTxt[AciaComType],IOModeTxt[AciaFileMode]);
}

//-----------------------------------------------------------------------
//  Save config to ini file
//----------------------------------------------------------------------
void SaveConfig(void)
{

PrintLogF("SaveConfig ty:%d cp:%d tp:%d tm:%d fi:%s\n",
			AciaComType,AciaComPort,AciaTcpPort,
            AciaTextMode,AciaFilePath);

    char txt[16];
    sprintf(txt,"%d",AciaComType);
    WritePrivateProfileString("Acia","AciaComType",txt,IniFile);
    sprintf(txt,"%d",AciaFileMode);
    WritePrivateProfileString("Acia","AciaFileMode",txt,IniFile);
    sprintf(txt,"%d",AciaTcpPort);
    WritePrivateProfileString("Acia","AciaTcpPort",txt,IniFile);
    sprintf(txt,"%d",AciaComPort);
    WritePrivateProfileString("Acia","AciaComPort",txt,IniFile);
    sprintf(txt,"%d",AciaTextMode);
    WritePrivateProfileString("Acia","AciaTextMode",txt,IniFile);
    WritePrivateProfileString("Acia","AciaFilePath",AciaFilePath,IniFile);
}

//-----------------------------------------------------------------------
// Config dialog.
// Radio Buttons: (I/O Type) :
//   IDC_T_CONS    I/O is to windows console 
//   IDC_T_FILE_R  Input from file (Output to bit bucket)
//   IDC_T_FILE_W  Output to file, Input returns null
//   IDC_T_TCP     I/O to TCPIP port
//   IDC_T_COM     I/O to windows COM port
// Text Boxes
//   IDC_PORT      Port for TCP and COM Number
//   IDC_FILE      File name for FILE_R and FILE_W
// Check Box 
//   IDC_TEXTMODE  Translate CR <> CRLF if checked
//-----------------------------------------------------------------------

LRESULT CALLBACK ConfigDlg(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam)
{
    int button;

    HANDLE hCtl;
    switch (msg) {

    case WM_INITDIALOG:
//LoadConfig();
        SetWindowPos(hDlg, HWND_TOP, 10, 10, 0, 0, SWP_NOSIZE);

        // Set Radio Button as per com type
        switch (AciaComType) {

        case COM_FILE:
            if (AciaFileMode == FILE_READ) {
                button = IDC_T_FILE_R;
            } else {
                // If file mode default to write
                AciaFileMode = FILE_WRITE;
                button = IDC_T_FILE_W;
            }
            SetDlgItemText(hDlg,IDC_FILE,AciaFilePath);
            SetDlgItemText(hDlg,IDC_PORT,"");
            break;

        case COM_TCPIP:
            button = IDC_T_TCP;
            SetDlgItemText(hDlg,IDC_FILE,"");
            SetDlgItemInt(hDlg,IDC_PORT,AciaTcpPort,FALSE);
            break;

        case COM_WINCOM:
            button = IDC_T_COM;
            SetDlgItemText(hDlg,IDC_FILE,"");
            SetDlgItemInt(hDlg,IDC_PORT,AciaComPort,FALSE);
            break;

        case COM_CONSOLE:
        default:
            button = IDC_T_CONS;
            AciaComType = COM_CONSOLE;
            SetDlgItemText(hDlg,IDC_FILE,"");
            SetDlgItemText(hDlg,IDC_PORT,"");
            break;
        }
        CheckRadioButton(hDlg,IDC_T_CONS,IDC_T_COM,button);

        // Text mode check box
        hCtl = GetDlgItem(hDlg,IDC_TXTMODE);
        SendMessage(hCtl,BM_SETCHECK,AciaTextMode,0);

        // String for Vcc status line
        sprintf(AciaStat,"Acia %s %s",
                         ComTypeTxt[AciaComType],IOModeTxt[AciaFileMode]);

PrintLogF("INITDIALOG ty:%d cp:%d tp:%d tm:%d fm:%d fi:%s\n",
			AciaComType,AciaComPort,AciaTcpPort,
            AciaTextMode,AciaFileMode,AciaFilePath);

        return TRUE;
        break;

    case WM_COMMAND:
        int port;

        button = LOWORD(wParam);
        switch (button) {
        case IDC_T_CONS:
            AciaComType  = COM_CONSOLE;
            AciaFileMode = FILE_NONE;
            SetDlgItemText(hDlg,IDC_FILE,"");
            SetDlgItemText(hDlg,IDC_PORT,"");
            break;

        case IDC_T_FILE_R:
            AciaComType  = COM_FILE;
            AciaFileMode = FILE_READ;
            SetDlgItemText(hDlg,IDC_FILE,AciaFilePath);
            SetDlgItemText(hDlg,IDC_PORT,"");
            break;

        case IDC_T_FILE_W:
            AciaComType  = COM_FILE;
            AciaFileMode = FILE_WRITE;
            SetDlgItemText(hDlg,IDC_FILE,AciaFilePath);
            SetDlgItemText(hDlg,IDC_PORT,"");
            break;

        case IDC_T_TCP: // tcpip
            AciaComType = COM_TCPIP;
            AciaFileMode = FILE_NONE;
            SetDlgItemText(hDlg,IDC_FILE,"");
            SetDlgItemInt(hDlg,IDC_PORT,AciaTcpPort,FALSE);
            break;

        case IDC_T_COM: // COMx
            AciaComType = COM_WINCOM;
            AciaFileMode = FILE_NONE;
            SetDlgItemText(hDlg,IDC_FILE,"");
            SetDlgItemInt(hDlg,IDC_PORT,AciaComPort,FALSE);
            break;

        case IDOK:
        case IDAPPLY:
            GetDlgItemText(hDlg,IDC_FILE,AciaFilePath,MAX_PATH);

            // Text mode check box
            hCtl = GetDlgItem(hDlg,IDC_TXTMODE);
            AciaTextMode = SendMessage(hCtl,BM_GETCHECK,0,0);

            // Validate parameters
            switch (AciaComType) {
            case COM_CONSOLE:
            case COM_FILE:
                break;
            case COM_TCPIP:
                port = GetDlgItemInt(hDlg,IDC_PORT,NULL,0);
                if ((port < 1024) || (port > 65536)) {
                    MessageBox(hDlg,"TCP Port must be 1024 thru 65536",
                                    "Error", MB_OK|MB_ICONEXCLAMATION);
                    return TRUE;
                }
			    AciaTcpPort = port;
                break;
            case COM_WINCOM:
                port = GetDlgItemInt(hDlg,IDC_PORT,NULL,0);
                if ((port < 1) || (port > 10)) {
                    MessageBox(hDlg,"COM number must be 1 thru 10",
                                    "Error", MB_OK|MB_ICONEXCLAMATION);
                    return TRUE;
                }
                break;
			    AciaComPort = port;
            }
            // String for Vcc status line
            sprintf(AciaStat,"Acia %s %s",
                              ComTypeTxt[AciaComType],IOModeTxt[AciaFileMode]);

            SaveConfig();

            if (button==IDOK) EndDialog(hDlg,button);
            break;

        case IDHELP:
            break;

        case IDCANCEL:
            EndDialog(hDlg, button);
            break;
        }
        return TRUE;
    }
    return FALSE;
}

//----------------------------------------------------------------
// Dispatch I/0 to communication type used.
//	 COM_CONSOLE 0
//	 COM_FILE    1
//	 COM_TCPIP   2
//	 COM_WINCOM  3
//-----------------------------------------------------------------
int com_open() {
    switch (AciaComType) {
    case COM_CONSOLE:
        return console_open();
    case COM_FILE:
        return file_open();
    }
    return 0;
}

void com_close() {
    switch (AciaComType) {
    case COM_CONSOLE:
        console_close();
        break;
    case COM_FILE:
		file_close();
        break;
    }
}

void com_set(int item, int val) {
    switch (AciaComType) {
    case COM_CONSOLE: // Console
        console_set(item,val);
        break;
    }
}

// com_write is assumed to block until some data is written
int com_write(char * buf, int len) {
    switch (AciaComType) {
    case COM_CONSOLE:
        return console_write(buf,len);
    case COM_FILE:
        return file_write(buf,len);
    }
    return 0;
}

// com_read is assumed to block until some data is read
int com_read(char * buf,int len) {  // returns bytes read
    switch (AciaComType) {
    case COM_CONSOLE:
        return console_read(buf,len);
    case COM_FILE:
        return file_read(buf,len);
    }
    return 0;
}