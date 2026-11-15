#include "item_schema.hpp"
#include <algorithm>
#include <cctype>
#include <unordered_set>
#include "../../valve/interfaces/interfaces.hpp"
#include "../../valve/interfaces/vtables/i_localize.hpp"
#include "../../valve/classes/c_cs_player_pawn.hpp"
#include "../../menu/acervo_catalog.hpp"

bool c_item_schema::is_paint_kit_for_item(const char* simple_weapon_name, c_paint_kit* paint_kit) {
	if (!simple_weapon_name || !paint_kit || !paint_kit->m_name)
		return false;

	std::string path = "panorama/images/econ/default_generated/" +
		std::string(simple_weapon_name) + "_" +
		paint_kit->m_name + "_light_png.vtex_c";

	return g_interfaces->m_file_system->exists(path.c_str(), "GAME");
}

void c_item_schema::build_paint_kits_for_item(uint16_t def_index, c_utl_map<int, c_econ_item_definition*>& items, c_utl_map<int, c_paint_kit*>& paint_kit_map) {
	c_econ_item_definition* item_def = nullptr;
	const int items_n = items.count();
	for (int i = 0; i < items_n; i++) {
		auto& node = items.element(i);
		if (node.m_value && node.m_value->m_definition_index == def_index) {
			item_def = node.m_value;
			break;
		}
	}

	if (!item_def)
		return;

	const char* simple_name = item_def->get_item_name();
	auto& kits = item_paint_kits[def_index];
	kits.push_back({ 0, "Default" });

	// Curated pairs are checked against the shipped inventory assets. Resolve
	// their IDs directly in the engine tree without depending on a guessed name.
	for (const auto& entry : acervo::catalog) {
		if (entry.finish.weapon_definition_id != def_index)
			continue;
		const int id = static_cast<int>(entry.finish.paint_kit_id);
		if (paint_kit_map.find_by_key(id))
			kits.push_back({ id, entry.name });
	}
	if (!simple_name || simple_name[0] == '\0')
		return;

	const int kits_n = paint_kit_map.count();
	for (int i = 0; i < kits_n; i++) {
		auto& node = paint_kit_map.element(i);
		if (node.m_left == i || !node.m_value)
			continue;

		c_paint_kit* kit = node.m_value;
		if (!kit->m_name || kit->m_id <= 0)
			continue;

		if (is_paint_kit_for_item(simple_name, kit)) {

			const char* display = localize::find_safe(kit->m_description_tag);
			if (!display || !*display)
				display = kit->m_name;
			kits.push_back({ kit->m_id, display });
		}
	}
}

