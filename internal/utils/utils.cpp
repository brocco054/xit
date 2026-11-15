#include "../main.hpp"

std::vector<int> c_opcodes::ida_to_bytes(const char* pattern)
{
    std::vector<int> bytes = std::vector<int>{};
    char* start = const_cast<char*>(pattern);
    char* end = const_cast<char*>(pattern) + strlen(pattern);

    for (char* current = start; current < end; ++current) {
        if (*current == '?') {
            ++current;

            if (*current == '?')
                ++current;

            bytes.push_back(-1);
        }
        else {
            bytes.push_back(strtoul(current, &current, 16));
        }
    }

    return bytes;
}

uint8_t* c_opcodes::scan(const char* module_name, const char* pattern) {
    void* module_handle = WINCALL(GetModuleHandle)(module_name);
    if (module_handle == nullptr)
        return nullptr;

    PIMAGE_DOS_HEADER dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(module_handle);
    PIMAGE_NT_HEADERS nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<uint8_t*>(module_handle) + dos_header->e_lfanew);

    auto size_of_image = nt_headers->OptionalHeader.SizeOfImage;
    auto pattern_bytes = ida_to_bytes(pattern);
    auto scan_bytes = reinterpret_cast<uint8_t*>(module_handle);

    auto pattern_size = pattern_bytes.size();
    auto pattern_data = pattern_bytes.data();

    for (unsigned int i = 0; i < size_of_image - pattern_size; i++) {
        bool found = true;

        for (unsigned int j = 0; j < pattern_size; ++j) {
            if (pattern_data[j] == -1)
                continue;

            if (scan_bytes[i + j] != pattern_data[j]) {
                found = false;
                break;
            }
        }

        if (found)
            return &scan_bytes[i];
    }
    LOG_ERROR("failed to find pattern: %s", pattern);
    return nullptr;
}

uint8_t* c_opcodes::scan(const char* module_name, const char* pattern, int offset)
{
    return scan(module_name, pattern) + offset;
}

uint8_t* c_opcodes::scan_absolute(const char* module_name, const char* pattern) {
    uint8_t* address = scan(module_name, pattern);
    if (address == nullptr)
        return nullptr;

    return get_absolute_address(address);
}

uint8_t* c_opcodes::scan_absolute(const char* module_name, const char* pattern, int pre_offset) {
    uint8_t* address = scan(module_name, pattern);
    if (address == nullptr)
        return nullptr;

    return get_absolute_address(address, pre_offset);
}

uint8_t* c_opcodes::scan_absolute(const char* module_name, const char* pattern, int pre_offset, int post_offset) {
    uint8_t* address = scan(module_name, pattern);
    if (address == nullptr)
        return nullptr;

    return get_absolute_address(address, pre_offset, post_offset);
}

unsigned __int64 c_opcodes::export_fn(unsigned __int64 base, const char* procedure_name)
{
    const auto dos_headers = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos_headers->e_magic != IMAGE_DOS_SIGNATURE)
        return 0;

    const auto nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos_headers->e_lfanew);
    if (nt_headers->Signature != IMAGE_NT_SIGNATURE)
        return 0;

    const std::uintptr_t export_directory_size = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
    const std::uintptr_t export_directory_address = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;

    if (export_directory_size == 0u || export_directory_address == 0u)
        return 0;

    const auto exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + export_directory_address);
    const auto names_rva = reinterpret_cast<const std::uint32_t*>(base + exports->AddressOfNames);
    const auto name_ordinals_rva = reinterpret_cast<const std::uint16_t*>(base + exports->AddressOfNameOrdinals);
    const auto functions_rva = reinterpret_cast<const std::uint32_t*>(base + exports->AddressOfFunctions);

    std::size_t right = exports->NumberOfNames, left = 0u;
    while (right != left)
    {
        const std::size_t middle = left + ((right - left) >> 1u);
        const int result = strcmp(procedure_name, reinterpret_cast<const char*>(base + names_rva[middle]));

        if (result == 0)
        {
            const std::uint32_t function_rva = functions_rva[name_ordinals_rva[middle]];

            if (function_rva >= export_directory_address && function_rva - export_directory_address < export_directory_size)
                break;

            return base + function_rva;
        }

        if (result > 0)
            left = middle + 1;
        else
            right = middle;
    }
    return 0;
}

