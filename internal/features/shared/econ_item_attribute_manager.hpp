#pragma once

class c_econ_item_view;
class c_cs_player_pawn;

namespace econ_item_attribute_manager {
	void create(c_econ_item_view* item, int paint_kit, float wear, int seed);
	void remove(c_econ_item_view* item);
#ifdef VC_ITEM_OBSERVE
	void observe_active_items(c_cs_player_pawn* pawn);
#endif
}
