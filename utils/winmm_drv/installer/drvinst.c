/* Copyright (C) 2011, 2012 Sergey V. Mikayev
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


const char OPL3EMU_DRIVER_NAME[] = "adlmididrv.dll";
const char OPL3EMU_CPLAPPLET_NAME[] = "libadlconfig.cpl";
const char OPL3EMU_SETUPTOOL_NAME[] = "adlmidiconfigtool.exe";

const char WDM_DRIVER_NAME[] = "wdmaud.drv";
const char SYSTEM_DIR_NAME[] = "SYSTEM32";
const char SYSTEM_ROOT_ENV_NAME[] = "SYSTEMROOT";
const char INSTALL_COMMAND[] = "install";
const char UNINSTALL_COMMAND[] = "uninstall";
const char DRIVERS_REGISTRY_KEY[] = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Drivers32";
const char PATH_SEPARATOR[] = "\\";

const char SUCCESSFULLY_INSTALLED_MSG[] = "libADLMIDI synth MIDI Driver successfully installed";
const char SUCCESSFULLY_UPDATED_MSG[] = "libADLMIDI synth MIDI Driver successfully updated";
const char SUCCESSFULLY_UNINSTALLED_MSG[] = "libADLMIDI synth MIDI Driver successfully uninstalled";
const char USAGE_MSG[] = "Usage:\n  drvsetup install - install driver\n  drvsetup uninstall - uninstall driver";

const char CANNOT_OPEN_REGISTRY_ERR[] = "Cannot open registry key";
const char CANNOT_INSTALL_NO_PORTS_ERR[] = "Cannot install libADLMIDI synth MIDI driver:\n There is no MIDI ports available";
const char CANNOT_REGISTER_ERR[] = "Cannot register driver";
const char CANNOT_UNINSTALL_ERR[] = "Cannot uninstall libADLMIDI synth MIDI driver";
const char CANNOT_UNINSTALL_NOT_FOUND_ERR[] = "Cannot uninstall libADLMIDI synth MIDI driver:\n There is no driver registry entry found";
const char CANNOT_INSTALL_PATH_TOO_LONG_ERR[] = "libADLMIDI synth MIDI Driver cannot be installed:\n Installation path is too long";
const char CANNOT_INSTALL_FILE_COPY_ERR[] = "libADLMIDI synth MIDI Driver failed to install:\n File copying error";

const char INFORMATION_TITLE[] = "Information";
const char ERROR_TITLE[] = "Error";
const char REGISTRY_ERROR_TITLE[] = "Registry error";
const char FILE_ERROR_TITLE[] = "File error";

BOOL registerDriver(BOOL *installMode)
{
    char str[255];
    char drvName[] = "midi0";
    DWORD len, res;
    BOOL wdmEntryFound = FALSE;
    int freeEntry = -1;
    HKEY hReg;
    int i;

    if(RegOpenKeyA(HKEY_LOCAL_MACHINE, DRIVERS_REGISTRY_KEY, &hReg))
    {
        MessageBoxA(NULL, CANNOT_OPEN_REGISTRY_ERR, REGISTRY_ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }

    for(i = 0; i < 10; i++)
    {
        len = 255;
        if(i)
            drvName[4] = '0' + i;
        else
            drvName[4] = 0;
        res = RegQueryValueExA(hReg, drvName, NULL, NULL, (LPBYTE)str, &len);
        if(res != ERROR_SUCCESS)
        {
            if((freeEntry == -1) && (res == ERROR_FILE_NOT_FOUND))
                freeEntry = i;
            continue;
        }
        if(!_stricmp(str, OPL3EMU_DRIVER_NAME))
        {
            RegCloseKey(hReg);
            *installMode = FALSE;
            return TRUE;
        }
        if(freeEntry != -1) continue;
        if(strlen(str) == 0)
            freeEntry = i;
        else if(!_stricmp(str, WDM_DRIVER_NAME))
        {
            // Considering multiple WDM entries are just garbage, though one entry shouldn't be modified
            if(wdmEntryFound)
                freeEntry = i;
            else
                wdmEntryFound = TRUE;
        }
    }

    if(freeEntry == -1)
    {
        MessageBoxA(NULL, CANNOT_INSTALL_NO_PORTS_ERR, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        RegCloseKey(hReg);
        return FALSE;
    }

    if(freeEntry)
        drvName[4] = '0' + freeEntry;
    else
        drvName[4] = 0;

    res = RegSetValueExA(hReg, drvName, 0, REG_SZ, (LPBYTE)OPL3EMU_DRIVER_NAME, sizeof(OPL3EMU_DRIVER_NAME));
    if(res != ERROR_SUCCESS)
    {
        MessageBoxA(NULL, CANNOT_REGISTER_ERR, REGISTRY_ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        RegCloseKey(hReg);
        return FALSE;
    }

    RegCloseKey(hReg);

    return TRUE;
}

void unregisterDriver()
{
    char str[255];
    char drvName[] = "midi0";
    DWORD len, res;
    HKEY hReg;
    int i;

    if(RegOpenKeyA(HKEY_LOCAL_MACHINE, DRIVERS_REGISTRY_KEY, &hReg))
    {
        MessageBoxA(NULL, CANNOT_OPEN_REGISTRY_ERR, REGISTRY_ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return;
    }

    for(i = 0; i < 10; i++)
    {
        len = 255;
        if(i)
            drvName[4] = '0' + i;
        else
            drvName[4] = 0;
        res = RegQueryValueExA(hReg, drvName, NULL, NULL, (LPBYTE)str, &len);
        if(res != ERROR_SUCCESS)
            continue;
        if(!_stricmp(str, OPL3EMU_DRIVER_NAME))
        {
            res = RegDeleteValueA(hReg, drvName);
            if(res != ERROR_SUCCESS)
            {
                MessageBoxA(NULL, CANNOT_UNINSTALL_ERR, REGISTRY_ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
                RegCloseKey(hReg);
                return;
            }
            MessageBoxA(NULL, SUCCESSFULLY_UNINSTALLED_MSG, INFORMATION_TITLE, MB_OK | MB_ICONINFORMATION);
            RegCloseKey(hReg);
            return;
        }
    }

    MessageBoxA(NULL, CANNOT_UNINSTALL_NOT_FOUND_ERR, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
    RegCloseKey(hReg);
}

void constructSystemDirName(char *pathName)
{
    char sysRoot[MAX_PATH + 1];
    GetEnvironmentVariableA(SYSTEM_ROOT_ENV_NAME, sysRoot, MAX_PATH);
    strncpy(pathName, sysRoot, MAX_PATH + 1);
    strncat(pathName, PATH_SEPARATOR, MAX_PATH - strlen(pathName));
    strncat(pathName, SYSTEM_DIR_NAME, MAX_PATH - strlen(pathName));
    strncat(pathName, PATH_SEPARATOR, MAX_PATH - strlen(pathName));
}

void constructDriverPathName(char *pathName, const char*fileName)
{
    constructSystemDirName(pathName);
    strncat(pathName, fileName, MAX_PATH - strlen(pathName));
}

void deleteFileReliably(char *pathName, const char *fileName)
{
    const size_t nameSize = strlen(fileName) + 1;
    char tmpFilePrefix[MAX_PATH + 1];
    char tmpDirName[MAX_PATH + 1];
    char tmpPathName[MAX_PATH + 1];

    if(DeleteFileA(pathName))
        return;
    // File doesn't exist, nothing to do
    if(ERROR_FILE_NOT_FOUND == GetLastError())
        return;

    // File can't be deleted, rename it and register pending deletion
    strncpy(tmpFilePrefix, fileName, nameSize);
    strncat(tmpFilePrefix, ".", nameSize);
    constructSystemDirName(tmpDirName);
    GetTempFileNameA(tmpDirName, tmpFilePrefix, 0, tmpPathName);
    DeleteFileA(tmpPathName);
    MoveFileA(pathName, tmpPathName);
    MoveFileExA(tmpPathName, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
}

BOOL copyFileIntoSystem(const char *fileName, char **argv)
{
    char driverPathName[MAX_PATH + 1];
    char setupPathName[MAX_PATH + 1];
    int setupPathLen;

    setupPathLen = strrchr(argv[0], '\\') - argv[0];
    if(setupPathLen > (int)(MAX_PATH - strlen(fileName) + 1 - 2))
    {
        MessageBoxA(NULL, CANNOT_INSTALL_PATH_TOO_LONG_ERR, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return 2;
    }

    constructDriverPathName(driverPathName, fileName);
    deleteFileReliably(driverPathName, fileName);

    strncpy(setupPathName, argv[0], setupPathLen);
    setupPathName[setupPathLen] = 0;
    strncat(setupPathName, PATH_SEPARATOR, MAX_PATH - strlen(setupPathName));
    strncat(setupPathName, fileName, MAX_PATH - strlen(setupPathName));

    if(!CopyFileA(setupPathName, driverPathName, FALSE))
    {
        MessageBoxA(NULL, CANNOT_INSTALL_FILE_COPY_ERR, FILE_ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    return TRUE;
}

int main(int argc, char *argv[])
{
    char pathName[MAX_PATH + 1];
    BOOL installMode;

    if(argc != 2 || (_stricmp(INSTALL_COMMAND, argv[1]) != 0 && _stricmp(UNINSTALL_COMMAND, argv[1]) != 0))
    {
        MessageBoxA(NULL, USAGE_MSG, INFORMATION_TITLE, MB_OK | MB_ICONINFORMATION);
        return 1;
    }

    if(_stricmp(UNINSTALL_COMMAND, argv[1]) == 0)
    {
        constructDriverPathName(pathName, OPL3EMU_DRIVER_NAME);
        deleteFileReliably(pathName, OPL3EMU_DRIVER_NAME);
        constructDriverPathName(pathName, OPL3EMU_CPLAPPLET_NAME);
        deleteFileReliably(pathName, OPL3EMU_CPLAPPLET_NAME);
        constructDriverPathName(pathName, OPL3EMU_SETUPTOOL_NAME);
        deleteFileReliably(pathName, OPL3EMU_SETUPTOOL_NAME);

        unregisterDriver();
        return 0;
    }

    installMode = TRUE;
    if(!registerDriver(&installMode))
        return 3;

    if(!copyFileIntoSystem(OPL3EMU_DRIVER_NAME, argv))
        return 4;
    if(!copyFileIntoSystem(OPL3EMU_CPLAPPLET_NAME, argv))
        return 4;
    if(!copyFileIntoSystem(OPL3EMU_SETUPTOOL_NAME, argv))
        return 4;

    MessageBoxA(NULL, installMode ? SUCCESSFULLY_INSTALLED_MSG : SUCCESSFULLY_UPDATED_MSG, INFORMATION_TITLE, MB_OK | MB_ICONINFORMATION);
    return 0;
}
