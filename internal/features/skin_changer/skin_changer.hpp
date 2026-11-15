#pragma once

#include "../../main.hpp"
#include "../../valve/classes/c_cs_player_pawn.hpp"
#include "../shared/item_schema.hpp"

class c_skin_changer {
public:
	void run(int stage);
	void initialize();

	bool should_update = false;
	bool is_initialized() const { return m_initialized; }

private:
	bool m_initialized = false;
	c_base_handle m_last_active_weapon;
	c_base_entity* m_last_hud_weapon = nullptr;

	c_base_entity* get_hud_weapon(c_base_entity* weapon, c_cs_player_pawn* local_pawn);
	void apply_skin(c_econ_entity* weapon, c_econ_item_view* item, c_base_entity* hud_weapon,
		int paint_kit_id, float wear, int seed);
	void process_weapon(c_econ_entity* weapon, c_econ_item_view* item, c_base_entity* hud_weapon, bool force_update);
	void process_knife(c_econ_entity* weapon, c_econ_item_view* item, c_base_entity* hud_weapon, bool force_update);
};

inline const auto g_skin_changer = std::make_unique<c_skin_changer>();
