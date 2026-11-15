#pragma once

#include <Windows.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>

namespace lab_diagnostics {

inline void record(const char* stage, const char* detail = "") noexcept {
    wchar_t directory[MAX_PATH];
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", directory, MAX_PATH);
    if (!length || length + 64 >= MAX_PATH)
        return;
    swprintf_s(directory + length, MAX_PATH - length, L"\\Hy3x");
    if (!CreateDirectoryW(directory, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        return;
    length = static_cast<DWORD>(wcslen(directory));
    swprintf_s(directory + length, MAX_PATH - length, L"\\sessao-%lu.log", GetCurrentProcessId());
    const auto file = CreateFileW(directory, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;

    SYSTEMTIME time{};
    GetSystemTime(&time);
    char line[640];
    const int count = _snprintf_s(line, sizeof line, _TRUNCATE,
        "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ %s %.480s\r\n",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute,
        time.wSecond, time.wMilliseconds, stage, detail);
    DWORD written = 0;
    if (count > 0)
        WriteFile(file, line, static_cast<DWORD>(count), &written, nullptr);
    CloseHandle(file);
}

inline void measure_frame(bool menu_open) noexcept {
    using clock = std::chrono::steady_clock;
    static const bool enabled = [] {
        wchar_t path[MAX_PATH];
        const auto length = GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH);
        if (!length || length + 64 >= MAX_PATH)
            return false;
        swprintf_s(path + length, MAX_PATH - length, L"\\Hy3x\\medir-%lu.flag", GetCurrentProcessId());
        const auto attributes = GetFileAttributesW(path);
        return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
    }();
    if (!enabled)
        return;

    constexpr size_t batch = 300;
    static thread_local std::array<double, batch> frame_times{};
    static thread_local size_t used = 0;
    static thread_local bool was_open = false;
    static thread_local auto previous = clock::now();
    const auto now = clock::now();
    const double milliseconds = std::chrono::duration<double, std::milli>(now - previous).count();
    previous = now;

    if (menu_open != was_open) {
        record(menu_open ? "panel_open" : "panel_closed");
        used = 0;
        was_open = menu_open;
        return;
    }

    frame_times[used++] = milliseconds;
    if (used == batch) {
        std::sort(frame_times.begin(), frame_times.end());
        char summary[128];
        sprintf_s(summary, "%s n=%zu p95_ms=%.3f p99_ms=%.3f",
            menu_open ? "open" : "closed", batch, frame_times[284], frame_times[296]);
        record("frame_sample", summary);
        used = 0;
    }
}

} // namespace lab_diagnostics
