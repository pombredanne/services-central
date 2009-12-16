/* -*- Mode: C++; c-basic-offset: 2; tab-width: 8; indent-tabs-mode: nil; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Fennec Installer for WinCE.
 *
 * The Initial Developer of the Original Code is The Mozilla Foundation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Pakhotin <alexp@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 *
 * Uninstall.cpp : Mozilla Fennec Uninstaller
 *
 */

#include <windows.h>
#include "nsSetupStrings.h"

const WCHAR c_sStringsFile[] = L"setup.ini";
nsSetupStrings Strings;

WCHAR g_sUninstallPath[MAX_PATH];

const DWORD c_nTempBufSize = MAX_PATH * 2;
const WCHAR c_sRemoveParam[] = L"remove";

enum {
  ErrOK = 0,
  ErrCancel = 1,
  ErrNoStrings = -1,
  ErrInstallationNotFound = -2,
  ErrShutdownFailed = -3,
};


// Forward declarations
BOOL GetModulePath(WCHAR *sPath);
BOOL LoadStrings();
BOOL GetInstallPath(WCHAR *sPath);
BOOL DeleteShortcut(HWND hwndParent);
BOOL DeleteDirectory(const WCHAR* sPathToDelete);
BOOL DeleteRegistryKey();
BOOL CopyAndLaunch();
BOOL ShutdownFastStartService(const WCHAR *sInstallPath);

// Main
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPTSTR    lpCmdLine,
                   int       nCmdShow)
{
  HWND hWnd = GetForegroundWindow();
  *g_sUninstallPath = 0;

  WCHAR *sCommandLine = GetCommandLine();
  WCHAR *sRemoveParam = wcsstr(sCommandLine, c_sRemoveParam);
  if (sRemoveParam != NULL)
  {
    // Launched from a temp directory with parameters "remove \Program Files\Fennec\"
    wcscpy(g_sUninstallPath, sRemoveParam + wcslen(c_sRemoveParam) + 1);
  }
  else
  {
    // Just copy this EXE and launch it from a temp location with a special parameter
    // to delete itself in the installation directory
    if (CopyAndLaunch())
      return ErrOK;
  }
  // Perform uninstallation when executed with a special parameter
  // (or in case when CopyAndLaunch failed - just execute in place)

  if (!LoadStrings())
  {
    MessageBoxW(hWnd, L"Cannot find the strings file.", L"Uninstall", MB_OK|MB_ICONWARNING);
    return ErrNoStrings;
  }

  WCHAR sInstallPath[MAX_PATH];
  if (GetInstallPath(sInstallPath))
  {
    if (!ShutdownFastStartService(sInstallPath))
    {
      //TODO: May need to handle this situation

      //MessageBoxW(hWnd, L"Unable to shut down Fennec. Try to reset your device.",
      //            Strings.GetString(StrID_UninstallCaption), MB_OK|MB_ICONWARNING);
      //return ErrShutdownFailed;
    }

    WCHAR sMsg[c_nTempBufSize];
    _snwprintf(sMsg, c_nTempBufSize, L"%s %s\n%s", Strings.GetString(StrID_FilesWillBeRemoved),
      sInstallPath, Strings.GetString(StrID_AreYouSure));
    if (MessageBoxW(hWnd, sMsg, Strings.GetString(StrID_UninstallCaption),
                    MB_YESNO|MB_ICONWARNING) == IDNO)
    {
      return ErrCancel;
    }

    // Remove all installed files
    DeleteDirectory(sInstallPath);
    DeleteShortcut(hWnd);
    DeleteRegistryKey();

    // TODO: May probably handle errors during deletion (such as locked directories)
    // and notify user. Right now just show successful message.
    MessageBoxW(hWnd, Strings.GetString(StrID_UninstalledSuccessfully),
                Strings.GetString(StrID_UninstallCaption), MB_OK|MB_ICONINFORMATION);
  }
  else
  {
    MessageBoxW(hWnd, Strings.GetString(StrID_InstallationNotFound),
                Strings.GetString(StrID_UninstallCaption), MB_OK|MB_ICONINFORMATION);
    return ErrInstallationNotFound;
  }

  return ErrOK;
}