uint8_t* c_opcodes::get_absolute_address(unsigned char* address, int pre_offset, int post_offset) {
    address += pre_offset;
    address += *reinterpret_cast<std::int32_t*>(address) + 0x4;
    address += post_offset;

    return address;
}

uint8_t* c_opcodes::get_absolute_address(unsigned char* address, int pre_offset) {
    return get_absolute_address(address, pre_offset, 0);
}

uint8_t* c_opcodes::get_absolute_address(unsigned char* address) {
    return get_absolute_address(address, 0, 0);
}

uint8_t* c_opcodes::resolve_relative_address(unsigned char* address, int rva_offset, int rip_offset) {
    int32_t RVA = *reinterpret_cast<int32_t*>(address + rva_offset);
    uint64_t RIP = reinterpret_cast<uint64_t>(address) + rip_offset;

    return reinterpret_cast<uint8_t*>(RIP + RVA);
}

uint8_t* c_opcodes::resolve_relative_address(unsigned char* address, int rva_offset) {
    return resolve_relative_address(address, rva_offset, 0);
}

#include <fstream>
#include <sstream>

namespace {
    // returns the quoted token that follows `key` (skips the key's own
    // closing quote, then reads the next "..." pair). empty on failure.
    std::string vdf_value_after(const std::string& data, size_t key_pos, size_t key_len) {
        size_t q1 = data.find('"', key_pos + key_len);
        if (q1 == std::string::npos) return {};
        size_t q2 = data.find('"', q1 + 1);
        if (q2 == std::string::npos) return {};
        return data.substr(q1 + 1, q2 - q1 - 1);
    }

    std::string reg_string(HKEY root, const char* sub, const char* val) {
        std::string out;
        HKEY key = nullptr;
        if (RegOpenKeyExA(root, sub, 0, KEY_READ, &key) == ERROR_SUCCESS) {
            char buf[512] = {};
            DWORD size = sizeof(buf), type = 0;
            if (RegQueryValueExA(key, val, nullptr, &type,
                    reinterpret_cast<LPBYTE>(buf), &size) == ERROR_SUCCESS && type == REG_SZ)
                out.assign(buf);
            RegCloseKey(key);
        }
        return out;
    }
}

// Resolves the most-recent Steam persona name. Priority:
//   1) loginusers.vdf  -> block with MostRecent "1" -> PersonaName
//   2) registry HKCU\Software\Valve\Steam\LastGameNameUsed
// Empty string if nothing resolves (caller renders nothing).
std::string get_steam_username() {
    const std::string steam_path = reg_string(HKEY_CURRENT_USER,
        "Software\\Valve\\Steam", "SteamPath");

    if (!steam_path.empty()) {
        std::ifstream file(steam_path + "/config/loginusers.vdf");
        if (file.is_open()) {
            std::stringstream ss;
            ss << file.rdbuf();
            const std::string data = ss.str();

            std::string first_name;
            size_t pos = 0;
            const std::string pkey = "\"PersonaName\"";

            while ((pos = data.find(pkey, pos)) != std::string::npos) {
                std::string name = vdf_value_after(data, pos, pkey.size());
                size_t after = pos + pkey.size();
                pos = after;
                if (name.empty())
                    continue;
                if (first_name.empty())
                    first_name = name;

                // search this account block (up to its closing brace) for
                // MostRecent / mostrecent == "1"
                size_t brace = data.find('}', after);
                size_t mr = data.find("\"MostRecent\"", after);
                if (mr == std::string::npos)
                    mr = data.find("\"mostrecent\"", after);
                if (mr != std::string::npos &&
                    (brace == std::string::npos || mr < brace)) {
                    if (vdf_value_after(data, mr, 12) == "1")
                        return name;
                }
            }

            if (!first_name.empty())
                return first_name;
        }
    }

    // registry fallback (last game name used by the active Steam user)
    std::string last = reg_string(HKEY_CURRENT_USER,
        "Software\\Valve\\Steam", "LastGameNameUsed");
    return last; // may be empty -> caller renders nothing
}
