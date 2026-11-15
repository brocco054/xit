#pragma once

#include "../../config.hpp"
#include "../includes/json.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <istream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace acervo::profile {

enum class status { ok, invalid_name, not_found, conflict, io_error, invalid_data, unavailable };

struct result {
    status code = status::ok;
    std::string field;
    explicit operator bool() const noexcept { return code == status::ok; }
};

using json = nlohmann::json;

namespace detail {

class section {
public:
    section(const json& document, std::string_view name) : name_(name) {
        const auto entry = document.find(name);
        if (entry != document.end()) {
            if (!entry->is_object())
                throw result{ status::invalid_data, std::string(name) };
            fields_ = &*entry;
        }
    }

    std::optional<int> integer(std::string_view key, int min = 0,
        int max = (std::numeric_limits<int>::max)()) const {
        const auto* value = find(key);
        if (!value)
            return {};
        if (!value->is_number_integer())
            fail(key);
        if (value->is_number_unsigned()) {
            const auto number = value->get<std::uint64_t>();
            if (number > static_cast<std::uint64_t>(max) || (min > 0 && number < static_cast<std::uint64_t>(min)))
                fail(key);
        } else {
            const auto number = value->get<std::int64_t>();
            if (number < min || number > max)
                fail(key);
        }
        return value->get<int>();
    }

    void boolean(std::string_view key, bool& destination) const {
        if (const auto* value = find(key)) {
            if (!value->is_boolean())
                fail(key);
            destination = value->get<bool>();
        }
    }

    void wear(std::string_view key, float& destination) const {
        if (const auto* value = find(key)) {
            if (!value->is_number())
                fail(key);
            const auto number = value->get<double>();
            if (!std::isfinite(number) || number < 0.0 || number > 1.0)
                fail(key);
            destination = static_cast<float>(number);
        }
    }

    template<std::size_t size>
    void name(std::string_view key, char (&destination)[size]) const {
        if (const auto* value = find(key)) {
            if (!value->is_string())
                fail(key);
            const auto& text = value->get_ref<const std::string&>();
            if (text.size() >= size || text.find('\0') != std::string::npos)
                fail(key);
            std::memset(destination, 0, size);
            std::memcpy(destination, text.data(), text.size());
        }
    }

    [[noreturn]] void fail(std::string_view key, status code = status::invalid_data) const {
        throw result{ code, std::string(name_) + "." + std::string(key) };
    }

private:
    std::string_view name_;
    const json* fields_ = nullptr;

    const json* find(std::string_view key) const {
        if (!fields_)
            return nullptr;
        const auto value = fields_->find(key);
        return value == fields_->end() ? nullptr : &*value;
    }
};

template<typename Entries, typename Predicate>
int index_of(const Entries& items, Predicate matches) {
    const auto found = std::find_if(items.begin(), items.end(), matches);
    return found == items.end() ? -1 : static_cast<int>(found - items.begin());
}

template<typename Entries>
uint16_t definition_for(const Entries& items, int index) {
    return index >= 0 && static_cast<std::size_t>(index) < items.size()
        ? items[index].definition_index : 0;
}

template<typename Entries>
int definition_index(const Entries& items, int id, int empty_index, const section& fields,
    std::string_view key) {
    if (!id)
        return empty_index;
    const int index = index_of(items, [id](const auto& item) { return item.definition_index == id; });
    if (index < 0)
        fields.fail(key, status::unavailable);
    return index;
}

template<typename Schema>
int paint_id(const Schema& schema, uint16_t definition, int index) {
    const auto slot = schema.item_paint_kits.find(definition);
    if (slot != schema.item_paint_kits.end() && index >= 0
        && static_cast<std::size_t>(index) < slot->second.size())
        return slot->second[index].id;
    return 0;
}

template<typename Schema>
int paint_index(const Schema& schema, uint16_t definition, const section& fields,
    const std::string& key, int value, bool stable) {
    if (!value)
        return 0;
    const auto slot = schema.item_paint_kits.find(definition);
    if (slot == schema.item_paint_kits.end())
        fields.fail(key, status::unavailable);
    if (stable) {
        const int index = index_of(slot->second, [value](const auto& kit) { return kit.id == value; });
        if (index > 0)
            return index;
    } else if (static_cast<std::size_t>(value) < slot->second.size()) {
        return value;
    }
    fields.fail(key, status::unavailable);
}

template<typename Schema, typename Entries, typename Config>
void decode_accessory(const Schema& schema, const Entries& entries, Config& state, int& index,
    const section& fields, const std::string& index_key) {
    fields.boolean("enabled", state.m_enabled);
    fields.wear("wear", state.m_wear);
    state.m_seed = fields.integer("seed", 0, 1000).value_or(state.m_seed);

    const auto index_id_key = index_key + "_id";
    const auto stable_id = fields.integer(index_id_key, 0, UINT16_MAX);
    const auto legacy_index = fields.integer(index_key);
    if (stable_id) {
        index = definition_index(entries, *stable_id, 0, fields, index_id_key);
    } else if (legacy_index) {
        // The oldest files used pre-sentinel positions, with zero meaning off.
        if (*legacy_index && static_cast<std::size_t>(*legacy_index) + 1 >= entries.size())
            fields.fail(index_key, status::unavailable);
        index = *legacy_index ? *legacy_index + 1 : 0;
    }

    const auto stable_kit = fields.integer("paint_kit_id");
    const auto legacy_kit = fields.integer("paint_kit");
    const auto definition = definition_for(entries, index);
    if (stable_kit) {
        state.m_paint_kit = paint_index(schema, definition, fields, "paint_kit_id", *stable_kit, true);
    } else if (stable_id) {
        state.m_paint_kit = 0;
    } else if (legacy_kit) {
        state.m_paint_kit = paint_index(schema, definition, fields, "paint_kit", *legacy_kit, false);
    } else if (legacy_index) {
        state.m_paint_kit = 0;
    }
}

template<typename Schema>
void decode_weapons(const Schema& schema, c_config::skin_changer_t& state, const section& fields) {
    fields.boolean("enabled", state.m_enabled);
    const auto stable_id = fields.integer("selected_weapon_id", 0, UINT16_MAX);
    const auto legacy_index = fields.integer("selected_weapon", -1);
    if (stable_id) {
        // Older native saves emitted ID zero for the real first weapon.
        const bool legacy_first_weapon = *stable_id == 0 && legacy_index == 0 && !schema.weapons.empty();
        state.m_selected_weapon = legacy_first_weapon ? 0
            : definition_index(schema.weapons, *stable_id, -1, fields, "selected_weapon_id");
    } else if (legacy_index) {
        if (*legacy_index >= 0 && static_cast<std::size_t>(*legacy_index) >= schema.weapons.size())
            fields.fail("selected_weapon", status::unavailable);
        state.m_selected_weapon = *legacy_index;
    }

    for (std::size_t slot = 0; slot < std::size(state.weapon_skins); ++slot) {
        const auto prefix = "weapon_" + std::to_string(slot);
        auto& finish = state.weapon_skins[slot];
        const auto stable_key = prefix + "_paint_kit_id";
        const auto legacy_key = prefix + "_paint_kit";
        const auto stable_kit = fields.integer(stable_key);
        const auto legacy_kit = fields.integer(legacy_key);
        const auto definition = static_cast<uint16_t>(c_config::skin_changer_t::get_config_index(static_cast<int>(slot)));
        if (stable_kit) {
            finish.paint_kit = paint_index(schema, definition, fields, stable_key, *stable_kit, true);
        } else if (legacy_kit) {
            finish.paint_kit = paint_index(schema, definition, fields, legacy_key, *legacy_kit, false);
        }
        finish.seed = fields.integer(prefix + "_seed", 0, 1000).value_or(finish.seed);
        fields.wear(prefix + "_wear", finish.wear);
        fields.name(prefix + "_custom_name", finish.custom_name);
    }
}

template<typename Schema, typename Entries, typename Config>
json encode_accessory(const Schema& schema, const Entries& entries, const Config& state,
    int index, const char* index_key, const char* id_key) {
    const auto definition = definition_for(entries, index);
    return {
        { "enabled", state.m_enabled }, { index_key, index }, { id_key, definition },
        { "paint_kit", state.m_paint_kit }, { "paint_kit_id", paint_id(schema, definition, state.m_paint_kit) },
        { "seed", state.m_seed }, { "wear", state.m_wear }
    };
}

} // namespace detail

