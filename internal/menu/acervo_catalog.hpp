#pragma once

#include "../../src/preset.hpp"

namespace acervo {

enum class category { rifle, pistol, knife, glove };

struct catalog_entry {
    const char* weapon;
    const char* name;
    category group;
    cosmetics::Finish finish;
};

inline constexpr catalog_entry catalog[] = {
#include "acervo_finishes.inc"
};

struct pattern_preset {
    const char* label;
    cosmetics::Preset preset;
};

// AK-47 Case Hardened | Blue Gem by korenevskiy and Krieke.
// https://steamcommunity.com/sharedfiles/filedetails/?id=2941982575
inline constexpr pattern_preset pattern_presets[] = {
    { "Blue Gem #661 (Scar)", { 7, 44, 661, 0.03f } },
    { "Blue Gem #670", { 7, 44, 670, 0.03f } },
    { "Blue Gem #321", { 7, 44, 321, 0.03f } },
    { "Blue Gem #955", { 7, 44, 955, 0.03f } },
    // Model-specific Case Hardened patterns, documented in Steam guide 1889871377.
    { "Blue Gem #555", { 500, 44, 555, 0.03f } },
    { "Blue Gem #403", { 503, 44, 403, 0.03f } },
    { "Blue Gem #670", { 505, 44, 670, 0.03f } },
    { "Blue Gem #567", { 506, 44, 567, 0.03f } },
    { "Blue Gem #387", { 507, 44, 387, 0.03f } },
    { "Blue Gem #601", { 508, 44, 601, 0.03f } },
    { "Blue Gem #618", { 509, 44, 618, 0.03f } },
    { "Blue Gem #494", { 512, 44, 494, 0.03f } },
    { "Blue Gem #182", { 514, 44, 182, 0.03f } },
    { "Blue Gem #182", { 515, 44, 182, 0.03f } },
    { "Blue Gem #56", { 516, 44, 56, 0.03f } },
    { "Blue Gem #403", { 517, 44, 403, 0.03f } },
    { "Blue Gem #403", { 518, 44, 403, 0.03f } },
    { "Blue Gem #494", { 519, 44, 494, 0.03f } },
    { "Blue Gem #398", { 520, 44, 398, 0.03f } },
    { "Blue Gem #577", { 521, 44, 577, 0.03f } },
    { "Blue Gem #182", { 522, 44, 182, 0.03f } },
    { "Blue Gem #55", { 523, 44, 55, 0.03f } },
    { "Blue Gem #403", { 525, 44, 403, 0.03f } },
    { "Blue Gem #494", { 526, 44, 494, 0.03f } },
    // Butterfly Doppler pattern guides 3260663518 and 3246704692.
    { "Pink Galaxy #339", { 515, 618, 339, 0.01f } },
    { "Pink Galaxy #681", { 515, 618, 681, 0.01f } },
    { "Max Blue #763", { 515, 421, 763, 0.01f } },
    { "Max Blue #575", { 515, 421, 575, 0.01f } },
};

}  // namespace acervo
