#pragma once

#ifndef SETUP_DIALOG_HHHH
#define SETUP_DIALOG_HHHH

#include <windef.h>

extern BOOL initAdlSetupBox(HINSTANCE hModule, HWND hwnd);
extern BOOL runAdlSetupBox(HINSTANCE hModule, HWND hwnd);
extern BOOL cleanUpAdlSetupBox(HINSTANCE hModule, HWND hwnd);

#endif
