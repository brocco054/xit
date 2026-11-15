#pragma once

#include "profile_codec.hpp"
#include <Windows.h>
#include <atomic>
#include <filesystem>
#include <utility>
#include <vector>

namespace acervo::profile {

class files {
public:
    explicit files(std::filesystem::path directory) : directory_(std::move(directory)) {}

    result ensure_directory() const {
        directory_handles directory;
        return open_directory(directory, true);
    }

    result read(std::string_view name, std::string& output) const {
        const auto path = path_for(name);
        if (path.empty())
            return { status::invalid_name, {} };
        directory_handles directory;
        if (const auto ready = open_directory(directory, false); !ready)
            return ready;

        const handle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (!file)
            return last_error();
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(file.value, &info) || !regular(info.dwFileAttributes))
            return { status::io_error, {} };
        if (info.nFileSizeHigh || info.nFileSizeLow > max_size)
            return { status::invalid_data, {} };
        std::string staged(info.nFileSizeLow, '\0');
        DWORD bytes = 0;
        if (!ReadFile(file.value, staged.data(), info.nFileSizeLow, &bytes, nullptr) || bytes != info.nFileSizeLow)
            return { status::io_error, {} };
        output = std::move(staged);
        return {};
    }

    result write(std::string_view name, std::string_view contents, bool overwrite) const {
        const auto path = path_for(name);
        if (path.empty())
            return { status::invalid_name, {} };
        if (contents.size() > max_size)
            return { status::invalid_data, {} };
        directory_handles directory;
        if (const auto ready = open_directory(directory, !overwrite); !ready)
            return ready;
        if (overwrite) {
            const auto present = check_file(path);
            if (!present)
                return present;
        }

        std::filesystem::path temporary;
        result written;
        {
            // CREATE_NEW ensures a stale or concurrent writer's temp is never truncated.
            static std::atomic<unsigned long long> sequence{0};
            const auto label = L".acervo-" + std::to_wstring(GetCurrentProcessId()) + L"-"
                + std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(++sequence) + L".tmp";
            temporary = directory_ / label;
            const handle file(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (!file)
                return last_error();
            const auto length = static_cast<DWORD>(contents.size());
            DWORD bytes = 0;
            if (!WriteFile(file.value, contents.data(), length, &bytes, nullptr)
                || bytes != length || !FlushFileBuffers(file.value))
                written = { status::io_error, {} };
        }
        if (written) {
            const DWORD flags = MOVEFILE_WRITE_THROUGH | (overwrite ? MOVEFILE_REPLACE_EXISTING : 0);
            if (MoveFileExW(temporary.c_str(), path.c_str(), flags))
                return {};
            written = last_error();
        }
        DeleteFileW(temporary.c_str());
        return written;
    }

    result remove(std::string_view name) const {
        const auto path = path_for(name);
        if (path.empty())
            return { status::invalid_name, {} };
        directory_handles directory;
        if (const auto ready = open_directory(directory, false); !ready)
            return ready;
        if (const auto present = check_file(path); !present)
            return present;
        return DeleteFileW(path.c_str()) ? result{} : last_error();
    }

    result rename(std::string_view from, std::string_view to) const {
        const auto source = path_for(from);
        const auto destination = path_for(to);
        if (source.empty() || destination.empty())
            return { status::invalid_name, {} };
        directory_handles directory;
        if (const auto ready = open_directory(directory, false); !ready)
            return ready;
        if (const auto present = check_file(source); !present)
            return present;
        if (source == destination)
            return {};
        return MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH) ? result{} : last_error();
    }