template<typename Schema>
result decode(std::istream& input, const Schema& schema, c_config& destination) {
    try {
        const auto document = json::parse(input);
        if (!document.is_object())
            return { status::invalid_data, {} };

        c_config staged = destination;
        const detail::section knives(document, "knife_changer");
        const detail::section gloves(document, "glove_changer");
        const detail::section weapons(document, "skin_changer");
        detail::decode_accessory(schema, schema.knives, staged.knife_changer, staged.knife_changer.m_knife, knives, "knife");
        knives.name("custom_name", staged.knife_changer.m_custom_name);
        detail::decode_accessory(schema, schema.gloves, staged.glove_changer, staged.glove_changer.m_glove, gloves, "glove");
        detail::decode_weapons(schema, staged.skin_changer, weapons);
        destination = staged;
        return {};
    } catch (const result& failure) {
        return failure;
    } catch (const json::exception&) {
        return { status::invalid_data, {} };
    }
}

template<typename Schema>
json encode(const c_config& state, const Schema& schema) {
    auto knives = detail::encode_accessory(schema, schema.knives, state.knife_changer,
        state.knife_changer.m_knife, "knife", "knife_id");
    knives["custom_name"] = state.knife_changer.m_custom_name;
    auto gloves = detail::encode_accessory(schema, schema.gloves, state.glove_changer,
        state.glove_changer.m_glove, "glove", "glove_id");

    const auto& changer = state.skin_changer;
    json weapons{
        { "enabled", changer.m_enabled },
        { "selected_weapon", changer.m_selected_weapon },
        { "selected_weapon_id", detail::definition_for(schema.weapons, changer.m_selected_weapon) }
    };
    for (std::size_t slot = 0; slot < std::size(changer.weapon_skins); ++slot) {
        const auto& finish = changer.weapon_skins[slot];
        const auto prefix = "weapon_" + std::to_string(slot);
        weapons[prefix + "_paint_kit"] = finish.paint_kit;
        weapons[prefix + "_seed"] = finish.seed;
        weapons[prefix + "_wear"] = finish.wear;
        weapons[prefix + "_custom_name"] = finish.custom_name;
        if (c_config::skin_changer_t::get_config_index(static_cast<int>(slot)))
            weapons[prefix + "_paint_kit_id"] = detail::paint_id(schema, static_cast<uint16_t>(slot), finish.paint_kit);
    }

    return { { "knife_changer", std::move(knives) }, { "glove_changer", std::move(gloves) },
        { "skin_changer", std::move(weapons) } };
}

} // namespace acervo::profile
