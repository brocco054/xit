#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <string>

namespace {
int show_error(const wchar_t* message) {
    MessageBoxW(nullptr, message, L"Acervo | Laborat\u00f3rio", MB_OK | MB_ICONERROR);
    return 1;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t executable[32768];
    const DWORD length = GetModuleFileNameW(nullptr, executable, static_cast<DWORD>(std::size(executable)));
    if (length == 0 || length >= std::size(executable)) {
        return show_error(L"N\u00e3o foi poss\u00edvel localizar o iniciador.");
    }

    const auto directory = std::filesystem::path(executable).parent_path();
    const auto script = directory / L"internal" / L"lab-window.ps1";
    std::error_code error;
    if (!std::filesystem::is_regular_file(script, error)) {
        return show_error(L"Mantenha o iniciador na mesma pasta que a pasta internal.");
    }

    wchar_t system_directory[MAX_PATH];
    const UINT system_length = GetSystemDirectoryW(system_directory, MAX_PATH);
    if (system_length == 0 || system_length >= MAX_PATH) {
        return show_error(L"N\u00e3o foi poss\u00edvel localizar o PowerShell do Windows.");
    }

    const auto powershell = std::filesystem::path(system_directory) / L"WindowsPowerShell" / L"v1.0" / L"powershell.exe";
    const std::wstring arguments = L"-NoLogo -NoProfile -STA -WindowStyle Hidden -File \"" + script.native() + L"\"";
    const auto result = ShellExecuteW(nullptr, L"open", powershell.c_str(), arguments.c_str(), directory.c_str(), SW_HIDE);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        return show_error(L"N\u00e3o foi poss\u00edvel abrir a janela do laborat\u00f3rio.");
    }
    return 0;
}
