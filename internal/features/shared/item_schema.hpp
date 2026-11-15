#pragma once

#include "../../main.hpp"
#include "../../valve/classes/game_enums.hpp"
#include "../../valve/classes/c_cs_player_pawn.hpp"
#include "../../valve/interfaces/vtables/i_econ_item_system.hpp"
#include <vector>
#include <string>
#include <unordered_map>

struct item_info_t {
	uint16_t definition_index;
	std::string name;
	const char* model_path;
};

struct paint_kit_info_t {
	int id;
	std::string name;
};

class c_item_schema {
public:
	std::vector<item_info_t> knives;
	std::vector<item_info_t> gloves;
	std::vector<item_info_t> weapons;

	std::vector<paint_kit_info_t> all_paint_kits;

	std::unordered_map<uint16_t, std::vector<paint_kit_info_t>> item_paint_kits;
	std::unordered_map<uint16_t, std::vector<const char*>> item_paint_kit_names;

	std::vector<const char*> knife_names_cstr;
	std::vector<const char*> glove_names_cstr;
	std::vector<const char*> weapon_names_cstr;

	void initialize();
	bool is_initialized() const { return m_initialized; }

	const std::vector<const char*>& get_paint_kit_names_for_item(uint16_t def_index) const {
		static std::vector<const char*> empty = { "Default" };
		auto it = item_paint_kit_names.find(def_index);
		return it != item_paint_kit_names.end() ? it->second : empty;
	}

	int get_paint_kit_id_for_item(uint16_t def_index, int index) const {
		auto it = item_paint_kits.find(def_index);
		if (it != item_paint_kits.end() && index >= 0 && index < (int)it->second.size())
			return it->second[index].id;
		return 0;
	}

	// --- stable-identity mapping ---
	// list positions are presentation-only (built from a map walk, then
	// dedup + sort); engine definition indices and paint-kit ids are stable.
	// configs persist the stable ids and resolve them back through these.

	static uint16_t def_from_index(const std::vector<item_info_t>& v, int idx) {
		return (idx > 0 && idx < (int)v.size()) ? v[idx].definition_index : 0;
	}

	static int index_of_def(const std::vector<item_info_t>& v, uint16_t def) {
		if (def == 0)
			return 0;
		for (int i = 0; i < (int)v.size(); ++i)
			if (v[i].definition_index == def)
				return i;
		return 0;
	}

	int paint_kit_index_for_id(uint16_t def_index, int kit_id) const {
		if (kit_id == 0)
			return 0;
		auto it = item_paint_kits.find(def_index);
		if (it != item_paint_kits.end())
			for (int i = 0; i < (int)it->second.size(); ++i)
				if (it->second[i].id == kit_id)
					return i;
		return 0;
	}

private:
	bool m_initialized = false;
	bool is_paint_kit_for_item(const char* simple_weapon_name, c_paint_kit* paint_kit);
	void build_paint_kits_for_item(uint16_t def_index, c_utl_map<int, c_econ_item_definition*>& items, c_utl_map<int, c_paint_kit*>& paint_kit_map);
};

inline const auto g_item_schema = std::make_unique<c_item_schema>();
