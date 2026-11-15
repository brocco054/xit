#include "preset.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

void require(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        std::exit(EXIT_FAILURE);
    }
}

}  // namespace

int main() {
    using namespace cosmetics;

    // Fixture IDs and ranges exercise validation independently of the game catalog.
    constexpr Finish finish{7, 100, 1, 1000, 0.06f, 0.80f};
    constexpr Preset valid{7, 100, 42, 0.25f};

    require(is_valid(valid, finish), "compatible preset");
    require(is_valid({7, 100, 1, 0.06f}, finish), "inclusive lower bounds");
    require(is_valid({7, 100, 1000, 0.80f}, finish), "inclusive upper bounds");
    require(!is_valid({8, 100, 42, 0.25f}, finish), "wrong weapon");
    require(!is_valid({7, 101, 42, 0.25f}, finish), "wrong finish");
    require(!is_valid({7, 100, 0, 0.25f}, finish), "seed below range");
    require(!is_valid({7, 100, 1001, 0.25f}, finish), "seed above range");
    require(!is_valid({7, 100, 42, 0.05f}, finish), "wear below finish range");
    require(!is_valid({7, 100, 42, 0.81f}, finish), "wear above finish range");
    require(!is_valid({7, 100, 42, std::numeric_limits<float>::quiet_NaN()}, finish), "NaN wear");
    require(!is_valid({7, 100, 42, std::numeric_limits<float>::infinity()}, finish), "infinite wear");

    std::cout << "11 preset checks passed\n";
}
