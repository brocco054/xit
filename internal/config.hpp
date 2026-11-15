#pragma once

#include <memory>
#include <mutex>

struct knife_config {
    bool m_enabled = false;
    int m_knife = 0;
    int m_paint_kit = 0;
    float m_wear = 0.0001f;
    int m_seed = 0;
    char m_custom_name[161]{};
};

struct glove_config {
    bool m_enabled = false;
    int m_glove = 0;
    int m_paint_kit = 0;
    float m_wear = 0.0001f;
    int m_seed = 0;
};

struct weapon_finish_config {
    int paint_kit = 0;
    float wear = 0.0001f;
    int seed = 0;
    char custom_name[161]{};
};

struct weapon_config {
    bool m_enabled = false;
    int m_selected_weapon = -1;
    weapon_finish_config weapon_skins[100]{};
};

struct c_config {
    struct skin_changer_t : weapon_config {
        static constexpr int get_config_index(int definition) noexcept {
            return definition >= 1 && definition <= 70 ? definition : 0;
        }
    };

    knife_config knife_changer;
    glove_config glove_changer;
    skin_changer_t skin_changer;
};

inline const auto g_cfg = std::make_unique<c_config>();
inline std::recursive_mutex g_config_mutex;
