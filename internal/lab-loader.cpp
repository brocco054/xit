#include <windows.h>
#include <tlhelp32.h>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <iostream>
#include <memory>

namespace {
constexpr int load_indeterminate = 3;

struct close_handle {
    void operator()(void* value) const noexcept { CloseHandle(value); }
};
using unique_handle = std::unique_ptr<void, close_handle>;

int failure(const char* message, DWORD error) {
    std::cerr << message << " (Win32 " << error << ")\n";
    return 1;
}

std::uintptr_t module_base(DWORD pid, const wchar_t* name) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    unique_handle handle(snapshot);
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    for (BOOL found = Module32FirstW(snapshot, &entry); found; found = Module32NextW(snapshot, &entry)) {
        if (_wcsicmp(entry.szModule, name) == 0) return reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
    }
    return 0;
}
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) {
        std::cerr << "Usage: lab-loader <cs2 PID> <CS.dll>\n";
        return 2;
    }
    wchar_t* end = nullptr;
    const DWORD pid = std::wcstoul(argv[1], &end, 10);
    if (!pid || *end) return 2;

    std::error_code error;
    const auto dll = std::filesystem::absolute(argv[2], error);
    if (error || !std::filesystem::is_regular_file(dll, error) || _wcsicmp(dll.filename().c_str(), L"CS.dll")) return 2;

    const unique_handle process(OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid));
    if (!process) return failure("OpenProcess", GetLastError());
    wchar_t path[32768];
    DWORD length = static_cast<DWORD>(std::size(path));
    if (!QueryFullProcessImageNameW(process.get(), 0, path, &length)) return failure("QueryFullProcessImageNameW", GetLastError());
    if (_wcsicmp(std::filesystem::path(path).filename().c_str(), L"cs2.exe")) return 2;
    if (module_base(pid, L"CS.dll")) {
        std::cerr << "CS.dll is already loaded. Restart CS2 before retrying.\n";
        return 1;
    }

    const auto load = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HMODULE owner = nullptr;
    if (!load || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(load), &owner) ||
        !GetModuleFileNameW(owner, path, static_cast<DWORD>(std::size(path)))) {
        return failure("Resolve LoadLibraryW", GetLastError());
    }
    const auto remote_base = module_base(pid, std::filesystem::path(path).filename().c_str());
    if (!remote_base) return failure("Locate loader module", GetLastError());
    const auto offset = reinterpret_cast<std::uintptr_t>(load) - reinterpret_cast<std::uintptr_t>(owner);
    const auto start = reinterpret_cast<LPTHREAD_START_ROUTINE>(remote_base + offset);

    const auto size = (dll.native().size() + 1) * sizeof(wchar_t);
    void* buffer = VirtualAllocEx(process.get(), nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buffer) return failure("VirtualAllocEx", GetLastError());
    if (!WriteProcessMemory(process.get(), buffer, dll.c_str(), size, nullptr)) {
        const DWORD code = GetLastError();
        VirtualFreeEx(process.get(), buffer, 0, MEM_RELEASE);
        return failure("WriteProcessMemory", code);
    }
    const unique_handle thread(CreateRemoteThread(process.get(), nullptr, 0, start, buffer, 0, nullptr));
    if (!thread) {
        const DWORD code = GetLastError();
        VirtualFreeEx(process.get(), buffer, 0, MEM_RELEASE);
        return failure("CreateRemoteThread", code);
    }
    const DWORD wait = WaitForSingleObject(thread.get(), 10000);
    if (wait != WAIT_OBJECT_0) {
        // The load may still be using the buffer. Leave it until the process exits.
        failure("Module load outcome is indeterminate", wait == WAIT_FAILED ? GetLastError() : wait);
        return load_indeterminate;
    }
    VirtualFreeEx(process.get(), buffer, 0, MEM_RELEASE);
    if (!module_base(pid, L"CS.dll")) return failure("CS.dll was not loaded", ERROR_MOD_NOT_FOUND);
    std::cout << "CS.dll loaded.\n";
}
