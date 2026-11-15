#include "../internal/menu/acervo_kits.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::filesystem::path path = std::filesystem::path(argv[1]) / "kits.json";
    {
        std::ofstream fixture(path);
        fixture << R"({"version":1,"kits":[{"name":"Valid","items":[{"weapon_definition_id":7,"paint_kit_id":44,"seed":661,"wear":0.03}]},{"name":"Retired","items":[{"weapon_definition_id":9999,"paint_kit_id":44,"seed":661,"wear":0.03}]}]})";
    }

    acervo::kit_store store(path);
    std::vector<acervo::saved_kit> kits;
    std::vector<std::string> incompatible;
    assert(store.read(kits, &incompatible) == acervo::kit_io::ok);
    assert(kits.size() == 1 && kits.front().name == "Valid");
    assert(incompatible == std::vector<std::string>{ "Retired" });

    auto edited = kits.front();
    edited.name = "Renamed";
    edited.items.front().seed = 670;
    assert(store.save(edited, "Valid") == acervo::kit_io::ok);
    assert(store.read(kits, &incompatible) == acervo::kit_io::ok);
    assert(kits.size() == 1 && kits.front().name == "Renamed");
    assert(incompatible == std::vector<std::string>{ "Retired" });
    assert(kits.front().items.front().seed == 670);
    assert(store.save(edited) == acervo::kit_io::conflict);

    std::string backup;
    assert(store.backup(backup) == acervo::kit_io::ok);
    size_t added = 0;
    const auto imported_path = path.parent_path() / "imported-kits.json";
    acervo::kit_store imported(imported_path);
    assert(imported.import_backup(backup, added) == acervo::kit_io::ok && added == 1);
    assert(imported.read(kits) == acervo::kit_io::ok && kits.size() == 1);
    assert(imported.import_backup(backup, added) == acervo::kit_io::ok && added == 0);

    std::cout << "14 kit storage checks passed\n";
}
