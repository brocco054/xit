#include "../internal/sdk/config_system/profile_codec.hpp"
#include "../internal/sdk/config_system/profile_files.hpp"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace {
struct item { uint16_t definition_index; };
struct paint_kit { int id; };
struct test_schema {
    std::vector<item> weapons{ {7}, {4} };
    std::vector<item> knives{ {0}, {500}, {507} };
    std::vector<item> gloves{ {0}, {5027}, {5030} };
    std::unordered_map<uint16_t, std::vector<paint_kit>> item_paint_kits{
        { 7, { {0}, {724}, {44} } }, { 4, { {0}, {437} } },
        { 507, { {0}, {569}, {568} } }, { 5030, { {0}, {10037} } }
    };
};

bool approximately(float actual, float expected) {
    return std::abs(actual - expected) < 0.00001f;
}
}

int main(int argc, char** argv) {
    using namespace acervo::profile;
    assert(argc == 2 || argc == 3);
    const test_schema schema;
    c_config config;
    std::ifstream exported(argv[1], std::ios::binary);
    assert(exported);
    assert(decode(exported, schema, config));
    assert(config.skin_changer.m_enabled && config.skin_changer.m_selected_weapon == 0);
    const auto& ak = config.skin_changer.weapon_skins[7];
    assert(ak.paint_kit == 2 && ak.seed == 857 && approximately(ak.wear, 0.24f));
    const auto& knife = config.knife_changer;
    assert(knife.m_enabled && knife.m_knife == 2 && knife.m_paint_kit == 2);
    assert(knife.m_seed == 42 && approximately(knife.m_wear, 0.02f));
    const auto& glove = config.glove_changer;
    assert(glove.m_enabled && glove.m_glove == 2 && glove.m_paint_kit == 1);
    assert(glove.m_seed == 19 && approximately(glove.m_wear, 0.18f));

    const auto stable = encode(config, schema);
    assert(stable["skin_changer"]["selected_weapon_id"] == 7);
    assert(stable["skin_changer"]["weapon_7_paint_kit_id"] == 44);
    assert(stable["knife_changer"]["paint_kit_id"] == 568);
    assert(stable["glove_changer"]["paint_kit_id"] == 10037);

    auto read = [&](const char* input) {
        std::istringstream stream(input);
        return decode(stream, schema, config);
    };
    const auto rejects = [&](const char* input, status failure) {
        assert(read(input).code == failure);
        assert(encode(config, schema) == stable);
    };
    rejects(R"({"skin_changer":{"enabled":false},"knife_changer":{"seed":"42"}})", status::invalid_data);
    rejects(R"({"skin_changer":{"selected_weapon":-2}})", status::invalid_data);
    rejects(R"({"skin_changer":{"selected_weapon":2}})", status::unavailable);
    rejects(R"({"skin_changer":{"selected_weapon_id":65536}})", status::invalid_data);
    rejects(R"({"skin_changer":{"selected_weapon_id":999}})", status::unavailable);
    rejects(R"({"skin_changer":{"weapon_7_paint_kit_id":568}})", status::unavailable);
    rejects(R"({"skin_changer":{"weapon_7_wear":1.01}})", status::invalid_data);
    rejects(R"({"skin_changer":{"weapon_7_seed":1001}})", status::invalid_data);
    rejects(R"({"skin_changer":{"weapon_7_seed":0.5}})", status::invalid_data);
    rejects(R"({"skin_changer":{"weapon_7_seed":18446744073709551615}})", status::invalid_data);
    rejects(R"({"skin_changer":[]})", status::invalid_data);
    rejects(R"([)", status::invalid_data);

    assert(read(R"({"skin_changer":{"selected_weapon":1,"weapon_7_paint_kit":1},
                      "knife_changer":{"knife":1,"paint_kit":1,"seed":0},
                      "glove_changer":{"glove":1,"paint_kit":1}})"));
    assert(config.skin_changer.m_selected_weapon == 1 && ak.paint_kit == 1);
    assert(knife.m_knife == 2 && knife.m_paint_kit == 1 && knife.m_seed == 0);
    assert(glove.m_glove == 2 && glove.m_paint_kit == 1);

    // The old native writer emitted stable ID zero when the first weapon was selected.
    assert(read(R"({"skin_changer":{"selected_weapon_id":0,"selected_weapon":0}})"));
    assert(config.skin_changer.m_selected_weapon == 0);

    assert(read(R"({"skin_changer":{"selected_weapon_id":0,"weapon_7_paint_kit_id":0}})"));
    assert(config.skin_changer.m_selected_weapon == -1 && ak.paint_kit == 0);
    const auto empty_selection = encode(config, schema);
    assert(empty_selection["skin_changer"]["selected_weapon"] == -1);
    std::istringstream restored_selection(empty_selection.dump());
    config.skin_changer.m_selected_weapon = 0;
    assert(decode(restored_selection, schema, config));
    assert(config.skin_changer.m_selected_weapon == -1);

    if (argc == 3) {
        std::ifstream example(argv[2], std::ios::binary);
        assert(decode(example, schema, config));
        assert(ak.paint_kit == 2 && ak.seed == 857 && approximately(ak.wear, 0.24f));
        assert(!knife.m_enabled && !glove.m_enabled);
    }

    auto directory = std::filesystem::path(argv[1]);
    directory += L".d";
    files profiles(directory);
    assert(profiles.write("../escape", "{}", false).code == status::invalid_name);
    assert(profiles.write("NUL.name", "{}", false).code == status::invalid_name);
    assert(profiles.write("x:stream", "{}", false).code == status::invalid_name);
    assert(profiles.write("Acervo", stable.dump(), false));
    assert(profiles.write("acervo", "{}", false).code == status::conflict);
    assert(profiles.write("Ausente", "{}", true).code == status::not_found);
    std::string contents;
    assert(profiles.read("Acervo", contents) && contents == stable.dump());
    assert(profiles.rename("Acervo", "../escape").code == status::invalid_name);
    assert(profiles.rename("Acervo", "Reserva"));
    assert(profiles.write("Reserva", "{}", true));
    assert(profiles.read("Reserva", contents) && contents == "{}");
    std::vector<std::string> names;
    assert(profiles.list(names) && names == std::vector<std::string>{ "Reserva" });
    assert(profiles.remove("Reserva"));
    assert(profiles.read("Reserva", contents).code == status::not_found);
    assert(std::filesystem::remove(directory));
    std::cout << "Web-to-C++ profile, legacy, rejection and file checks passed\n";
}
