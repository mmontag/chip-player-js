#include <stdlib.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "regconfig.h"


#define TOTAL_BYTES_READ    1024
#define OFFSET_BYTES 1024

static BOOL createRegistryKey(HKEY hKeyParent, const PWCHAR subkey)
{
    DWORD dwDisposition; //It verify new key is created or open existing key
    HKEY  hKey;
    DWORD ret;

    ret = RegCreateKeyExW(hKeyParent, subkey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);
    if(ret != ERROR_SUCCESS)
    {
        return FALSE;
    }

    RegCloseKey(hKey); //close the key
    return TRUE;
}

static BOOL writeStringToRegistry(HKEY hKeyParent, const PWCHAR subkey, const PWCHAR valueName, PWCHAR strData)
{
    DWORD ret;
    HKEY hKey;

    //Check if the registry exists
    ret = RegOpenKeyExW(hKeyParent, subkey, 0, KEY_WRITE, &hKey);
    if(ret == ERROR_SUCCESS)
    {
        ret = RegSetValueExW(hKey, valueName, 0, REG_SZ, (LPBYTE)(strData), wcslen(strData) * sizeof(wchar_t));
        if(ret != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return FALSE;
        }

        RegCloseKey(hKey);
        return TRUE;
    }
    return FALSE;
}

static BOOL readStringFromRegistry(HKEY hKeyParent, const PWCHAR subkey, const PWCHAR valueName, PWCHAR *readData)
{
    HKEY hKey;
    DWORD len = TOTAL_BYTES_READ;
    DWORD readDataLen = len;
    PWCHAR readBuffer = (PWCHAR )malloc(sizeof(PWCHAR)* len);

    if (readBuffer == NULL)
        return FALSE;

    //Check if the registry exists
    DWORD ret = RegOpenKeyExW(hKeyParent, subkey, 0, KEY_READ, &hKey);

    if(ret == ERROR_SUCCESS)
    {
        ret = RegQueryValueExW(hKey, valueName, NULL, NULL, (BYTE*)readBuffer, &readDataLen);

        while(ret == ERROR_MORE_DATA)
        {
            // Get a buffer that is big enough.
            len += OFFSET_BYTES;
            readBuffer = (PWCHAR)realloc(readBuffer, len);
            readDataLen = len;
            ret = RegQueryValueExW(hKey, valueName, NULL, NULL, (BYTE*)readBuffer, &readDataLen);
        }
        if (ret != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return FALSE;
        }

        *readData = readBuffer;
        RegCloseKey(hKey);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static BOOL readIntFromRegistry(HKEY hKeyParent, const PWCHAR subkey, const PWCHAR valueName, int *readData)
{
    WCHAR *buf = NULL;
    BOOL ret;
    ret = readStringFromRegistry(hKeyParent, subkey, valueName, &buf);
    if(ret && readData)
    {
        *readData = _wtoi(buf);
    }
    if(buf)
        free(buf);
    return ret;
}

static BOOL writeIntToRegistry(HKEY hKeyParent, const PWCHAR subkey, const PWCHAR valueName, int intData)
{
    WCHAR buf[20];
    BOOL ret;

    ZeroMemory(buf, 20);
    _snwprintf(buf, 20, L"%d", intData);

    ret = writeStringToRegistry(hKeyParent, subkey, valueName, buf);
    return ret;
}




void setupDefault(DriverSettings *setup)
{
    setup->useExternalBank = 0;
    setup->bankId = 68;
    ZeroMemory(setup->bankPath, MAX_PATH * sizeof(WCHAR));
    setup->emulatorId = 0;

    setup->flagDeepTremolo = BST_INDETERMINATE;
    setup->flagDeepVibrato = BST_INDETERMINATE;

    setup->flagSoftPanning = BST_CHECKED;
    setup->flagScaleModulators = BST_UNCHECKED;
    setup->flagFullBrightness = BST_UNCHECKED;

    setup->volumeModel = 0;
    setup->numChips = 4;
    setup->num4ops = -1;
}



static const PWCHAR s_regPath       = L"SOFTWARE\\Wohlstand\\libADLMIDI";

void loadSetup(DriverSettings *setup)
{
    int iVal;
    WCHAR *sVal = NULL;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"useExternalBank", &iVal))
        setup->useExternalBank = iVal;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"bankId", &iVal))
        setup->bankId = iVal;

    if(readStringFromRegistry(HKEY_CURRENT_USER, s_regPath, L"bankPath", &sVal))
        wcsncpy(setup->bankPath, sVal, MAX_PATH);
    if(sVal)
        free(sVal);
    sVal = NULL;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"emulatorId", &iVal))
        setup->emulatorId = iVal;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"flagDeepTremolo", &iVal))
        setup->flagDeepTremolo = iVal;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"flagDeepVibrato", &iVal))
        setup->flagDeepVibrato = iVal;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"flagSoftPanning", &iVal))
        setup->flagSoftPanning = iVal;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"flagScaleModulators", &iVal))
        setup->flagScaleModulators = iVal;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"flagFullBrightness", &iVal))
        setup->flagFullBrightness = iVal;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"volumeModel", &iVal))
        setup->volumeModel = iVal;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"numChips", &iVal))
        setup->numChips = iVal;

    if(readIntFromRegistry(HKEY_CURRENT_USER, s_regPath, L"num4ops", &iVal))
        setup->num4ops = iVal;
}

