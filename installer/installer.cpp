#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <string>
#include <vector>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")

#define IDI_APP_ICON    101
#define IDR_PAYLOAD_EXE 1001
#define IDR_PAYLOAD_ICO 1002

static bool ExtractResourceToFile(int resId, const std::wstring& destinationPath) {
    HMODULE hModule = GetModuleHandleW(nullptr);
    HRSRC hRes = FindResourceW(hModule, MAKEINTRESOURCEW(resId), MAKEINTRESOURCEW(10));
    if (!hRes) return false;

    HGLOBAL hData = LoadResource(hModule, hRes);
    if (!hData) return false;

    DWORD size = SizeofResource(hModule, hRes);
    const void* pData = LockResource(hData);
    if (!pData || size == 0) return false;

    HANDLE hFile = CreateFileW(destinationPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok = WriteFile(hFile, pData, size, &written, nullptr);
    CloseHandle(hFile);
    return (ok && written == size);
}

static bool CreateShellShortcut(const std::wstring& shortcutPath, const std::wstring& targetExePath, const std::wstring& iconPath, const std::wstring& description) {
    IShellLinkW* psl = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl))) {
        return false;
    }

    psl->SetPath(targetExePath.c_str());
    psl->SetDescription(description.c_str());

    std::wstring workingDir = targetExePath.substr(0, targetExePath.find_last_of(L"\\/"));
    psl->SetWorkingDirectory(workingDir.c_str());

    if (!iconPath.empty()) {
        psl->SetIconLocation(iconPath.c_str(), 0);
    }

    IPersistFile* ppf = nullptr;
    bool success = false;
    if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))) {
        if (SUCCEEDED(ppf->Save(shortcutPath.c_str(), TRUE))) {
            success = true;
        }
        ppf->Release();
    }
    psl->Release();
    return success;
}

// Clean native Win32 process termination without cmd.exe or taskkill
static void TerminateExistingProcess(const wchar_t* processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName) == 0) {
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProc) {
                    TerminateProcess(hProc, 0);
                    CloseHandle(hProc);
                }
            }
        } while (Process32NextW(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);
    Sleep(200);
}

static std::wstring GetInstallDirectory() {
    PWSTR pLocalApp = nullptr;
    std::wstring installDir = L"";
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &pLocalApp)) && pLocalApp) {
        installDir = std::wstring(pLocalApp) + L"\\Programs\\Perdanga11";
        CoTaskMemFree(pLocalApp);
    }
    return installDir;
}

