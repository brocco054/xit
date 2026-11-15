#pragma once

#include "acervo_catalog.hpp"
#include "../sdk/includes/json.hpp"
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace acervo {

struct saved_kit {
    std::string name;
    std::vector<cosmetics::Preset> items;
};

enum class kit_io { ok, invalid, io_error, conflict };

class kit_store {
    using json = nlohmann::json;

    struct document {
        std::vector<saved_kit> kits;
        json rejected = json::array();
    };

public:
    explicit kit_store(std::filesystem::path path) : m_path(std::move(path)) {}

    static bool valid(const saved_kit& kit) {
        if (!valid_name(kit.name) || kit.items.empty())
            return false;

        std::set<unsigned int> occupied_slots;
        for (const auto& item : kit.items) {
            const auto* entry = std::find_if(std::begin(catalog), std::end(catalog),
                [&](const catalog_entry& candidate) { return cosmetics::is_valid(item, candidate.finish); });
            if (entry == std::end(catalog))
                return false;

            // The game keeps one knife and one glove pair per kit.
            const auto slot = entry->group == category::knife ? 500u
                : entry->group == category::glove ? 5000u : item.weapon_definition_id;
            if (!occupied_slots.insert(slot).second)
                return false;
        }
        return true;
    }

    kit_io read(std::vector<saved_kit>& kits, std::vector<std::string>* incompatible = nullptr) const {
        std::lock_guard<std::mutex> lock(m_io_lock);
        document data;
        const auto status = read_file(data);
        if (status == kit_io::ok) {
            kits = std::move(data.kits);
            if (incompatible) {
                incompatible->clear();
                for (const auto& entry : data.rejected) {
                    const auto name = entry.is_object() ? entry.find("name") : entry.end();
                    const auto* label = name == entry.end() ? nullptr : name->get_ptr<const std::string*>();
                    incompatible->push_back(label && valid_name(*label) ? *label : "Registro sem nome");
                }
            }
        }
        return status;
    }

    kit_io save(const saved_kit& kit, const std::string& replace = {}) const {
        if (!valid(kit))
            return kit_io::invalid;

        std::lock_guard<std::mutex> lock(m_io_lock);
        document data;
        const auto status = read_file(data);
        if (status != kit_io::ok)
            return status;
        const auto target = replace.empty() ? data.kits.end() : find_name(data.kits, replace);
        const auto duplicate = find_name(data.kits, kit.name);
        if (duplicate != data.kits.end() && duplicate != target)
            return kit_io::conflict;
        if (!replace.empty() && target == data.kits.end())
            return kit_io::invalid;

        if (target == data.kits.end())
            data.kits.push_back(kit);
        else
            *target = kit;
        return write_file(data);
    }

    kit_io remove(const std::string& name) const {
        if (!valid_name(name))
            return kit_io::invalid;

        std::lock_guard<std::mutex> lock(m_io_lock);
        document data;
        const auto status = read_file(data);
        if (status != kit_io::ok)
            return status;

        const auto match = find_name(data.kits, name);
        if (match == data.kits.end())
            return kit_io::invalid;
        data.kits.erase(match);
        return write_file(data);
    }

    kit_io backup(std::string& contents) const {
        std::lock_guard<std::mutex> lock(m_io_lock);
        document data;
        const auto status = read_file(data);
        if (status == kit_io::ok)
            contents = encode(data).dump(2);
        return status;
    }

    kit_io import_backup(const std::string& contents, size_t& added) const {
        added = 0;
        if (contents.size() > max_file_size)
            return kit_io::invalid;
        const auto parsed = json::parse(contents, nullptr, false);
        document backup;
        if (!decode_document(parsed, backup))
            return kit_io::invalid;

        std::lock_guard<std::mutex> lock(m_io_lock);
        document current;
        const auto status = read_file(current);
        if (status != kit_io::ok)
            return status;
        size_t count = 0;
        for (auto& kit : backup.kits) {
            if (find_name(current.kits, kit.name) == current.kits.end()) {
                current.kits.push_back(std::move(kit));
                ++count;
            }
        }
        if (count == 0)
            return kit_io::ok;
        const auto write_status = write_file(current);
        if (write_status == kit_io::ok)
            added = count;
        return write_status;
    }

private:
    static constexpr size_t max_file_size = 1024 * 1024;
    inline static std::mutex m_io_lock;
    std::filesystem::path m_path;

    static std::wstring name_as_wide(const std::string& name) {
        const auto size = static_cast<int>(name.size());
        const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), size, nullptr, 0);
        std::wstring wide(count, L'\0');
        if (count)
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), size, wide.data(), count);
        return wide;
    }

    static bool valid_name(const std::string& name) {
        if (name.empty() || name.size() > static_cast<size_t>((std::numeric_limits<int>::max)())
            || std::isspace(static_cast<unsigned char>(name.front()))
            || std::isspace(static_cast<unsigned char>(name.back())))
            return false;

        return std::none_of(name.begin(), name.end(), [](unsigned char ch) { return ch < 32 || ch == 127; })
            && !name_as_wide(name).empty();
    }

    static std::vector<saved_kit>::iterator find_name(std::vector<saved_kit>& kits, const std::string& name) {
        const auto wide_name = name_as_wide(name);
        return std::find_if(kits.begin(), kits.end(), [&](const saved_kit& kit) {
            const auto candidate = name_as_wide(kit.name);
            return CompareStringOrdinal(candidate.c_str(), -1, wide_name.c_str(), -1, TRUE) == CSTR_EQUAL;
        });
    }

    static bool decode_preset(const json& data, cosmetics::Preset& item) {
        const auto& definition = data.at("weapon_definition_id");
        const auto& finish = data.at("paint_kit_id");
        const auto& seed = data.at("seed");
        const auto& wear = data.at("wear");
        if (!definition.is_number_unsigned() || !finish.is_number_unsigned()
            || !seed.is_number_unsigned() || !wear.is_number())
            return false;

        const auto definition_id = definition.get<std::uint64_t>();
        const auto finish_id = finish.get<std::uint64_t>();
        const auto seed_value = seed.get<std::uint64_t>();
        const auto wear_value = wear.get<double>();
        if (definition_id > (std::numeric_limits<std::uint32_t>::max)()
            || finish_id > (std::numeric_limits<std::uint32_t>::max)()
            || seed_value > (std::numeric_limits<std::int32_t>::max)()
            || !(wear_value >= 0.0 && wear_value <= 1.0))
            return false;

        item = { static_cast<std::uint32_t>(definition_id), static_cast<std::uint32_t>(finish_id),
            static_cast<std::int32_t>(seed_value), static_cast<float>(wear_value) };
        return true;
    }

    static bool decode_kit(const json& entry, saved_kit& kit) {
        try {
            kit.name = entry.at("name").get<std::string>();
            const auto& items = entry.at("items");
            if (!items.is_array())
                return false;
            for (const auto& value : items) {
                cosmetics::Preset item{};
                if (!decode_preset(value, item))
                    return false;
                kit.items.push_back(item);
            }
            return valid(kit);
        } catch (const json::exception&) {
            return false;
        }
    }

    static bool decode_document(const json& data, document& output) {
        if (!data.is_object() || data.value("version", json{}) != 1)
            return false;
        const auto entries = data.find("kits");
        if (entries == data.end() || !entries->is_array())
            return false;
        for (const auto& entry : *entries) {
            saved_kit kit;
            if (decode_kit(entry, kit) && find_name(output.kits, kit.name) == output.kits.end())
                output.kits.push_back(std::move(kit));
            else
                output.rejected.push_back(entry);
        }
        return true;
    }

    kit_io read_file(document& output) const {
        std::error_code error;
        const auto size = std::filesystem::file_size(m_path, error);
        if (error)
            return error == std::errc::no_such_file_or_directory ? kit_io::ok : kit_io::io_error;
        if (size > max_file_size)
            return kit_io::invalid;

        std::ifstream file(m_path, std::ios::binary);
        if (!file)
            return kit_io::io_error;
        const auto data = json::parse(file, nullptr, false);
        if (file.bad())
            return kit_io::io_error;
        return decode_document(data, output) ? kit_io::ok : kit_io::invalid;
    }

    static json encode(const document& data) {
        json list = json::array();
        for (const auto& kit : data.kits) {
            json items = json::array();
            for (const auto& item : kit.items) {
                items.push_back({ { "weapon_definition_id", item.weapon_definition_id },
                    { "paint_kit_id", item.paint_kit_id }, { "seed", item.seed }, { "wear", item.wear } });
            }
            list.push_back({ { "name", kit.name }, { "items", std::move(items) } });
        }
        for (const auto& entry : data.rejected)
            list.push_back(entry);
        return { { "version", 1 }, { "kits", std::move(list) } };
    }

    kit_io write_file(const document& data) const {
        const auto contents = encode(data).dump(2);
        if (contents.size() + 1 > max_file_size)
            return kit_io::invalid;

        std::error_code error;
        if (m_path.has_parent_path()) {
            std::filesystem::create_directories(m_path.parent_path(), error);
            if (error)
                return kit_io::io_error;
        }

        auto temporary = m_path;
        temporary += L".tmp";
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file << contents << '\n';
        file.close();
        if (file && MoveFileExW(temporary.c_str(), m_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return kit_io::ok;

        std::filesystem::remove(temporary, error);
        return kit_io::io_error;
    }
};

}  // namespace acervo