void saveSetup(DriverSettings *setup)
{
    createRegistryKey(HKEY_CURRENT_USER, s_regPath);
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"useExternalBank", setup->useExternalBank);
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"bankId", setup->bankId);
    if(setup->bankPath[0] != L'\0')
        writeStringToRegistry(HKEY_CURRENT_USER, s_regPath, L"bankPath", setup->bankPath);
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"emulatorId", setup->emulatorId);

    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"flagDeepTremolo", setup->flagDeepTremolo);
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"flagDeepVibrato", setup->flagDeepVibrato);
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"flagSoftPanning", setup->flagSoftPanning);
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"flagScaleModulators", setup->flagScaleModulators);
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"flagFullBrightness", setup->flagFullBrightness);

    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"volumeModel", setup->volumeModel);
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"numChips", setup->numChips);
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPath, L"num4ops", setup->num4ops);
}


static const PWCHAR s_regPathNotify = L"SOFTWARE\\Wohlstand\\libADLMIDI\\notify";

void sendSignal(int sig)
{
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPathNotify, L"command", sig);
}


#ifdef ENABLE_REG_SERVER

static HKEY   hKey = 0;
static HANDLE hEvent = 0;

void openSignalListener()
{
    LONG  errorcode;
    DWORD  dwFilter = REG_NOTIFY_CHANGE_NAME |
                      REG_NOTIFY_CHANGE_ATTRIBUTES |
                      REG_NOTIFY_CHANGE_LAST_SET |
                      REG_NOTIFY_CHANGE_SECURITY;

    createRegistryKey(HKEY_CURRENT_USER, s_regPathNotify);
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPathNotify, L"command", 0);

    errorcode = RegOpenKeyExW(HKEY_CURRENT_USER, s_regPathNotify, 0, KEY_NOTIFY, &hKey);
    if(errorcode != ERROR_SUCCESS)
        return;

    hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if(hEvent == NULL)
    {
        RegCloseKey(hKey);
        hKey = 0;
        return;
    }

    errorcode = RegNotifyChangeKeyValue(hKey,
                                        FALSE,
                                        dwFilter,
                                        hEvent,
                                        TRUE);
    if(errorcode != ERROR_SUCCESS)
    {
        CloseHandle(hEvent);
        hEvent = 0;
        RegCloseKey(hKey);
        hKey = 0;
        return;
    }
}

int hasReloadSetupSignal()
{
    DWORD ret;
    int cmd;

    if(hEvent == 0)
        return 0;

    ret = WaitForSingleObject(hEvent, 0);

    if(ret == WAIT_OBJECT_0)
    {
        readIntFromRegistry(HKEY_CURRENT_USER, s_regPathNotify, L"command", &cmd);
        return cmd;
    }

    return 0;
}

void resetSignal()
{
    writeIntToRegistry(HKEY_CURRENT_USER, s_regPathNotify, L"command", 0);
}

void closeSignalListener()
{
    if(hKey)
        RegCloseKey(hKey);
    hKey = 0;

    if(hEvent)
        CloseHandle(hEvent);
    hEvent = 0;
}

#endif