static void RegisterUninstallEntry(const std::wstring& installDir, const std::wstring& exePath, const std::wstring& iconPath) {
    HKEY hKey = nullptr;
    const wchar_t* subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Perdanga11";

    if (RegCreateKeyExW(HKEY_CURRENT_USER, subKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        std::wstring uninstallerCmd = L"\"" + installDir + L"\\Uninstall.exe\" --uninstall";
        const wchar_t* displayName = L"Perdanga11";
        const wchar_t* displayVer = L"1.0.0";
        const wchar_t* publisher = L"Perdanga";

        RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ, (const BYTE*)displayName, (DWORD)((wcslen(displayName) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"DisplayVersion", 0, REG_SZ, (const BYTE*)displayVer, (DWORD)((wcslen(displayVer) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"Publisher", 0, REG_SZ, (const BYTE*)publisher, (DWORD)((wcslen(publisher) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"DisplayIcon", 0, REG_SZ, (const BYTE*)iconPath.c_str(), (DWORD)((iconPath.length() + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ, (const BYTE*)uninstallerCmd.c_str(), (DWORD)((uninstallerCmd.length() + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"InstallLocation", 0, REG_SZ, (const BYTE*)installDir.c_str(), (DWORD)((installDir.length() + 1) * sizeof(wchar_t)));

        DWORD noModify = 1;
        RegSetValueExW(hKey, L"NoModify", 0, REG_DWORD, (const BYTE*)&noModify, sizeof(noModify));
        RegSetValueExW(hKey, L"NoRepair", 0, REG_DWORD, (const BYTE*)&noModify, sizeof(noModify));

        RegCloseKey(hKey);
    }
}

static void UnregisterUninstallEntry() {
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Perdanga11");
}

static void PerformUninstall() {
    TerminateExistingProcess(L"Perdanga11.exe");

    // 1. Remove startup folder shortcut (official Windows autostart method)
    PWSTR pStartup = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Startup, 0, nullptr, &pStartup)) && pStartup) {
        std::wstring autoShortcut = std::wstring(pStartup) + L"\\Perdanga11.lnk";
        DeleteFileW(autoShortcut.c_str());
        CoTaskMemFree(pStartup);
    }

    // 2. Remove desktop shortcut
    PWSTR pDesktop = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &pDesktop)) && pDesktop) {
        std::wstring deskShortcut = std::wstring(pDesktop) + L"\\Perdanga11.lnk";
        DeleteFileW(deskShortcut.c_str());
        CoTaskMemFree(pDesktop);
    }

    // 3. Remove start menu shortcut
    PWSTR pPrograms = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr, &pPrograms)) && pPrograms) {
        std::wstring startShortcut = std::wstring(pPrograms) + L"\\Perdanga11.lnk";
        DeleteFileW(startShortcut.c_str());
        CoTaskMemFree(pPrograms);
    }

    // 4. Remove shell context menu entries
    const wchar_t* shellKeys[] = {
        L"Software\\Classes\\exefile\\shell\\PinToPerdanga11",
        L"Software\\Classes\\lnkfile\\shell\\PinToPerdanga11",
        L"Software\\Classes\\Directory\\shell\\PinToPerdanga11",
        L"Software\\Classes\\Folder\\shell\\PinToPerdanga11"
    };
    for (const wchar_t* k : shellKeys) {
        RegDeleteTreeW(HKEY_CURRENT_USER, k);
    }

    // 5. Unregister from Windows Settings -> Apps
    UnregisterUninstallEntry();

    // 6. Delete install files
    std::wstring installDir = GetInstallDirectory();
    DeleteFileW((installDir + L"\\Perdanga11.exe").c_str());
    DeleteFileW((installDir + L"\\perdanga11.ico").c_str());

    MessageBoxW(nullptr, L"Perdanga11 has been successfully uninstalled.", L"Perdanga11 Uninstaller", MB_OK | MB_ICONINFORMATION);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if (wcsstr(GetCommandLineW(), L"--uninstall") != nullptr) {
        int resp = MessageBoxW(nullptr, L"Are you sure you want to completely uninstall Perdanga11?", L"Perdanga11", MB_YESNO | MB_ICONQUESTION);
        if (resp == IDYES) {
            PerformUninstall();
        }
        CoUninitialize();
        return 0;
    }

    int prompt = MessageBoxW(nullptr, L"Do you want to install Perdanga11 on this computer?", L"Perdanga11 Setup", MB_YESNO | MB_ICONQUESTION);
    if (prompt != IDYES) {
        CoUninitialize();
        return 0;
    }

    TerminateExistingProcess(L"Perdanga11.exe");

    std::wstring installDir = GetInstallDirectory();
    if (installDir.empty()) {
        MessageBoxW(nullptr, L"Failed to determine installation directory.", L"Installation Error", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    SHCreateDirectoryExW(nullptr, installDir.c_str(), nullptr);

    std::wstring destExe = installDir + L"\\Perdanga11.exe";
    std::wstring destIco = installDir + L"\\perdanga11.ico";
    std::wstring destUninst = installDir + L"\\Uninstall.exe";

    if (!ExtractResourceToFile(IDR_PAYLOAD_EXE, destExe)) {
        MessageBoxW(nullptr, L"Failed to unpack Perdanga11 executable payload.", L"Installation Error", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    ExtractResourceToFile(IDR_PAYLOAD_ICO, destIco);

    wchar_t currentSetupExe[MAX_PATH];
    GetModuleFileNameW(nullptr, currentSetupExe, MAX_PATH);
    CopyFileW(currentSetupExe, destUninst.c_str(), FALSE);

    // 1. Create Start Menu shortcut
    PWSTR pPrograms = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr, &pPrograms)) && pPrograms) {
        std::wstring startShortcut = std::wstring(pPrograms) + L"\\Perdanga11.lnk";
        CreateShellShortcut(startShortcut, destExe, destIco, L"Perdanga11 Start Menu");
        CoTaskMemFree(pPrograms);
    }

    // 2. Create Desktop shortcut
    PWSTR pDesktop = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &pDesktop)) && pDesktop) {
        std::wstring deskShortcut = std::wstring(pDesktop) + L"\\Perdanga11.lnk";
        CreateShellShortcut(deskShortcut, destExe, destIco, L"Perdanga11 Start Menu");
        CoTaskMemFree(pDesktop);
    }

    // 3. Register in Windows Settings -> Apps & Features
    RegisterUninstallEntry(installDir, destExe, destIco);

    // 4. Launch installed application
    ShellExecuteW(nullptr, L"open", destExe.c_str(), nullptr, installDir.c_str(), SW_SHOWNORMAL);

    MessageBoxW(nullptr, L"Installation completed successfully!\nPerdanga11 is now running and ready.", L"Perdanga11 Setup", MB_OK | MB_ICONINFORMATION);

    CoUninitialize();
    return 0;
}