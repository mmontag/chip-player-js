#include "targetver.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cpl.h>

#include "resource.h"

#include "config_dialog.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow)
{
    initAdlSetupBox(hInstance, NULL);
    runAdlSetupBox(hInstance, NULL);
    return 0;

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);
}
