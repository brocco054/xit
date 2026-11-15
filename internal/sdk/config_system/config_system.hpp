#pragma once

#include "profile_files.hpp"
#include <memory>
#include <string>
#include <vector>

class c_config_system {
public:
    void setup_values();
    acervo::profile::result save(const std::string& name, bool overwrite);
    acervo::profile::result load(const std::string& name);
    acervo::profile::result remove(const std::string& name);
    acervo::profile::result rename(const std::string& from, const std::string& to);
    void reset();
    acervo::profile::result refresh();

    void auto_load();
    void auto_save();

    const std::vector<std::string>& get_config_files() const { return m_config_files; }

    std::string m_selected_config;

private:
    acervo::profile::files m_files{ LR"(C:\vc\configs)" };
    std::vector<std::string> m_config_files;
    static constexpr const char* default_config = "default";
};

inline const auto g_config_system = std::make_unique<c_config_system>();
