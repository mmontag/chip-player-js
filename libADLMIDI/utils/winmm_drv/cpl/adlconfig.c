#include "targetver.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cpl.h>

#include "resource.h"

#include "config_dialog.h"

static HANDLE  hModule = NULL;

BOOL WINAPI DllMain(
    PVOID hmod,
    ULONG ulReason,
    PCONTEXT pctx OPTIONAL
)
{
    if(ulReason != DLL_PROCESS_ATTACH)
    {
        return TRUE;
    }
    else
    {
        hModule = hmod;
    }

    return TRUE;

    UNREFERENCED_PARAMETER(pctx);
}

LONG APIENTRY CPlApplet(
    HWND hwndCPL,       // handle of Control Panel window
    UINT uMsg,          // message
    LONG_PTR lParam1,   // first message parameter
    LONG_PTR lParam2    // second message parameter
)
{
    LPCPLINFO lpCPlInfo;
    LPNEWCPLINFO lpNewCPlInfo;
    LONG retCode = 0;

    switch (uMsg)
    {
    // first message, sent once
    case CPL_INIT:
        initAdlSetupBox(hModule, hwndCPL);
        return TRUE;

    // second message, sent once
    case CPL_GETCOUNT:
        return 1L;

    // third message, sent once per app
    case CPL_INQUIRE:
        lpCPlInfo = (LPCPLINFO)lParam2;
        lpCPlInfo->idIcon = IDI_ICON1;
        lpCPlInfo->idName = IDC_DRIVERNAME;
        lpCPlInfo->idInfo = IDC_DRIVERDESC;
        lpCPlInfo->lData = 0L;
        break;

    // third message, sent once per app
    case CPL_NEWINQUIRE:
        lpNewCPlInfo = (LPNEWCPLINFO)lParam2;
        lpNewCPlInfo->dwSize = (DWORD) sizeof(NEWCPLINFO);
        lpNewCPlInfo->dwFlags = 0;
        lpNewCPlInfo->dwHelpContext = 0;
        lpNewCPlInfo->lData = 0;
        lpNewCPlInfo->hIcon = LoadIconW(hModule, (LPCTSTR)MAKEINTRESOURCEW(IDI_ICON1));
        lpNewCPlInfo->szHelpFile[0] = '\0';

        LoadStringW(hModule, IDC_DRIVERNAME, lpNewCPlInfo->szName, 32);
        LoadStringW(hModule, IDC_DRIVERDESC, lpNewCPlInfo->szInfo, 64);
        break;

    // application icon selected
    case CPL_SELECT:
        break;

    // application icon double-clicked
    case CPL_DBLCLK:
        runAdlSetupBox(hModule, hwndCPL);
        break;

    case CPL_STOP:
        break;

    case CPL_EXIT:
        cleanUpAdlSetupBox(hModule, hwndCPL);
        break;

    default:
        break;
    }

    return retCode;

    UNREFERENCED_PARAMETER(lParam1);
}