BOOL LoadStrings()
{
  // Locate and load the strings file used by installer
  if (*g_sUninstallPath == 0)
    GetModulePath(g_sUninstallPath);

  WCHAR sStringsFile[MAX_PATH];
  _snwprintf(sStringsFile, MAX_PATH, L"%s\\%s", g_sUninstallPath, c_sStringsFile);

  return Strings.LoadStrings(sStringsFile);
}

BOOL GetModulePath(WCHAR *sPath)
{
  if (GetModuleFileName(NULL, sPath, MAX_PATH) == 0)
    return FALSE;

  WCHAR *sSlash = wcsrchr(sPath, L'\\') + 1;
  *sSlash = L'\0'; // cut the file name

  return TRUE;
}

BOOL GetInstallPath(WCHAR *sPath)
{
  HKEY hKey;
  WCHAR sRegFennecKey[MAX_PATH];
  _snwprintf(sRegFennecKey, MAX_PATH, L"Software\\%s", Strings.GetString(StrID_AppShortName));

  LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, sRegFennecKey, 0, KEY_ALL_ACCESS, &hKey);
  if (result == ERROR_SUCCESS)
  {
    DWORD dwType = NULL;
    DWORD dwCount = MAX_PATH * sizeof(WCHAR);
    result = RegQueryValueEx(hKey, L"Path", NULL, &dwType, (LPBYTE)sPath, &dwCount);

    RegCloseKey(hKey);
  }

  return (result == ERROR_SUCCESS);
}

BOOL DeleteDirectory(const WCHAR* sPathToDelete)
{
  HANDLE          hFile;
  WCHAR           sFilePath[MAX_PATH];
  WCHAR           sPattern[MAX_PATH];
  WIN32_FIND_DATA findData;

  wcscpy(sPattern, sPathToDelete);
  wcscat(sPattern, L"\\*.*");
  hFile = FindFirstFile(sPattern, &findData);
  if (hFile != INVALID_HANDLE_VALUE)
  {
    do
    {
      if (findData.cFileName[0] != L'.')
      {
        _snwprintf(sFilePath, MAX_PATH, L"%s\\%s", sPathToDelete, findData.cFileName);

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
          // Delete subdirectory
          BOOL bRet = DeleteDirectory(sFilePath);
          if (!bRet)
          {
            // Just continue
            //return bRet;
          }
        }
        else
        {
          // Set file attributes
          if (SetFileAttributes(sFilePath, FILE_ATTRIBUTE_NORMAL))
          {
            // Delete file
            if (!DeleteFile(sFilePath))
            {
              // Just continue
              //return FALSE;
            }
          }
        }
      }
    } while (FindNextFile(hFile, &findData));

    // Close handle
    FindClose(hFile);

    DWORD dwError = GetLastError();
    if (dwError != ERROR_NO_MORE_FILES)
      return FALSE;
    else
    {
      // Set directory attributes
      if (SetFileAttributes(sPathToDelete, FILE_ATTRIBUTE_NORMAL))
      {
        // Delete directory
        if (!RemoveDirectory(sPathToDelete))
          return FALSE;
      }
    }
  }

  return TRUE;
}

