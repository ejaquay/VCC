
#include "acia.h"

//------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------

// Transfer points for menu callback and cpu assert interrupt
typedef void (*DYNAMICMENUCALLBACK)( char *,int, int);
typedef void (*ASSERTINTERUPT) (unsigned char,unsigned char);

void (*DynamicMenuCallback)(char *,int,int)=NULL;
void BuildDynaMenu(void);

LRESULT CALLBACK Config(HWND, UINT, WPARAM, LPARAM);
void LoadConfig(void);
void SaveConfig(void);

//------------------------------------------------------------------------
// Globals 
//------------------------------------------------------------------------
static HINSTANCE g_hDLL = NULL;      // DLL handle
static char IniFile[MAX_PATH];       // Ini file name
static char IniSect[MAX_LOADSTRING]; // Ini file section

//------------------------------------------------------------------------
//  DLL Entry  
//------------------------------------------------------------------------
BOOL APIENTRY 
DllMain(HINSTANCE hinst, DWORD reason, LPVOID foo)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_hDLL = hinst;
        sc6551_initialized = 0;
    } else if (reason == DLL_PROCESS_DETACH) {
        sc6551_close();
    }
    return TRUE;
}

//-----------------------------------------------------------------------
//  Register the DLL and add entry to dynamic menu
//-----------------------------------------------------------------------
__declspec(dllexport) void
ModuleName(char *ModName,char *CatNumber,DYNAMICMENUCALLBACK Temp)
{
	LoadString(g_hDLL,IDS_MODULE_NAME,ModName,MAX_LOADSTRING);
	LoadString(g_hDLL,IDS_CATNUMBER,CatNumber,MAX_LOADSTRING);		
	DynamicMenuCallback = Temp;
    strcpy(IniSect,ModName); // Use module name for ini file section
	if (DynamicMenuCallback != NULL) BuildDynaMenu();
	return ;
}

//-----------------------------------------------------------------------
// Export write to port
//-----------------------------------------------------------------------
__declspec(dllexport) void
PackPortWrite(unsigned char Port,unsigned char Data)
{
    sc6551_write(Data,Port);
    return;
}

//-----------------------------------------------------------------------
// Export read from port
//-----------------------------------------------------------------------
__declspec(dllexport) unsigned char 
PackPortRead(unsigned char Port)
{
    return sc6551_read(Port);
}

//-----------------------------------------------------------------------
// Capture the transfer point for CPU assert interrupt
//-----------------------------------------------------------------------
__declspec(dllexport) void AssertInterupt(ASSERTINTERUPT Dummy)
{
	AssertInt=Dummy;
	return;
}

//-----------------------------------------------------------------------
// Return acia status for VCC status line
//-----------------------------------------------------------------------
__declspec(dllexport) void ModuleStatus(char *status)
{
    if (sc6551_initialized) {
        if (AciaComType == 0) {
            if (ConsoleLineInput) strcpy(status,"LineMode");
            else strcpy(status,"Console"); 
        } else {
            strcpy(status,"AciaUnknown");
        }
    } else {
        strcpy(status,"Acia");
    }
    return;
}

//-----------------------------------------------------------------------
//  Start the config dialog 
//-----------------------------------------------------------------------
__declspec(dllexport) void ModuleConfig(unsigned char MenuID)
{
    DialogBox(g_hDLL, (LPCTSTR) IDD_PROPPAGE, NULL, (DLGPROC) Config);
	return;
}

//-----------------------------------------------------------------------
// Capture the VCC ini file path and load settings saved there
//-----------------------------------------------------------------------
__declspec(dllexport) void SetIniPath (char *IniFilePath)
{
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
    AciaComType = GetPrivateProfileInt(IniSect,"AciaComType",0,IniFile);
    AciaTcpPort = GetPrivateProfileInt(IniSect,"AciaTcpPort",1024,IniFile);
    AciaComPort = GetPrivateProfileInt(IniSect,"AciaComPort",1,IniFile);
}

//-----------------------------------------------------------------------
//  Save config to ini file
//----------------------------------------------------------------------
void SaveConfig(void)
{
    char txt[16];
    sprintf(txt,"%d",AciaComType);
    WritePrivateProfileString(IniSect,"AciaComType",txt,IniFile);
    sprintf(txt,"%d",AciaTcpPort);
    WritePrivateProfileString(IniSect,"AciaTcpPort",txt,IniFile);
    sprintf(txt,"%d",AciaComPort);
    WritePrivateProfileString(IniSect,"AciaComPort",txt,IniFile);
}

//-----------------------------------------------------------------------
//  Config dialog. Allows user to select Console, TCP, COMx, port
//-----------------------------------------------------------------------
LRESULT CALLBACK Config(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch (msg) {

	case WM_INITDIALOG:

        // Center dialog on screen 
	    RECT rDlg, rScr; 
		GetWindowRect(hDlg, &rDlg); 
        GetWindowRect(GetDesktopWindow(), &rScr); 
        DWORD x = (rScr.right - rScr.left - rDlg.right + rDlg.left)/2;
        DWORD y = (rScr.bottom - rScr.top - rDlg.bottom + rDlg.top)/2;
        SetWindowPos(hDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);

        // Fill in current values
        SetDlgItemInt(hDlg,IDC_TYPE,AciaComType,FALSE);
        switch (AciaComType) {
        case 0: // console
            SetDlgItemInt(hDlg,IDC_PORT,0,FALSE);
            break;
        case 1: // tcpip
            SetDlgItemInt(hDlg,IDC_PORT,AciaTcpPort,FALSE);
            break;
        case 2: // COMx
            SetDlgItemInt(hDlg,IDC_PORT,AciaComPort,FALSE);
            break;
        }
		return TRUE; 
		break;

	case WM_COMMAND:
	    switch (LOWORD(wParam)) {
		case IDOK:
            int type = GetDlgItemInt(hDlg,IDC_TYPE,NULL,FALSE);
            int port = GetDlgItemInt(hDlg,IDC_PORT,NULL,FALSE);
            switch (type) {
            case 0: // console
                break;
            case 1: // tcpip
                if ((port < 1024) || (port > 65536)) {
                    MessageBox(hDlg,"TCP Port must be 1024 thru 65536","Error",
                                MB_OK | MB_ICONEXCLAMATION);
                    return TRUE;
                }
                AciaTcpPort = port;
                break;
            case 2: // COMx
                if ((port < 1) || (port > 10)) {
                    MessageBox(hDlg,"COM# must be 1 thru 10","Error",
                                MB_OK | MB_ICONEXCLAMATION);
                    return TRUE;
                }
                AciaComPort = port;
                break;
            default:
                MessageBox(hDlg,"Type must be 0, 1, or 2","Error",
                           MB_OK | MB_ICONEXCLAMATION);
                return TRUE;
            }
            AciaComType = type;
            SaveConfig();
			EndDialog(hDlg, LOWORD(wParam));
			break;

	    case IDHELP:
		    break;

	    case IDCANCEL:
			EndDialog(hDlg, LOWORD(wParam));
			break;
	    }
	    return TRUE;
	}
    return FALSE;
}