void c_item_schema::initialize() {
	if (m_initialized)
		return;

	auto* item_schema = g_interfaces->get_econ_item_schema();
	if (!item_schema)
		return;

	auto& items = item_schema->get_sorted_item_definition_map();
	auto& paint_kit_map = item_schema->get_paint_kits();

	// start clean so a retry (econ schema not ready at inject) can't accumulate
	knives.clear();
	gloves.clear();
	weapons.clear();
	all_paint_kits.clear();
	item_paint_kits.clear();
	item_paint_kit_names.clear();
	knife_names_cstr.clear();
	glove_names_cstr.clear();
	weapon_names_cstr.clear();

	const int items_n = items.count();
	for (int i = 0; i < items_n; i++) {
		auto& node = items.element(i);
		if (!node.m_value || !node.m_value->m_item_type_name)
			continue;

		c_econ_item_definition* item_def = node.m_value;

		std::string item_name;
		if (item_def->m_item_base_name && item_def->m_item_base_name[0] != '\0') {
			const char* localized = localize::find_safe(item_def->m_item_base_name);
			item_name = (localized && *localized) ? localized : item_def->m_item_base_name;
		} else {
			item_name = "Item " + std::to_string(item_def->m_definition_index);
		}

		const char* model_path = item_def->get_model_name();

		if (item_def->is_knife(false)) {
			knives.push_back({ item_def->m_definition_index, item_name, model_path });
		}
		else if (item_def->is_glove(false)) {
			gloves.push_back({ item_def->m_definition_index, item_name, model_path });
		}
		else {
			const char* type_name = item_def->m_item_type_name;
			bool is_weapon = strstr(type_name, "Pistol") || strstr(type_name, "Rifle") ||
			                 strstr(type_name, "SMG") || strstr(type_name, "Sniper") ||
			                 strstr(type_name, "Shotgun") || strstr(type_name, "Machine") ||
			                 strstr(type_name, "SubMachinegun");

			const auto definition = item_def->m_definition_index;
			const auto known_weapon = std::find_if(std::begin(acervo::catalog), std::end(acervo::catalog),
				[definition](const acervo::catalog_entry& entry) { return entry.finish.weapon_definition_id == definition; });
			if ((is_weapon || known_weapon != std::end(acervo::catalog)) && definition > 0 && definition <= 70)
				weapons.push_back({ item_def->m_definition_index, item_name, model_path });
		}
	}

	std::sort(weapons.begin(), weapons.end(), [](const item_info_t& a, const item_info_t& b) {
		return a.definition_index < b.definition_index;
	});

	// presentation cleanup: drop entries whose full display name was already
	// seen (collapses generic "Knife" defaults, repeated Butterfly Knife,
	// duplicate gloves, etc). keeps first occurrence, exact-name match.
	auto dedup_by_name = [](std::vector<item_info_t>& v) {
		std::unordered_set<std::string> seen;
		std::vector<item_info_t> out;
		out.reserve(v.size());
		for (auto& it : v)
			if (seen.insert(it.name).second)
				out.push_back(it);
		v = std::move(out);
	};
	dedup_by_name(knives);
	dedup_by_name(gloves);
	dedup_by_name(weapons);

	// presentation order: Default T Gloves, Default CT Gloves, then the rest
	{
		auto to_lower = [](std::string s) {
			std::transform(s.begin(), s.end(), s.begin(),
				[](unsigned char c) { return (char)std::tolower(c); });
			return s;
		};
		auto rank = [&](const item_info_t& g) -> int {
			std::string n = to_lower(g.name);
			bool is_default = n.find("default") != std::string::npos;
			if (!is_default)
				return 2;
			return (n.find("ct") != std::string::npos) ? 1 : 0;
		};
		std::stable_sort(gloves.begin(), gloves.end(),
			[&](const item_info_t& a, const item_info_t& b) { return rank(a) < rank(b); });
	}

	// nothing came back from the econ schema yet (inject-time race): bail
	// without marking ready so the caller retries next frame. checked here,
	// before the sentinels below would make knives/gloves permanently
	// non-empty and defeat the retry.
	if (knives.empty() && gloves.empty() && weapons.empty())
		return;

	// canonical "(None)" sentinel at list index 0 for knives & gloves. the
	// entire pipeline already assumes this row exists: glove_changer /
	// knife_changer treat m_*==0 as "no override", the build loops below
	// skip definition_index 0, def_from_index()/index_of_def() use slot 0
	// as the safe fallback, and the ready-log subtracts it. without it list
	// row 0 was a real item the runtime silently dropped and every
	// stable-id lookup was off by one. Weapon lists have no sentinel.
	knives.insert(knives.begin(), { 0, "(None)", nullptr });
	gloves.insert(gloves.begin(), { 0, "(None)", nullptr });

	for (auto& knife : knives)
		if (knife.definition_index != 0)
			build_paint_kits_for_item(knife.definition_index, items, paint_kit_map);

	for (auto& glove : gloves)
		if (glove.definition_index != 0)
			build_paint_kits_for_item(glove.definition_index, items, paint_kit_map);

	for (auto& weapon : weapons)
		if (weapon.definition_index != 0)
			build_paint_kits_for_item(weapon.definition_index, items, paint_kit_map);

	for (auto& knife : knives)
		knife_names_cstr.push_back(knife.name.c_str());
	for (auto& glove : gloves)
		glove_names_cstr.push_back(glove.name.c_str());
	for (auto& weapon : weapons)
		weapon_names_cstr.push_back(weapon.name.c_str());

	// Keep differently numbered phases of finishes such as Doppler.
	for (auto& [def_index, kits] : item_paint_kits) {
		std::unordered_set<int> seen;
		std::vector<paint_kit_info_t> out;
		out.reserve(kits.size());
		for (auto& kit : kits)
			if (seen.insert(kit.id).second)
				out.push_back(kit);
		kits = std::move(out);
	}

	for (auto& [def_index, kits] : item_paint_kits)
		for (auto& kit : kits)
			item_paint_kit_names[def_index].push_back(kit.name.c_str());

	m_initialized = true;
	LOG_INFO(xorstr_("[item_schema] %d knives, %d gloves, %d weapons"),
		(int)knives.size() - 1, (int)gloves.size() - 1, (int)weapons.size());
}
