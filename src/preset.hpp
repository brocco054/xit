#pragma once

#include <cstdint>

namespace cosmetics {

struct Preset {
    std::uint32_t weapon_definition_id;
    std::uint32_t paint_kit_id;
    std::int32_t seed;
    float wear;
};

struct Finish {
    std::uint32_t weapon_definition_id;
    std::uint32_t paint_kit_id;
    std::int32_t min_seed;
    std::int32_t max_seed;
    float min_wear;
    float max_wear;
};

[[nodiscard]] constexpr bool is_valid(const Preset& preset, const Finish& finish) noexcept {
    return preset.weapon_definition_id == finish.weapon_definition_id
        && preset.paint_kit_id == finish.paint_kit_id
        && preset.seed >= finish.min_seed
        && preset.seed <= finish.max_seed
        && preset.wear >= finish.min_wear
        && preset.wear <= finish.max_wear;
}

}  // namespace cosmetics
