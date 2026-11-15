#include "config_system.hpp"
#include "../../features/skin_changer/skin_changer.hpp"
#include "../../features/glove_changer/glove_changer.hpp"
#include "../../features/shared/item_schema.hpp"
#include "../console/console.hpp"
#include <sstream>

namespace {
using namespace acervo::profile;

void mark_updated() {
    g_skin_changer->should_update = true;
    g_glove_changer->should_update = true;
}

void log_failure(const char* operation, const result& outcome) {
    (void)operation;
    (void)outcome;
    LOG_WARNING("[profiles] %s failed (%d) %s", operation,
        static_cast<int>(outcome.code), outcome.field.c_str());
}
}

void c_config_system::setup_values() {
    const auto ready = m_files.ensure_directory();
    if (!ready)
        log_failure("prepare directory", ready);
}

acervo::profile::result c_config_system::save(const std::string& name, bool overwrite) {
    // Snapshot under the lock; encode and disk I/O (flush + write-through) run
    // outside it so the render-thread hook never waits on the disk.
    c_config snapshot;
    {
        std::lock_guard config_lock(g_config_mutex);
        if (!g_item_schema->is_initialized())
            return { status::unavailable, {} };
        snapshot = *g_cfg;
    }

    std::string contents;
    try {
        contents = encode(snapshot, *g_item_schema).dump(2);
    } catch (const json::exception&) {
        return { status::invalid_data, {} };
    }
    c_config checked;
    std::istringstream input(contents);
    if (const auto valid = decode(input, *g_item_schema, checked); !valid)
        return valid;

    contents += '\n';
    const auto saved = m_files.write(name, contents, overwrite);
    if (saved)
        refresh();
    return saved;
}

acervo::profile::result c_config_system::load(const std::string& name) {
    std::string contents;
    if (const auto opened = m_files.read(name, contents); !opened)
        return opened;
    std::lock_guard config_lock(g_config_mutex);
    if (!g_item_schema->is_initialized())
        return { status::unavailable, {} };
    std::istringstream input(contents);
    const auto loaded = decode(input, *g_item_schema, *g_cfg);
    if (loaded)
        mark_updated();
    return loaded;
}

acervo::profile::result c_config_system::remove(const std::string& name) {
    const auto removed = m_files.remove(name);
    if (removed)
        refresh();
    return removed;
}

acervo::profile::result c_config_system::rename(const std::string& from, const std::string& to) {
    const auto renamed = m_files.rename(from, to);
    if (renamed)
        refresh();
    return renamed;
}

void c_config_system::reset() {
    std::lock_guard config_lock(g_config_mutex);
    *g_cfg = c_config{};
    mark_updated();
}

acervo::profile::result c_config_system::refresh() {
    return m_files.list(m_config_files);
}

void c_config_system::auto_load() {
    const auto loaded = load(default_config);
    if (!loaded && loaded.code != status::not_found)
        log_failure("load default", loaded);
}

void c_config_system::auto_save() {
    auto saved = save(default_config, true);
    if (saved.code == status::not_found)
        saved = save(default_config, false);
    if (!saved)
        log_failure("save default", saved);
}