    result list(std::vector<std::string>& output) const {
        directory_handles directory;
        if (const auto ready = open_directory(directory, false); !ready) {
            if (ready.code == status::not_found) {
                output.clear();
                return {};
            }
            return ready;
        }
        std::vector<std::string> staged;
        std::error_code error;
        for (auto entry = std::filesystem::directory_iterator(directory_, error);
            !error && entry != std::filesystem::directory_iterator(); entry.increment(error)) {
            const auto& path = entry->path();
            if (CompareStringOrdinal(path.extension().c_str(), -1, L".cfg", -1, TRUE) != CSTR_EQUAL
                || !check_file(path))
                continue;
            const auto utf8 = path.stem().u8string();
            std::string name(utf8.begin(), utf8.end());
            if (!path_for(name).empty())
                staged.push_back(std::move(name));
        }
        if (error)
            return { status::io_error, {} };
        std::sort(staged.begin(), staged.end());
        output = std::move(staged);
        return {};
    }

private:
    static constexpr DWORD max_size = 1024 * 1024;

    struct handle {
        HANDLE value = INVALID_HANDLE_VALUE;
        explicit handle(HANDLE handle_value) : value(handle_value) {}
        handle(const handle&) = delete;
        handle& operator=(const handle&) = delete;
        handle(handle&& other) noexcept : value(std::exchange(other.value, INVALID_HANDLE_VALUE)) {}
        ~handle() { if (*this) CloseHandle(value); }
        explicit operator bool() const noexcept { return value != INVALID_HANDLE_VALUE; }
    };
    using directory_handles = std::vector<handle>;

    std::filesystem::path directory_;

    static result last_error() {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
            return { status::not_found, {} };
        if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS)
            return { status::conflict, {} };
        return { status::io_error, {} };
    }

    static bool regular(DWORD attributes) {
        return attributes != INVALID_FILE_ATTRIBUTES
            && !(attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
    }

    static result check_file(const std::filesystem::path& path) {
        const auto attributes = GetFileAttributesW(path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES)
            return last_error();
        return regular(attributes) ? result{} : result{ status::io_error, {} };
    }

    result open_directory(directory_handles& opened, bool create) const {
        std::error_code error;
        const auto absolute = std::filesystem::absolute(directory_, error).lexically_normal();
        if (error)
            return { status::io_error, {} };
        auto path = absolute.root_path();
        // Hold each directory without FILE_SHARE_DELETE until the file operation ends.
        for (const auto& component : absolute.relative_path()) {
            if (component.empty())
                continue;
            path /= component;
            if (create && !CreateDirectoryW(path.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
                return last_error();
            handle directory(CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            if (!directory)
                return last_error();
            BY_HANDLE_FILE_INFORMATION info{};
            if (!GetFileInformationByHandle(directory.value, &info)
                || !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
                return { status::io_error, {} };
            opened.push_back(std::move(directory));
        }
        return {};
    }

    std::filesystem::path path_for(std::string_view name) const {
        if (name.empty() || name.size() > 240 || name.front() == ' ' || name.back() == ' ' || name.back() == '.')
            return {};
        if (std::any_of(name.begin(), name.end(), [](unsigned char c) {
            return c < 32 || std::string_view("<>:\"/\\|?*").find(c) != std::string_view::npos;
        }))
            return {};

        const auto length = static_cast<int>(name.size());
        const int wide_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), length, nullptr, 0);
        if (!wide_length)
            return {};
        std::wstring wide(static_cast<std::size_t>(wide_length), L'\0');
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), length, wide.data(), wide_length))
            return {};
        auto device = wide.substr(0, wide.find(L'.'));
        while (!device.empty() && device.back() == L' ')
            device.pop_back();
        for (auto& c : device)
            if (c >= L'a' && c <= L'z')
                c = static_cast<wchar_t>(c - L'a' + L'A');
        if (device == L"CON" || device == L"PRN" || device == L"AUX" || device == L"NUL"
            || (device.size() == 4 && (device.compare(0, 3, L"COM") == 0 || device.compare(0, 3, L"LPT") == 0)
                && std::wstring_view(L"123456789\u00b9\u00b2\u00b3").find(device.back()) != std::wstring_view::npos))
            return {};
        return directory_ / (wide + L".cfg");
    }
};

} // namespace acervo::profile