// Deletes shortcuts for Fennec and FastStart service created by the installer.
// Note: The shortcut names have to be in sync with CreateShortcut in nsInstallerDlg.cpp
BOOL DeleteShortcut(HWND hwndParent)
{
  BOOL result = FALSE;

  WCHAR sProgramsPath[MAX_PATH];
  if (SHGetSpecialFolderPath(hwndParent, sProgramsPath, CSIDL_PROGRAMS, FALSE))
  {
    WCHAR sShortcutPath[MAX_PATH];
    _snwprintf(sShortcutPath, MAX_PATH, L"%s\\%s.lnk", sProgramsPath, Strings.GetString(StrID_AppShortName));


    if(SetFileAttributes(sShortcutPath, FILE_ATTRIBUTE_NORMAL))
      result = DeleteFile(sShortcutPath);
  }

  // Handle faststart shortcut
  WCHAR sStartupPath[MAX_PATH];
  if (SHGetSpecialFolderPath(hwndParent, sStartupPath, CSIDL_STARTUP, FALSE))
  {
    WCHAR sStartupShortcutPath[MAX_PATH];
    _snwprintf(sStartupShortcutPath, MAX_PATH, L"%s\\%sFastStart.lnk", sStartupPath, Strings.GetString(StrID_AppShortName));

    // It may not exist - just ignore that
    if(SetFileAttributes(sStartupShortcutPath, FILE_ATTRIBUTE_NORMAL))
      DeleteFile(sStartupShortcutPath);
  }

  return result;
}

BOOL DeleteRegistryKey()
{
  WCHAR sRegFennecKey[MAX_PATH];
  _snwprintf(sRegFennecKey, MAX_PATH, L"Software\\%s", Strings.GetString(StrID_AppShortName));
  LONG result = RegDeleteKey(HKEY_LOCAL_MACHINE, sRegFennecKey);
  return (result == ERROR_SUCCESS);
}

// Copy to a temp directory and launch from there
BOOL CopyAndLaunch()
{
  WCHAR sNewName[] = L"\\Temp\\uninstall.exe";
  WCHAR sModule[MAX_PATH];
  if (GetModuleFileName(NULL, sModule, MAX_PATH) == 0 || !GetModulePath(g_sUninstallPath))
    return FALSE;
  if (CopyFile(sModule, sNewName, FALSE))
  {
    PROCESS_INFORMATION pi;
    WCHAR sParam[c_nTempBufSize];
    _snwprintf(sParam, c_nTempBufSize, L"%s %s", c_sRemoveParam, g_sUninstallPath);

    // Launch "\Temp\uninstall.exe remove \Program Files\Fennec\"
    return CreateProcess(sNewName, sParam, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
  }
  else
    return FALSE;
}

BOOL ConvertToChar(const WCHAR *wstr, char *str, DWORD bufSize)
{
  return 0 != WideCharToMultiByte(CP_ACP, 0, wstr, -1, str, bufSize, NULL, NULL);
}

BOOL ShutdownFastStartService(const WCHAR *wsInstallPath)
{
  BOOL result = TRUE;

  // Class name: appName + "MessageWindow"
  WCHAR sClassName[c_nTempBufSize];
  _snwprintf(sClassName, c_nTempBufSize, L"%s%s", Strings.GetString(StrID_AppShortName), L"MessageWindow");

  HWND handle = ::FindWindowW(sClassName, NULL);
  if (handle)
  {
    char sPath[MAX_PATH];
    ConvertToChar(wsInstallPath, sPath, MAX_PATH);
    size_t pathLen = strlen(sPath);
    if (pathLen > 0 && sPath[pathLen-1] != '\\')
    {
      strcat(sPath, "\\");
      pathLen++;
    }

    char sCopyData[MAX_PATH * 3];
    _snprintf(sCopyData, MAX_PATH * 2, "\"%s%S.exe\" -shutdown-faststart", sPath, Strings.GetString(StrID_AppShortName));
    size_t cmdLineLen = strlen(sCopyData);

    char *sRest = sCopyData + cmdLineLen + 1;
    strcpy(sRest, sPath);

    COPYDATASTRUCT cds = {
      1,
      pathLen + 1 + cmdLineLen,
      (void*) sCopyData
    };
    ::SendMessage(handle, WM_COPYDATA, 0, (LPARAM)&cds);

    // Wait 10 seconds or until it's shut down
    for (int i = 0; i < 20 && handle; i++)
    {
      Sleep(500);
      handle = ::FindWindowW(sClassName, NULL);
    }
    // The window must not exist if the service shut down properly
    result = (handle == NULL);
  }
  return result;
}
